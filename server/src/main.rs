use futures_util::{SinkExt, StreamExt};
use log::{error, info, warn};
use shared::{ClientMessage, ServerMessage};
use std::collections::HashMap;
use std::net::SocketAddr;
use std::sync::Arc;
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::{Mutex, broadcast};
use tokio_tungstenite::tungstenite::Message;
use std::sync::atomic::{AtomicU32, Ordering};

type PeerMap = Arc<Mutex<HashMap<SocketAddr, PlayerState>>>;

struct PlayerState {
    player_id: u32,
    name: String,
}

static NEXT_PLAYER_ID: AtomicU32 = AtomicU32::new(1);

#[tokio::main]
async fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    let addr = "0.0.0.0:9770";
    let listener = TcpListener::bind(addr).await.expect("Failed to bind");

    info!("Phantom server listening on {}", addr);

    let peers: PeerMap = Arc::new(Mutex::new(HashMap::new()));
    let (broadcast_tx, _) = broadcast::channel::<(SocketAddr, String)>(256);

    while let Ok((stream, addr)) = listener.accept().await {
        let peers = peers.clone();
        let broadcast_tx = broadcast_tx.clone();
        let broadcast_rx = broadcast_tx.subscribe();

        tokio::spawn(handle_connection(stream, addr, peers, broadcast_tx, broadcast_rx));
    }
}

async fn handle_connection(
    stream: TcpStream,
    addr: SocketAddr,
    peers: PeerMap,
    broadcast_tx: broadcast::Sender<(SocketAddr, String)>,
    mut broadcast_rx: broadcast::Receiver<(SocketAddr, String)>,
) {
    let ws_stream = match tokio_tungstenite::accept_async(stream).await {
        Ok(ws) => ws,
        Err(e) => {
            error!("WebSocket handshake failed for {}: {}", addr, e);
            return;
        }
    };

    info!("New WebSocket connection from {}", addr);

    let player_id = NEXT_PLAYER_ID.fetch_add(1, Ordering::Relaxed);

    let (mut ws_sender, mut ws_receiver) = ws_stream.split();

    // Send welcome message
    let welcome = ServerMessage::Welcome { player_id };
    if let Ok(json) = serde_json::to_string(&welcome) {
        let _ = ws_sender.send(Message::Text(json.into())).await;
    }

    // Handle incoming messages and broadcast forwarding concurrently
    let peers_clone = peers.clone();
    let broadcast_tx_clone = broadcast_tx.clone();

    let receive_task = tokio::spawn(async move {
        while let Some(msg) = ws_receiver.next().await {
            match msg {
                Ok(Message::Text(text)) => {
                    match serde_json::from_str::<ClientMessage>(&text) {
                        Ok(client_msg) => {
                            handle_client_message(
                                &client_msg,
                                addr,
                                player_id,
                                &peers_clone,
                                &broadcast_tx_clone,
                            )
                            .await;
                        }
                        Err(e) => {
                            warn!("Invalid message from {}: {}", addr, e);
                        }
                    }
                }
                Ok(Message::Close(_)) => break,
                Err(e) => {
                    error!("Error receiving from {}: {}", addr, e);
                    break;
                }
                _ => {}
            }
        }
    });

    let send_task = tokio::spawn(async move {
        while let Ok((sender_addr, msg)) = broadcast_rx.recv().await {
            if sender_addr != addr {
                if ws_sender.send(Message::Text(msg.into())).await.is_err() {
                    break;
                }
            }
        }
    });

    let _ = receive_task.await;
    send_task.abort();

    // Clean up on disconnect
    let mut peers_lock = peers.lock().await;
    if let Some(player) = peers_lock.remove(&addr) {
        info!("Player {} ({}) disconnected", player.name, player.player_id);
        let disconnect_msg = ServerMessage::PlayerDisconnected {
            player_id: player.player_id,
        };
        if let Ok(json) = serde_json::to_string(&disconnect_msg) {
            let _ = broadcast_tx.send((addr, json));
        }
    }
}

async fn handle_client_message(
    msg: &ClientMessage,
    addr: SocketAddr,
    player_id: u32,
    peers: &PeerMap,
    broadcast_tx: &broadcast::Sender<(SocketAddr, String)>,
) {
    match msg {
        ClientMessage::Connect { name } => {
            info!("Player {} connected as '{}'", player_id, name);
            peers.lock().await.insert(
                addr,
                PlayerState {
                    player_id,
                    name: name.clone(),
                },
            );

            let msg = ServerMessage::PlayerConnected {
                player_id,
                name: name.clone(),
            };
            if let Ok(json) = serde_json::to_string(&msg) {
                let _ = broadcast_tx.send((addr, json));
            }
        }
        ClientMessage::Disconnect => {
            info!("Player {} requested disconnect", player_id);
        }
        ClientMessage::PlayerUpdate(transform) => {
            let msg = ServerMessage::PlayerUpdate {
                player_id,
                transform: *transform,
            };
            if let Ok(json) = serde_json::to_string(&msg) {
                let _ = broadcast_tx.send((addr, json));
            }
        }
        ClientMessage::EntityUpdate {
            entity_id,
            transform,
            model,
        } => {
            let msg = ServerMessage::EntityUpdate {
                entity_id: *entity_id,
                transform: *transform,
                model: *model,
            };
            if let Ok(json) = serde_json::to_string(&msg) {
                let _ = broadcast_tx.send((addr, json));
            }
        }
        ClientMessage::ChatMessage(message) => {
            info!("Player {}: {}", player_id, message);
            let msg = ServerMessage::ChatMessage {
                player_id,
                message: message.clone(),
            };
            if let Ok(json) = serde_json::to_string(&msg) {
                let _ = broadcast_tx.send((addr, json));
            }
        }
    }
}
