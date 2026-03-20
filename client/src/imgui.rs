use std::sync::{
    Mutex,
    atomic::{AtomicBool, Ordering},
};

use std::sync::OnceLock;
use winapi::{
    shared::minwindef::LPARAM, shared::minwindef::UINT, shared::minwindef::WPARAM,
    shared::windef::HWND, um::winuser::FindWindowA,
};

use crate::{entities::EntityHandle, wrapped_natives::entities};

static SHOW_CURSOR: AtomicBool = AtomicBool::new(false);
static LOG_BUFFER: Mutex<Vec<String>> = Mutex::new(Vec::new());

static CTX: OnceLock<Mutex<ContextWrapper>> = OnceLock::new();
static IMGUI_DATA: Mutex<Option<ImguiData>> = Mutex::new(None);

#[derive(Debug)]
struct ContextWrapper {
    pub ctx: imgui::Context,
}

#[derive(Debug)]
struct ImguiData {
    pub show_commands: bool,
    pub show_logs: bool,
    pub vehicle_model: imgui::ImString,
    pub primary_color: i32,
    pub secondary_color: i32,
}

impl Default for ImguiData {
    fn default() -> Self {
        ImguiData {
            show_commands: false,
            show_logs: false,
            vehicle_model: imgui::ImString::with_capacity(32),
            primary_color: 112,
            secondary_color: 112,
        }
    }
}

// Safety: ImGui context is only accessed from the render thread via the Present hook callback.
// All access is serialized through the Mutex.
unsafe impl Send for ContextWrapper {}

fn get_context(
    device: *mut hook::winapi::um::d3d11::ID3D11Device,
    context: *mut hook::winapi::um::d3d11::ID3D11DeviceContext,
) -> &'static Mutex<ContextWrapper> {
    CTX.get_or_init(|| {
        unsafe {
            let window_name = std::ffi::CString::new("grcWindow").unwrap();
            let window = FindWindowA(window_name.as_ptr(), std::ptr::null());

            let ctx = imgui::Context::create();

            imgui_sys::ImGui_ImplDX11_Init(
                device as *mut imgui_sys::ID3D11Device,
                context as *mut imgui_sys::ID3D11DeviceContext,
            );
            imgui_sys::ImGui_ImplWin32_Init(window as *mut std::ffi::c_void);
        }

        add_log("Initialized D3D11_PRESENT hook");

        Mutex::new(ContextWrapper {
            ctx: imgui::Context::create(),
        })
    })
}

fn get_data() -> std::sync::MutexGuard<'static, Option<ImguiData>> {
    let mut guard = IMGUI_DATA.lock().unwrap();
    if guard.is_none() {
        *guard = Some(ImguiData::default());
    }
    guard
}

pub(super) fn d3d11_present(
    device: *mut hook::winapi::um::d3d11::ID3D11Device,
    context: *mut hook::winapi::um::d3d11::ID3D11DeviceContext,
) {
    let mut wrapper = get_context(device, context).lock().unwrap();
    let mut data_guard = get_data();
    let data = data_guard.as_mut().unwrap();

    unsafe {
        imgui_sys::ImGui_ImplDX11_NewFrame();
        imgui_sys::ImGui_ImplWin32_NewFrame();
    };

    let ui = wrapper.ctx.frame();

    draw(&ui, data);

    unsafe {
        (*imgui_sys::igGetIO()).MouseDrawCursor = SHOW_CURSOR.load(Ordering::Relaxed);
        let draw_data = ui.render() as *const imgui::DrawData;
        imgui_sys::ImGui_ImplDX11_RenderDrawData(
            (draw_data as *const std::ffi::c_void) as *mut std::ffi::c_void,
        );
    };
}

