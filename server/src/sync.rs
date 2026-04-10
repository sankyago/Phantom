use dashmap::DashMap;

/// Snapshot of an entity's replicated state.
#[derive(Debug, Clone)]
pub struct EntityState {
    pub entity_id: u32,
    pub entity_type: u32,
    pub model_hash: u32,
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub owner_id: u32,
}

/// Manages the authoritative world state for entity synchronisation.
pub struct EntitySyncBroadcaster {
    entities: DashMap<u32, EntityState>,
}

impl EntitySyncBroadcaster {
    pub fn new() -> Self {
        Self {
            entities: DashMap::new(),
        }
    }

    /// Inserts or updates an entity's state.
    pub fn update_entity(&self, state: EntityState) {
        self.entities.insert(state.entity_id, state);
    }

    /// Removes an entity from the world.
    pub fn remove_entity(&self, entity_id: u32) {
        self.entities.remove(&entity_id);
    }

    /// Returns a snapshot of every entity currently in the world.
    pub fn get_world_state(&self) -> Vec<EntityState> {
        self.entities.iter().map(|e| e.value().clone()).collect()
    }
}
