use crate::wrapped_natives::{entities as entity_helpers, ui as ui_native};
use hook::natives::*;
use log::info;

const MENU_X: f32 = 0.15;
const MENU_Y_START: f32 = 0.3;
const MENU_WIDTH: f32 = 0.22;
const ITEM_HEIGHT: f32 = 0.04;
const TITLE_HEIGHT: f32 = 0.05;
const TEXT_SCALE: f32 = 0.35;

const BG_COLOR: (i32, i32, i32, i32) = (10, 10, 10, 200);
const TITLE_COLOR: (i32, i32, i32, i32) = (100, 50, 200, 230);
const SELECTED_COLOR: (i32, i32, i32, i32) = (80, 40, 160, 200);
const TEXT_COLOR: (i32, i32, i32, i32) = (255, 255, 255, 255);
const TEXT_SELECTED_COLOR: (i32, i32, i32, i32) = (255, 255, 100, 255);

static VEHICLE_MODELS: &[&str] = &[
    "adder", "zentorno", "t20", "osiris", "turismor", "entityxf", "cheetah",
    "infernus", "bullet", "vacca", "voltic", "comet2", "elegy2", "sultanrs",
    "kuruma", "insurgent", "hydra", "buzzard", "rhino", "dump",
];

pub struct Menu {
    pub open: bool,
    selected: usize,
    page: MenuPage,
}

#[derive(Clone, Copy, PartialEq)]
enum MenuPage {
    Main,
    SpawnVehicle,
}

struct MenuEntry {
    label: &'static str,
}

impl Menu {
    pub fn new() -> Self {
        Menu {
            open: false,
            selected: 0,
            page: MenuPage::Main,
        }
    }

    pub fn update(&mut self, world: &mut hecs::World) {
        // F1 toggles menu
        if hook::is_key_released(hook::keycodes::KEY_F1, true) {
            self.open = !self.open;
            if self.open {
                self.page = MenuPage::Main;
                self.selected = 0;
            }
        }

        if !self.open {
            return;
        }

        // Disable game input while menu is open
        pad::disable_all_control_actions(0);

        let item_count = self.current_items().len();

        // Navigate
        if hook::is_key_released(hook::keycodes::KEY_UP, true) && self.selected > 0 {
            self.selected -= 1;
        }
        if hook::is_key_released(hook::keycodes::KEY_DOWN, true) && self.selected + 1 < item_count
        {
            self.selected += 1;
        }

        // Back
        if hook::is_key_released(hook::keycodes::KEY_BACKSPACE, true) {
            if self.page != MenuPage::Main {
                self.page = MenuPage::Main;
                self.selected = 0;
            } else {
                self.open = false;
            }
            return;
        }

        // Select
        if hook::is_key_released(hook::keycodes::KEY_RETURN, true) {
            self.execute_selected(world);
        }

        self.draw();
    }

    fn current_items(&self) -> Vec<MenuEntry> {
        match self.page {
            MenuPage::Main => vec![
                MenuEntry { label: "Spawn Vehicle >" },
                MenuEntry { label: "Delete Current Vehicle" },
                MenuEntry { label: "Load Cayo Perico" },
                MenuEntry { label: "Unload Cayo Perico" },
                MenuEntry { label: "Teleport to Cayo Perico" },
            ],
            MenuPage::SpawnVehicle => VEHICLE_MODELS
                .iter()
                .map(|m| MenuEntry { label: m })
                .collect(),
        }
    }

    fn execute_selected(&mut self, world: &mut hecs::World) {
        match self.page {
            MenuPage::Main => match self.selected {
                0 => {
                    self.page = MenuPage::SpawnVehicle;
                    self.selected = 0;
                }
                1 => delete_current_vehicle(world),
                2 => set_cayo_perico(true),
                3 => set_cayo_perico(false),
                4 => teleport_cayo_perico(),
                _ => {}
            },
            MenuPage::SpawnVehicle => {
                if self.selected < VEHICLE_MODELS.len() {
                    spawn_vehicle(world, VEHICLE_MODELS[self.selected]);
                }
            }
        }
    }