fn draw(ui: &imgui::Ui, data: &mut ImguiData) {
    ui.main_menu_bar(|| {
        ui.menu(imgui::im_str!("Phantom"), true, || {});

        ui.menu(imgui::im_str!("Windows"), true, || {
            if imgui::MenuItem::new(imgui::im_str!("Commands"))
                .selected(data.show_commands)
                .build(ui)
            {
                data.show_commands = !data.show_commands;
            }

            if imgui::MenuItem::new(imgui::im_str!("Logs"))
                .selected(data.show_logs)
                .build(ui)
            {
                data.show_logs = !data.show_logs;
            }
        });
    });

    if data.show_commands {
        imgui::Window::new(imgui::im_str!("Commands"))
            .size([300.0, 100.0], imgui::Condition::FirstUseEver)
            .build(ui, || {
                if ui.button(imgui::im_str!("Spawn Vehicle"), [0.0, 0.0]) {
                    ui.open_popup(imgui::im_str!("Spawn Vehicle"));
                }

                ui.same_line_with_spacing(0.0, -1.0);

                if ui.button(imgui::im_str!("Delete Vehicle"), [0.0, 0.0]) {
                    crate::thread_jumper::run(Box::new(|world| {
                        let player_ped_id = hook::natives::player::player_ped_id();
                        let is_in_vehicle =
                            hook::natives::ped::is_ped_in_any_vehicle(player_ped_id, false);

                        if !is_in_vehicle {
                            add_log("You are not in any vehicle");
                            return;
                        }

                        let mut vehicle_id =
                            hook::natives::ped::get_vehicle_ped_is_in(player_ped_id, false);
                        let vehicle_id_copy = vehicle_id;

                        entities::delete_entity(&mut vehicle_id);

                        let found_entity: Option<hecs::Entity> = {
                            let mut result = None;
                            for (entity, entity_data) in
                                world.query::<&EntityHandle>().iter()
                            {
                                if entity_data.handle == vehicle_id_copy {
                                    result = Some(entity);
                                    break;
                                }
                            }
                            result
                        };

                        if let Some(entity) = found_entity {
                            let _ = world.despawn(entity);
                            add_log("Vehicle has been deleted");
                        }
                    }));
                }

                if ui.button(imgui::im_str!("Load Cayo Perico"), [0.0, 0.0]) {
                    crate::thread_jumper::run(Box::new(|_| {
                        let heist_island_cstring =
                            std::ffi::CString::new("HeistIsland").unwrap();
                        hook::natives::streaming::_set_island_hopper_enabled(
                            &heist_island_cstring,
                            true,
                        );
                    }));
                }

                ui.same_line_with_spacing(0.0, -1.0);

                if ui.button(imgui::im_str!("Unload Cayo Perico"), [0.0, 0.0]) {
                    crate::thread_jumper::run(Box::new(|_| {
                        let heist_island_cstring =
                            std::ffi::CString::new("HeistIsland").unwrap();
                        hook::natives::streaming::_set_island_hopper_enabled(
                            &heist_island_cstring,
                            false,
                        );
                    }));
                }

                if ui.button(imgui::im_str!("Teleport to Cayo Perico"), [0.0, 0.0]) {
                    crate::thread_jumper::run(Box::new(|_| {
                        let player_ped = hook::natives::player::player_ped_id();

                        hook::natives::entity::set_entity_coords_no_offset(
                            player_ped, 4840.571, -5174.425, 2.0, false, false, false,
                        );
                    }));
                }

                ui.popup_modal(imgui::im_str!("Spawn Vehicle"))
                    .always_auto_resize(true)
                    .build(|| {
                        ui.input_text(
                            imgui::im_str!("Vehicle Model"),
                            &mut data.vehicle_model,
                        )
                        .build();

                        ui.input_int(imgui::im_str!("Primary Color"), &mut data.primary_color)
                            .build();

                        ui.input_int(
                            imgui::im_str!("Secondary Color"),
                            &mut data.secondary_color,
                        )
                        .build();

                        ui.separator();

                        if ui.button(imgui::im_str!("Done"), [120.0, 0.0]) {
                            let model_name = data.vehicle_model.to_string();
                            let primary_color = data.primary_color;
                            let secondary_color = data.secondary_color;

                            crate::thread_jumper::run(Box::new(move |world| {
                                let player_ped_id = hook::natives::player::player_ped_id();
                                let position = hook::natives::entity::get_entity_coords(
                                    player_ped_id,
                                    true,
                                );
                                let rotation = hook::natives::entity::get_entity_rotation(
                                    player_ped_id,
                                    2,
                                );

                                let (handle, model, transform, vehicle) =
                                    crate::entities::create_vehicle_with_model_name(
                                        &model_name,
                                        position,
                                        rotation,
                                        primary_color,
                                        secondary_color,
                                    );

                                hook::natives::ped::set_ped_into_vehicle(
                                    player_ped_id,
                                    handle.handle,
                                    -1,
                                );

                                world.spawn((handle, model, transform, vehicle));

                                add_log(&format!(
                                    "Spawned vehicle ({})",
                                    model_name.to_uppercase()
                                ));
                            }));

                            ui.close_current_popup();
                        }
                    });
            });
    }

    if data.show_logs {
        imgui::Window::new(imgui::im_str!("Logs"))
            .size([300.0, 100.0], imgui::Condition::FirstUseEver)
            .build(ui, || {
                let buffer = LOG_BUFFER.lock().unwrap();
                ui.text(imgui::ImString::new(buffer.join("\n")));
            });
    }
}

pub(super) fn wndproc(hwnd: HWND, message: UINT, w_param: WPARAM, l_param: LPARAM) {
    if !SHOW_CURSOR.load(Ordering::Relaxed) {
        return;
    }

    unsafe {
        imgui_sys::ImGui_ImplWin32_WndProcHandler(
            hwnd as *mut std::ffi::c_void,
            message,
            w_param as *mut u32,
            l_param as i64,
        );
    };
}

pub(super) fn handle_cursor() {
    if hook::is_key_released(hook::keycodes::KEY_F1, true) {
        let current = SHOW_CURSOR.load(Ordering::Relaxed);
        SHOW_CURSOR.store(!current, Ordering::Relaxed);
    }

    if SHOW_CURSOR.load(Ordering::Relaxed) {
        hook::natives::pad::disable_all_control_actions(0);
    }
}

fn add_log(message: &str) {
    let mut buffer = LOG_BUFFER.lock().unwrap();
    let time = chrono::Local::now();
    buffer.push(format!("[{}] {}", time.format("%Y-%m-%d %H:%M:%S"), message));
}
