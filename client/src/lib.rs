#![deny(unsafe_code)]

use log::{debug, info};
use std::sync::LazyLock;
use winreg::{enums::HKEY_LOCAL_MACHINE, RegKey};

mod cleanup;
mod entities;
#[allow(unsafe_code)]
mod imgui;
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

    hook::register_present_callback(imgui::d3d11_present);
    hook::register_window_proc_callback(imgui::wndproc);

    info!("Successfully started Phantom Multiplayer");
}

fn script_callback() {
    let mut world = hecs::World::new();

    // Startup systems (run once)
    cleanup::startup_system(&mut world);
    ui::ui_startup_system(&mut world);

    // Main loop
    loop {
        hook::update_keyboard();
        cleanup::cleanup_tick_system();
        cleanup::cleanup_system(&mut world);
        cleanup::hijack_frontend_menu();
        ui::draw_text_entries(&mut world);
        imgui::handle_cursor();
        thread_jumper::run_native_callbacks(&mut world);
        hook::script_wait(0);
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
