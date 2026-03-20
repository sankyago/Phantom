use glam::Vec3;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Copy, Clone, Debug, PartialEq)]
pub struct EntityTransform {
    pub position: Vec3,
    pub rotation: Vec3,
    pub velocity: Vec3,
}

#[derive(Serialize, Deserialize, Copy, Clone, Debug, PartialEq)]
pub struct EntityModel {
    pub model: u32,
}

impl std::fmt::Display for EntityModel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:#X}", self.model)
    }
}

/// Messages sent from client to server.
#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum ClientMessage {
    Connect { name: String },
    Disconnect,
    PlayerUpdate(EntityTransform),
    EntityUpdate { entity_id: u32, transform: EntityTransform, model: EntityModel },
    ChatMessage(String),
}

/// Messages sent from server to client.
#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum ServerMessage {
    Welcome { player_id: u32 },
    PlayerConnected { player_id: u32, name: String },
    PlayerDisconnected { player_id: u32 },
    PlayerUpdate { player_id: u32, transform: EntityTransform },
    EntityUpdate { entity_id: u32, transform: EntityTransform, model: EntityModel },
    ChatMessage { player_id: u32, message: String },
}
