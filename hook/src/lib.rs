#![recursion_limit = "1024"]
use cpp::cpp;
pub use d3drenderer::register_present_callback;
use retour::static_detour;
pub use keyboard::{
    is_key_pressed, is_key_released, register_keyboard_callback, register_window_proc_callback,
    update_keyboard, KeyboardCallback, KeyboardCallbackState,
};
pub use keycodes;
use log::{debug, error};
use memory::{address_fill, get_pattern, get_pattern_rip, get_pattern_sub};
pub use replay_interface::{get_all_peds, get_all_vehicles};
use std::ffi::{c_void, CStr};
pub use winapi;
use winapi::um::winnt::HRESULT;

mod crossmap;
mod d3drenderer;
mod keyboard;
mod memory;
#[macro_use]
pub(crate) mod native_handling;
#[allow(warnings)]
pub mod natives;
mod replay_interface;
mod script_patches;
pub mod types;

static_detour! {
    static GetLabelText: fn(i64, *const i8, i64) -> *mut c_void;
}

pub(crate) static mut SCRIPT_CALLBACK: Option<fn()> = None;
static LABEL: &str = "Loading CryV Multiplayer\0";

type AddressToEntity = fn(*mut c_void) -> i32;
type SwapchainPresent = fn(*mut winapi::shared::dxgi::IDXGISwapChain, u32, u32) -> HRESULT;

pub struct Pointers {
    pub logos: *mut c_void,
    pub game_legal_skip: *mut c_void,
    pub frame_count: *mut c_void,
    pub game_state: *mut c_void,
    pub swapchain: *mut *mut winapi::shared::dxgi::IDXGISwapChain,
    pub address_to_entity: Option<AddressToEntity>,
    pub registration_table: *mut c_void,
    pub get_label_text: *mut c_void,
    pub replay_interface: *mut replay_interface::CReplayInterface,
    pub set_vector_results: *mut c_void,
    pub story_mode_skip: *mut c_void,
    pub model_check_skip: *mut c_void,
    pub model_spawn_fix: *mut c_void,
    pub slowdown_fix: *mut c_void,
    pub swapchain_hook: Option<d3drenderer::VmtHook>,
}

impl Pointers {
    const INIT: Self = Pointers {
        logos: std::ptr::null_mut(),
        game_legal_skip: std::ptr::null_mut(),
        frame_count: std::ptr::null_mut(),
        game_state: std::ptr::null_mut(),
        swapchain: std::ptr::null_mut(),
        address_to_entity: None,
        registration_table: std::ptr::null_mut(),
        get_label_text: std::ptr::null_mut(),
        replay_interface: std::ptr::null_mut(),
        set_vector_results: std::ptr::null_mut(),
        story_mode_skip: std::ptr::null_mut(),
        model_check_skip: std::ptr::null_mut(),
        model_spawn_fix: std::ptr::null_mut(),
        slowdown_fix: std::ptr::null_mut(),
        swapchain_hook: None,
    };
}

pub static mut POINTERS: Pointers = Pointers::INIT;

fn swapchain_present(
    swapchain: *mut winapi::shared::dxgi::IDXGISwapChain,
    sync_interval: u32,
    flags: u32,
) -> HRESULT {
    d3drenderer::present(swapchain);

    unsafe {
        let mut hook = POINTERS.swapchain_hook.unwrap();
        let obj = hook.get_original(8) as *mut SwapchainPresent;
        let obj = (&(obj as *const SwapchainPresent) as *const *const SwapchainPresent)
            as *const SwapchainPresent;

        return (*obj)(swapchain, sync_interval, flags);
    }
}

pub fn initialize(script_callback: fn()) {
    unsafe {
        SCRIPT_CALLBACK = Some(script_callback);
    }

    keyboard::initialize_keyboard_hook();
    fetch_pointers();
    hook_get_label_text();
    d3drenderer::init();

    unsafe {
        POINTERS.swapchain_hook = Some(d3drenderer::VmtHook::new(
            (*POINTERS.swapchain) as *mut std::ffi::c_void,
            19,
        ));

        POINTERS
            .swapchain_hook
            .unwrap()
            .hook(swapchain_present as *mut std::ffi::c_void, 8);
        POINTERS.swapchain_hook.unwrap().enable();
    };

    // Wait a few seconds until game "initializes"
    std::thread::sleep(std::time::Duration::from_secs(3));

    unsafe {
        while *(POINTERS.game_state as *mut i32) != 0 {
            std::thread::sleep(std::time::Duration::from_millis(50));
        }
    }

    native_handling::hook_get_frame_count();
}

pub fn script_wait(time: u32) {
    unsafe {
        native_handling::script_wait(time);
    }
}

