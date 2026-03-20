#![deny(unsafe_code)]

use log::{debug, info};
use std::sync::LazyLock;
use winreg::{enums::HKEY_LOCAL_MACHINE, RegKey};

mod cleanup;
mod entities;
mod menu;
pub mod network;
mod thread_jumper;
mod ui;
mod utility;
mod wrapped_natives;

static INSTALL_DIRECTORY: LazyLock<String> = LazyLock::new(|| {
    let key = RegKey::predef(HKEY_LOCAL_MACHINE)
        .open_subkey(r"Software\Phantom")
        .expect("Phantom registry key not found. Run the launcher first.");

    let value: String = key
        .get_value("InstallDirectory")
        .expect("InstallDirectory registry value not found");

    value
});

make_entrypoint!(entrypoint);

fn entrypoint() {
    create_logger().expect("Something went wrong while creating the logger!");

    debug!("InstallDirectory: {}", *INSTALL_DIRECTORY);

    info!("--------------------------");
    info!("Starting Phantom Multiplayer");

    hook::initialize(script_callback);

    info!("Successfully started Phantom Multiplayer");
}

fn script_callback() {
    let mut world = hecs::World::new();
    let mut game_menu = menu::Menu::new();

    // Startup systems (run once)
    cleanup::startup_system(&mut world);
    ui::ui_startup_system(&mut world);

    // Connect to the multiplayer server
    let net = network::NetworkClient::connect_localhost("Player");

    // Main loop
    loop {
        hook::update_keyboard();
        cleanup::cleanup_tick_system();
        cleanup::cleanup_system(&mut world);
        cleanup::hijack_frontend_menu();

        // Menu (F1 to toggle)
        game_menu.update(&mut world);

        // Send player position to server each tick
        if net.is_connected() {
            let player_ped = hook::natives::player::player_ped_id();
            let pos = hook::natives::entity::get_entity_coords(player_ped, true);
            let rot = hook::natives::entity::get_entity_rotation(player_ped, 2);
            let vel = hook::natives::entity::get_entity_velocity(player_ped);

            net.send_player_update(
                glam::Vec3::new(pos.x, pos.y, pos.z),
                glam::Vec3::new(rot.x, rot.y, rot.z),
                glam::Vec3::new(vel.x, vel.y, vel.z),
            );
        }

        // Process incoming server messages
        for msg in net.poll_messages() {
            handle_server_message(&msg);
        }

        ui::draw_text_entries(&mut world);
        thread_jumper::run_native_callbacks(&mut world);
        hook::script_wait(0);
    }
}

fn handle_server_message(msg: &shared::ServerMessage) {
    use shared::ServerMessage;

    match msg {
        ServerMessage::Welcome { player_id } => {
            info!("Connected to server! Player ID: {}", player_id);
        }
        ServerMessage::PlayerConnected { player_id, name } => {
            info!("Player '{}' (ID: {}) connected", name, player_id);
        }
        ServerMessage::PlayerDisconnected { player_id } => {
            info!("Player {} disconnected", player_id);
        }
        ServerMessage::PlayerUpdate { player_id, transform } => {
            debug!(
                "Player {} at ({:.1}, {:.1}, {:.1})",
                player_id, transform.position.x, transform.position.y, transform.position.z
            );
        }
        ServerMessage::EntityUpdate { entity_id, transform, model } => {
            debug!("Entity {} (model {}) updated", entity_id, model);
            let _ = transform;
        }
        ServerMessage::ChatMessage { player_id, message } => {
            info!("[Chat] Player {}: {}", player_id, message);
        }
    }
}

fn create_logger() -> Result<(), fern::InitError> {
    let log_file_path = format!("{}/phantom.log", *INSTALL_DIRECTORY);

    fern::Dispatch::new()
        .format(|out, message, record| {
            out.finish(format_args!(
                "{}[{}][{}] {}",
                chrono::Local::now().format("[%Y-%m-%d][%H:%M:%S]"),
                record.target(),
                record.level(),
                message
            ))
        })
        .level(log::LevelFilter::Debug)
        .chain(std::io::stdout())
        .chain(fern::log_file(log_file_path)?)
        .apply()?;

    Ok(())
}
