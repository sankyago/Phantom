mod router;
mod session;
mod sync;

use std::sync::Arc;

use tokio::net::TcpListener;
use tokio_tungstenite::accept_async;
use tracing::{error, info};

use crate::session::SessionManager;
use crate::sync::EntitySyncBroadcaster;

/// Shared server state accessible from all connection tasks.
pub struct ServerState {
    pub sessions: SessionManager,
    pub sync: EntitySyncBroadcaster,
}

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .init();

    let state = Arc::new(ServerState {
        sessions: SessionManager::new(),
        sync: EntitySyncBroadcaster::new(),
    });

    let listener = TcpListener::bind("0.0.0.0:7788")
        .await
        .expect("Failed to bind TCP listener on 0.0.0.0:7788");

    info!("Phantom server listening on 0.0.0.0:7788");

    loop {
        let (stream, addr) = match listener.accept().await {
            Ok(conn) => conn,
            Err(e) => {
                error!("Failed to accept TCP connection: {e}");
                continue;
            }
        };

        let state = Arc::clone(&state);

        tokio::spawn(async move {
            let ws_stream = match accept_async(stream).await {
                Ok(ws) => ws,
                Err(e) => {
                    error!("WebSocket handshake failed for {addr}: {e}");
                    return;
                }
            };

            let player_id = state.sessions.add_player(addr, ws_stream);
            info!("Player {player_id} connected from {addr} (total: {})", state.sessions.player_count());

            router::MessageRouter::handle_connection(Arc::clone(&state), player_id).await;

            state.sessions.remove_player(player_id);
            info!("Player {player_id} disconnected (total: {})", state.sessions.player_count());
        });
    }
}