fn fetch_pointers() {
    unsafe {
        POINTERS.logos = get_pattern("70 6C 61 74 66 6F 72 6D 3A".to_owned(), 0);
        debug!("Logos: {:p}", POINTERS.logos);
        address_fill(POINTERS.logos, 1, 0xC3);

        POINTERS.game_legal_skip = get_pattern("72 1F E8 ? ? ? ? 8B 0D".to_owned(), 0);
        debug!("GameLegalSkip: {:p}", POINTERS.game_legal_skip);
        address_fill(POINTERS.game_legal_skip, 2, 0x90);

        POINTERS.frame_count = get_pattern("8B 15 ? ? ? ? 41 FF CF".to_owned(), 2);
        debug!("FrameCount: {:p}", POINTERS.frame_count);

        POINTERS.game_state = get_pattern_rip("83 3D ? ? ? ? ? 8A D9 74 0A".to_owned(), 2);
        debug!("GameState: {:p}", POINTERS.game_state);

        POINTERS.swapchain = get_pattern_rip("48 8B 0D ? ? ? ? 48 8D 55 A0 48 8B 01".to_owned(), 3)
            as *mut *mut winapi::shared::dxgi::IDXGISwapChain;
        debug!("Swapchain: {:p}", POINTERS.swapchain);

        POINTERS.address_to_entity = Some(std::mem::transmute(get_pattern(
            "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 8B 15 ? ? ? ? 48 8B F9 48 83 C1 10 33 DB"
                .to_owned(),
            0,
        )));
        debug!("AddressToEntity: {:p}", POINTERS.address_to_entity.unwrap());

        POINTERS.registration_table = get_pattern_rip("76 32 48 8B 53 40 48 8D 0D".to_owned(), 9);
        debug!("RegistrationTable: {:p}", POINTERS.registration_table);

        POINTERS.get_label_text = get_pattern_sub("75 ? E8 ? ? ? ? 8B 0D ? ? ? ? 65 48 8B 04 25 ? ? ? ? BA ? ? ? ? 48 8B 04 C8 8B 0C 02 D1 E9".to_owned(), 19);
        debug!("GetLabelText: {:p}", POINTERS.get_label_text);

        let replay_interface = get_pattern_rip("48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ?".to_owned(), 3);
        POINTERS.replay_interface =
            *(replay_interface as *mut *mut replay_interface::CReplayInterface);
        debug!("ReplayInterface: {:p}", POINTERS.replay_interface);

        let set_vector_results = get_pattern("83 79 18 00 48 8B D1 74 4A FF 4A 18".to_owned(), 0);
        POINTERS.set_vector_results = set_vector_results;
        cpp!([set_vector_results as "void*"] {
            ScrNativeCallContext::SetVectorResults = reinterpret_cast<decltype(ScrNativeCallContext::SetVectorResults)>(set_vector_results);
        });
        debug!("SetVectorResults: {:p}", POINTERS.set_vector_results);

        POINTERS.story_mode_skip =
            get_pattern("48 83 3D ? ? ? ? ? 88 05 ? ? ? ? 75 0B".to_owned(), 8);
        debug!("StoryModeSkip: {:p}", POINTERS.story_mode_skip);
        address_fill(POINTERS.story_mode_skip, 6, 0x90);

        POINTERS.model_check_skip = get_pattern("48 85 C0 0F 84 ? ? ? ? 8B 48 50".to_owned(), 0);
        debug!("ModelCheckSkip: {:p}", POINTERS.model_check_skip);
        address_fill(POINTERS.model_check_skip, 24, 0x90);

        POINTERS.model_spawn_fix = get_pattern("48 8B C8 FF 52 30 84 C0 74 05 48".to_owned(), 8);
        debug!("ModelSpawnFix: {:p}", POINTERS.model_spawn_fix);
        address_fill(POINTERS.model_spawn_fix, 2, 0x90);

        POINTERS.slowdown_fix = get_pattern("75 05 0F 28 E3 EB 03".to_owned(), 0);
        debug!("SlowdownFix: {:p}", POINTERS.slowdown_fix);
        address_fill(POINTERS.slowdown_fix, 2, 0x90);
    };
}

fn hook_get_label_text() {
    if let Err(error) = unsafe {
        GetLabelText.initialize(std::mem::transmute(POINTERS.get_label_text), get_label_text)
    } {
        error!("Error initializing GetLabelText hook: {}", error);

        return;
    }

    if let Err(error) = unsafe { GetLabelText.enable() } {
        error!("Error enabling GetLabelText hook: {}", error);

        return;
    }
}

fn get_label_text(a1: i64, a2: *const i8, a3: i64) -> *mut c_void {
    unsafe {
        let label = CStr::from_ptr(a2);
        let label_string = label.to_str().unwrap();

        if label_string == "LOADING_SPLAYER_L" {
            return LABEL.as_ptr() as *mut c_void;
        }

        GetLabelText.call(a1, a2, a3)
    }
}