    fn draw(&self) {
        let items = self.current_items();
        let total_height = TITLE_HEIGHT + (items.len() as f32 * ITEM_HEIGHT) + 0.01;

        let title = match self.page {
            MenuPage::Main => "Phantom",
            MenuPage::SpawnVehicle => "Spawn Vehicle",
        };

        // Title background
        graphics::draw_rect(
            MENU_X,
            MENU_Y_START,
            MENU_WIDTH,
            TITLE_HEIGHT,
            TITLE_COLOR.0,
            TITLE_COLOR.1,
            TITLE_COLOR.2,
            TITLE_COLOR.3,
        );

        // Title text
        ui_native::draw_text(
            title,
            MENU_X - MENU_WIDTH / 2.0 + 0.01,
            MENU_Y_START - TITLE_HEIGHT / 2.0 + 0.005,
            TEXT_SCALE + 0.05,
            TEXT_COLOR,
            false,
            MENU_X + MENU_WIDTH / 2.0,
        );

        // Items background
        let items_bg_y = MENU_Y_START + TITLE_HEIGHT / 2.0 + (items.len() as f32 * ITEM_HEIGHT) / 2.0;
        graphics::draw_rect(
            MENU_X,
            items_bg_y,
            MENU_WIDTH,
            items.len() as f32 * ITEM_HEIGHT + 0.01,
            BG_COLOR.0,
            BG_COLOR.1,
            BG_COLOR.2,
            BG_COLOR.3,
        );

        // Draw items
        for (i, item) in items.iter().enumerate() {
            let y = MENU_Y_START + TITLE_HEIGHT / 2.0 + (i as f32 * ITEM_HEIGHT) + 0.005;
            let is_selected = i == self.selected;

            if is_selected {
                graphics::draw_rect(
                    MENU_X,
                    y + ITEM_HEIGHT / 2.0,
                    MENU_WIDTH,
                    ITEM_HEIGHT,
                    SELECTED_COLOR.0,
                    SELECTED_COLOR.1,
                    SELECTED_COLOR.2,
                    SELECTED_COLOR.3,
                );
            }

            let color = if is_selected { TEXT_SELECTED_COLOR } else { TEXT_COLOR };

            ui_native::draw_text(
                item.label,
                MENU_X - MENU_WIDTH / 2.0 + 0.01,
                y,
                TEXT_SCALE,
                color,
                false,
                MENU_X + MENU_WIDTH / 2.0,
            );
        }

        // Footer hint
        let footer_y = MENU_Y_START + TITLE_HEIGHT / 2.0 + (items.len() as f32 * ITEM_HEIGHT) + 0.01;
        ui_native::draw_text(
            "Enter: Select | Backspace: Back | F1: Close",
            MENU_X - MENU_WIDTH / 2.0 + 0.005,
            footer_y,
            0.25,
            (180, 180, 180, 200),
            false,
            MENU_X + MENU_WIDTH / 2.0,
        );
    }
}

fn spawn_vehicle(world: &mut hecs::World, model_name: &str) {
    let player_ped_id = player::player_ped_id();
    let position = entity::get_entity_coords(player_ped_id, true);
    let rotation = entity::get_entity_rotation(player_ped_id, 2);

    let (handle, model, transform, vehicle_data) =
        crate::entities::create_vehicle_with_model_name(model_name, position, rotation, 112, 112);

    ped::set_ped_into_vehicle(player_ped_id, handle.handle, -1);
    world.spawn((handle, model, transform, vehicle_data));

    info!("Spawned vehicle: {}", model_name.to_uppercase());
}

fn delete_current_vehicle(world: &mut hecs::World) {
    let player_ped_id = player::player_ped_id();

    if !ped::is_ped_in_any_vehicle(player_ped_id, false) {
        return;
    }

    let mut vehicle_id = ped::get_vehicle_ped_is_in(player_ped_id, false);
    let vehicle_id_copy = vehicle_id;

    entity_helpers::delete_entity(&mut vehicle_id);

    let found: Option<hecs::Entity> = world
        .query::<&crate::entities::EntityHandle>()
        .iter()
        .find(|(_, h)| h.handle == vehicle_id_copy)
        .map(|(e, _)| e);

    if let Some(entity) = found {
        let _ = world.despawn(entity);
        info!("Deleted vehicle");
    }
}

fn set_cayo_perico(enable: bool) {
    let name = std::ffi::CString::new("HeistIsland").unwrap();
    streaming::_set_island_hopper_enabled(&name, enable);
    info!(
        "Cayo Perico {}",
        if enable { "loaded" } else { "unloaded" }
    );
}

fn teleport_cayo_perico() {
    let player_ped = player::player_ped_id();
    entity::set_entity_coords_no_offset(player_ped, 4840.571, -5174.425, 2.0, false, false, false);
    info!("Teleported to Cayo Perico");
}
