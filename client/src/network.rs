use glam::Vec3;
use log::{error, info, warn};
use shared::{ClientMessage, EntityTransform, ServerMessage};
use std::sync::mpsc::{Receiver, Sender, TryRecvError};
use std::sync::{Arc, Mutex};
use tungstenite::{Message, WebSocket, stream::MaybeTlsStream};
use std::net::TcpStream;

const DEFAULT_SERVER_ADDR: &str = "ws://127.0.0.1:9770";

/// Handle to the network connection, used from the script thread.
pub struct NetworkClient {
    outgoing_tx: Sender<ClientMessage>,
    incoming_rx: Mutex<Receiver<ServerMessage>>,
    connected: Arc<std::sync::atomic::AtomicBool>,
    player_id: Arc<Mutex<Option<u32>>>,
}

impl NetworkClient {
    /// Spawn a background thread that connects to the server.
    /// Returns immediately with a handle for sending/receiving messages.
    pub fn connect(server_addr: &str, player_name: &str) -> Self {
        let (outgoing_tx, outgoing_rx) = std::sync::mpsc::channel::<ClientMessage>();
        let (incoming_tx, incoming_rx) = std::sync::mpsc::channel::<ServerMessage>();
        let connected = Arc::new(std::sync::atomic::AtomicBool::new(false));
        let player_id = Arc::new(Mutex::new(None));

        let addr = server_addr.to_string();
        let name = player_name.to_string();
        let connected_clone = connected.clone();
        let player_id_clone = player_id.clone();

        std::thread::spawn(move || {
            network_thread(addr, name, outgoing_rx, incoming_tx, connected_clone, player_id_clone);
        });

        NetworkClient {
            outgoing_tx,
            incoming_rx: Mutex::new(incoming_rx),
            connected,
            player_id,
        }
    }

    /// Connect to localhost with default port.
    pub fn connect_localhost(player_name: &str) -> Self {
        Self::connect(DEFAULT_SERVER_ADDR, player_name)
    }

    /// Send the current player position to the server.
    pub fn send_player_update(&self, position: Vec3, rotation: Vec3, velocity: Vec3) {
        if !self.is_connected() {
            return;
        }

        let _ = self.outgoing_tx.send(ClientMessage::PlayerUpdate(EntityTransform {
            position,
            rotation,
            velocity,
        }));
    }

    /// Send a chat message.
    pub fn send_chat(&self, message: &str) {
        if !self.is_connected() {
            return;
        }
        let _ = self.outgoing_tx.send(ClientMessage::ChatMessage(message.to_string()));
    }

    /// Drain all pending server messages. Call this each tick from the script thread.
    pub fn poll_messages(&self) -> Vec<ServerMessage> {
        let rx = self.incoming_rx.lock().unwrap();
        let mut messages = Vec::new();
        loop {
            match rx.try_recv() {
                Ok(msg) => messages.push(msg),
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => {
                    self.connected.store(false, std::sync::atomic::Ordering::Relaxed);
                    break;
                }
            }
        }
        messages
    }

    pub fn is_connected(&self) -> bool {
        self.connected.load(std::sync::atomic::Ordering::Relaxed)
    }

    pub fn player_id(&self) -> Option<u32> {
        *self.player_id.lock().unwrap()
    }
}

fn network_thread(
    addr: String,
    player_name: String,
    outgoing_rx: Receiver<ClientMessage>,
    incoming_tx: Sender<ServerMessage>,
    connected: Arc<std::sync::atomic::AtomicBool>,
    player_id: Arc<Mutex<Option<u32>>>,
) {
    info!("Connecting to server at {}...", addr);

    let mut socket = match connect_with_retry(&addr, 5) {
        Some(s) => s,
        None => {
            error!("Failed to connect to server at {} after retries", addr);
            return;
        }
    };

    connected.store(true, std::sync::atomic::Ordering::Relaxed);
    info!("Connected to server at {}", addr);

    // Send connect message
    let connect_msg = ClientMessage::Connect { name: player_name };
    if let Err(e) = send_message(&mut socket, &connect_msg) {
        error!("Failed to send connect message: {}", e);
        return;
    }

    // Set socket to non-blocking so we can interleave reads and writes
    if let MaybeTlsStream::Plain(ref tcp) = socket.get_ref() {
        let _ = tcp.set_nonblocking(true);
    }

    loop {
        // Try to read incoming messages (non-blocking)
        match socket.read() {
            Ok(Message::Text(text)) => {
                match serde_json::from_str::<ServerMessage>(&text) {
                    Ok(server_msg) => {
                        // Capture player_id from Welcome
                        if let ServerMessage::Welcome { player_id: pid } = &server_msg {
                            *player_id.lock().unwrap() = Some(*pid);
                            info!("Server assigned player ID: {}", pid);
                        }
                        if incoming_tx.send(server_msg).is_err() {
                            info!("Script thread dropped, closing network");
                            break;
                        }
                    }
                    Err(e) => {
                        warn!("Invalid server message: {}", e);
                    }
                }
            }
            Ok(Message::Close(_)) => {
                info!("Server closed connection");
                break;
            }
            Err(tungstenite::Error::Io(ref e))
                if e.kind() == std::io::ErrorKind::WouldBlock =>
            {
                // No data available, that's fine
            }
            Err(e) => {
                error!("WebSocket read error: {}", e);
                break;
            }
            _ => {}
        }

        // Try to send outgoing messages
        loop {
            match outgoing_rx.try_recv() {
                Ok(msg) => {
                    if let Err(e) = send_message(&mut socket, &msg) {
                        error!("Failed to send message: {}", e);
                        break;
                    }
                }
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => {
                    info!("Outgoing channel closed, shutting down network");
                    let _ = send_message(&mut socket, &ClientMessage::Disconnect);
                    let _ = socket.close(None);
                    connected.store(false, std::sync::atomic::Ordering::Relaxed);
                    return;
                }
            }
        }

        // Small sleep to avoid busy-spinning
        std::thread::sleep(std::time::Duration::from_millis(16));
    }

    connected.store(false, std::sync::atomic::Ordering::Relaxed);
    let _ = socket.close(None);
    info!("Disconnected from server");
}

fn connect_with_retry(
    addr: &str,
    max_attempts: u32,
) -> Option<WebSocket<MaybeTlsStream<TcpStream>>> {
    for attempt in 1..=max_attempts {
        match tungstenite::connect(addr) {
            Ok((socket, _response)) => return Some(socket),
            Err(e) => {
                warn!(
                    "Connection attempt {}/{} failed: {}",
                    attempt, max_attempts, e
                );
                if attempt < max_attempts {
                    std::thread::sleep(std::time::Duration::from_secs(2));
                }
            }
        }
    }
    None
}

fn send_message(
    socket: &mut WebSocket<MaybeTlsStream<TcpStream>>,
    msg: &ClientMessage,
) -> Result<(), tungstenite::Error> {
    let json = serde_json::to_string(msg).map_err(|e| {
        tungstenite::Error::Io(std::io::Error::new(std::io::ErrorKind::Other, e))
    })?;

    // Socket might be in non-blocking mode, temporarily set blocking for writes
    if let MaybeTlsStream::Plain(ref tcp) = socket.get_ref() {
        let _ = tcp.set_nonblocking(false);
    }

    socket.send(Message::Text(json.into()))?;

    if let MaybeTlsStream::Plain(ref tcp) = socket.get_ref() {
        let _ = tcp.set_nonblocking(true);
    }

    Ok(())
}
