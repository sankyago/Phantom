use std::net::SocketAddr;
use std::sync::atomic::{AtomicU32, Ordering};

use dashmap::DashMap;
use futures_util::stream::SplitSink;
use futures_util::StreamExt;
use tokio::net::TcpStream;
use tokio::sync::Mutex;
use tokio_tungstenite::tungstenite::Message;
use tokio_tungstenite::WebSocketStream;
use tracing::warn;

/// Represents a single connected player.
pub struct PlayerSession {
    pub id: u32,
    pub addr: SocketAddr,
    pub sink: Mutex<SplitSink<WebSocketStream<TcpStream>, Message>>,
}

/// Thread-safe registry of all connected players.
pub struct SessionManager {
    players: DashMap<u32, PlayerSession>,
    next_id: AtomicU32,
}

impl SessionManager {
    pub fn new() -> Self {
        Self {
            players: DashMap::new(),
            next_id: AtomicU32::new(1),
        }
    }

    /// Splits the WebSocket stream, stores the sink half, and returns the
    /// assigned player id. The read half is intentionally dropped here;
    /// the router will re-acquire it via its own mechanism in the future.
    pub fn add_player(&self, addr: SocketAddr, ws: WebSocketStream<TcpStream>) -> u32 {
        let id = self.next_id.fetch_add(1, Ordering::Relaxed);
        let (sink, _read) = ws.split();

        let session = PlayerSession {
            id,
            addr,
            sink: Mutex::new(sink),
        };

        self.players.insert(id, session);
        id
    }

    /// Removes a player from the session map.
    pub fn remove_player(&self, id: u32) {
        self.players.remove(&id);
    }

    /// Returns the number of currently connected players.
    pub fn player_count(&self) -> usize {
        self.players.len()
    }

    /// Broadcasts a binary message to all connected players, optionally
    /// excluding one (typically the sender).
    pub async fn broadcast(&self, data: &[u8], exclude: Option<u32>) {
        for entry in self.players.iter() {
            if Some(entry.id) == exclude {
                continue;
            }
            let mut sink = entry.sink.lock().await;
            use futures_util::SinkExt;
            if let Err(e) = sink.send(Message::Binary(data.to_vec().into())).await {
                warn!("Failed to send to player {}: {e}", entry.id);
            }
        }
    }
}
