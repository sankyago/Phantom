use std::sync::Arc;

use tokio::time::{self, Duration};
use tracing::info;

use crate::ServerState;

/// Routes incoming FlatBuffers messages to the appropriate handler.
pub struct MessageRouter;

impl MessageRouter {
    /// Placeholder message dispatch loop.
    ///
    /// In the future this will deserialise incoming FlatBuffers frames from
    /// the player's read-half and dispatch them to the correct handler.
    /// For now it keeps the connection alive by periodically checking
    /// player count.
    pub async fn handle_connection(state: Arc<ServerState>, player_id: u32) {
        let mut interval = time::interval(Duration::from_secs(5));

        loop {
            interval.tick().await;

            // If the player was removed externally, exit the loop.
            if state.sessions.player_count() == 0 {
                break;
            }

            info!(
                "Player {player_id} heartbeat - {} player(s) online",
                state.sessions.player_count()
            );
        }
    }
}
