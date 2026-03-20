extern crate winapi;

use std::ffi::CString;
use sysinfo::System;
use winapi::shared::minwindef::DWORD;
use winapi::um::libloaderapi::{GetProcAddress, LoadLibraryA};
use winapi::um::memoryapi::{VirtualAllocEx, WriteProcessMemory};
use winapi::um::processthreadsapi::{CreateRemoteThread, OpenProcess};
use winapi::um::winuser::{FindWindowA, GetWindowThreadProcessId, IsWindowVisible};
use winreg::enums::HKEY_LOCAL_MACHINE;
use winreg::RegKey;

fn main() {
    let path = match get_gta_path() {
        Some(path) => path,
        None => {
            println!("Could not find GTA5 path! Make sure the game is installed via Steam or Rockstar Games Launcher.");
            return;
        }
    };

    println!("Grand Theft Auto V Path: {}", path);

    let dll_path = match std::env::current_exe() {
        Ok(exe) => match exe.parent() {
            Some(dir) => match dir.join("client.dll").to_str() {
                Some(s) => s.to_string(),
                None => {
                    println!("Failed to resolve DLL path");
                    return;
                }
            },
            None => {
                println!("Failed to resolve executable directory");
                return;
            }
        },
        Err(e) => {
            println!("Failed to get current executable path: {}", e);
            return;
        }
    };

    let dll_directory = std::env::current_exe()
        .ok()
        .and_then(|exe| exe.parent().map(|p| p.to_string_lossy().to_string()))
        .unwrap_or_default();

    println!("Current dll path: {}", &dll_path);
    println!("Current dll directory: {}", &dll_directory);

    if let Err(e) = write_registry_value(
        RegKey::predef(HKEY_LOCAL_MACHINE),
        r"Software\Phantom",
        "InstallDirectory",
        &dll_directory,
    ) {
        println!("Warning: Could not write registry value (try running as admin): {}", e);
    }

    if let Err(error) = start_process(&path, "") {
        println!("Error while trying to start GTA5 process: {}", error);
        return;
    }

    let process_id = wait_for_process("GTA5.exe");
    println!("Found GTA5 process with PID: {}", process_id);

    wait_for_window("Grand Theft Auto V", process_id);

    println!("Injecting client");

    let process = unsafe {
        OpenProcess(
            winapi::um::winnt::PROCESS_ALL_ACCESS,
            winapi::shared::minwindef::FALSE,
            process_id,
        )
    };

    if process.is_null() {
        println!("Failed to open GTA5 process. Try running as administrator.");
        return;
    }

    if let Err(e) = inject_dll(process, &dll_path) {
        println!("Injection failed: {}", e);
        return;
    }

    println!("Done");
}

fn inject_dll(process: *mut winapi::ctypes::c_void, dll_path: &str) -> Result<(), String> {
    let dll_path_string = CString::new(dll_path).map_err(|e| format!("Invalid DLL path: {}", e))?;
    let dll_path_size = dll_path.len() + 1;

    let memory_allocation = unsafe {
        VirtualAllocEx(
            process,
            std::ptr::null_mut(),
            dll_path_size,
            winapi::um::winnt::MEM_COMMIT | winapi::um::winnt::MEM_RESERVE,
            winapi::um::winnt::PAGE_READWRITE,
        )
    };

    if memory_allocation.is_null() {
        return Err("Failed to allocate memory in target process".into());
    }

    let mut bytes_written: usize = 0;
    let write_result = unsafe {
        WriteProcessMemory(
            process,
            memory_allocation,
            dll_path_string.as_ptr() as *mut winapi::ctypes::c_void,
            dll_path_size,
            &mut bytes_written,
        )
    };

    if write_result == 0 {
        return Err("Failed to write memory into target process".into());
    }

    let kernel_string = CString::new("kernel32").unwrap();
    let load_library_string = CString::new("LoadLibraryA").unwrap();

    let load_library_address = unsafe {
        GetProcAddress(
            LoadLibraryA(kernel_string.as_ptr()),
            load_library_string.as_ptr(),
        )
    };

    if load_library_address.is_null() {
        return Err("Failed to find LoadLibraryA address".into());
    }

    let mut thread_id: DWORD = 0;
    let thread = unsafe {
        CreateRemoteThread(
            process,
            std::ptr::null_mut(),
            0,
            Some(std::mem::transmute(load_library_address)),
            memory_allocation,
            0,
            &mut thread_id,
        )
    };

    if thread.is_null() {
        return Err("Failed to create remote thread in target process".into());
    }

    Ok(())
}

fn wait_for_process(name: &str) -> u32 {
    let mut system = System::new_all();

    loop {
        system.refresh_processes();

        for (pid, process) in system.processes() {
            if process.name().to_string_lossy() == name {
                return pid.as_u32();
            }
        }

        std::thread::sleep(std::time::Duration::from_millis(100));
    }
}

fn wait_for_window(window_name: &str, process_id: u32) {
    let window_cstring = CString::new(window_name).unwrap();

    loop {
        let handle = unsafe { FindWindowA(std::ptr::null_mut(), window_cstring.as_ptr()) };

        if !handle.is_null() {
            let mut window_pid: DWORD = 0;
            unsafe {
                GetWindowThreadProcessId(handle, &mut window_pid);
            };

            let is_visible = unsafe { IsWindowVisible(handle) == winapi::shared::minwindef::TRUE };

            if window_pid == process_id && is_visible {
                return;
            }
        }

        std::thread::sleep(std::time::Duration::from_millis(100));
    }
}

fn start_process(path: &str, arguments: &str) -> std::io::Result<()> {
    std::process::Command::new(path).arg(arguments).spawn()?;
    Ok(())
}

fn get_gta_path() -> Option<String> {
    // Steam Launcher
    if let Ok(value) = read_registry_value(
        RegKey::predef(HKEY_LOCAL_MACHINE),
        r"SOFTWARE\WOW6432Node\Rockstar Games\GTAV",
        "InstallFolderSteam",
    ) {
        return Some(value);
    }

    // Rockstar Games Launcher (formerly Social Club)
    if let Ok(value) = read_registry_value(
        RegKey::predef(HKEY_LOCAL_MACHINE),
        r"SOFTWARE\WOW6432Node\Rockstar Games\Grand Theft Auto V",
        "InstallFolder",
    ) {
        return Some(value);
    }

    // Rockstar Games Launcher fallback
    if let Ok(value) = read_registry_value(
        RegKey::predef(HKEY_LOCAL_MACHINE),
        r"SOFTWARE\WOW6432Node\Rockstar Games\Launcher",
        "InstallFolder",
    ) {
        let path = std::path::Path::new(&value);

        return path
            .parent()
            .map(|p| p.join(r"Grand Theft Auto V\GTAVLauncher.exe"))
            .and_then(|p| p.into_os_string().into_string().ok());
    }

    None
}

fn read_registry_value(registry_key: RegKey, key: &str, name: &str) -> std::io::Result<String> {
    let sub_key = registry_key.open_subkey(key)?;
    let value: String = sub_key.get_value(name)?;
    Ok(value)
}

fn write_registry_value(
    registry_key: RegKey,
    key: &str,
    name: &str,
    value: &str,
) -> std::io::Result<()> {
    let (sub_key, _) = registry_key.create_subkey(key)?;
    sub_key.set_value(name, &value)?;
    Ok(())
}
