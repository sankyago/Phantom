#include "launcher/injector.h"
#include "hook/scoped_handle.h"

namespace phantom::launcher
{

const char* injection_error_to_string(InjectionError error)
{
    switch (error)
    {
    case InjectionError::ProcessOpenFailed:
        return "Failed to open target process";
    case InjectionError::AllocFailed:
        return "Failed to allocate memory in target process";
    case InjectionError::WriteFailed:
        return "Failed to write DLL path to target process";
    case InjectionError::ThreadCreationFailed:
        return "Failed to create remote thread";
    case InjectionError::InjectionTimeout:
        return "Injection timed out waiting for remote thread";
    case InjectionError::DllNotFound:
        return "DLL file not found";
    default:
        return "Unknown error";
    }
}

std::expected<void, InjectionError> Injector::inject(DWORD process_id, const std::filesystem::path& dll_path)
{
    // 1. Check that the DLL exists
    if (!std::filesystem::exists(dll_path))
    {
        return std::unexpected(InjectionError::DllNotFound);
    }

    std::string dll_path_str = dll_path.string();
    SIZE_T path_size = dll_path_str.size() + 1; // include null terminator

    // 2. Open the target process
    hook::ScopedHandle process{
        ::OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                          PROCESS_VM_WRITE | PROCESS_VM_READ,
                      FALSE, process_id)};
    if (!process)
    {
        return std::unexpected(InjectionError::ProcessOpenFailed);
    }

    // 3. Allocate memory in the target process for the DLL path
    LPVOID remote_memory =
        ::VirtualAllocEx(process.get(), nullptr, path_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_memory == nullptr)
    {
        return std::unexpected(InjectionError::AllocFailed);
    }

    // 4. Write the DLL path into the allocated memory
    SIZE_T bytes_written = 0;
    BOOL write_ok =
        ::WriteProcessMemory(process.get(), remote_memory, dll_path_str.c_str(), path_size, &bytes_written);
    if (!write_ok || bytes_written != path_size)
    {
        ::VirtualFreeEx(process.get(), remote_memory, 0, MEM_RELEASE);
        return std::unexpected(InjectionError::WriteFailed);
    }

    // 5. Get the address of LoadLibraryA in kernel32.dll
    FARPROC load_library_addr = ::GetProcAddress(::GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

    // 6. Create a remote thread that calls LoadLibraryA with our DLL path
    hook::ScopedHandle remote_thread{::CreateRemoteThread(
        process.get(), nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(load_library_addr),
        remote_memory, 0, nullptr)};
    if (!remote_thread)
    {
        ::VirtualFreeEx(process.get(), remote_memory, 0, MEM_RELEASE);
        return std::unexpected(InjectionError::ThreadCreationFailed);
    }

    // 7. Wait for the remote thread to complete (10 second timeout)
    DWORD wait_result = ::WaitForSingleObject(remote_thread.get(), 10000);

    // Cleanup allocated memory
    ::VirtualFreeEx(process.get(), remote_memory, 0, MEM_RELEASE);

    if (wait_result != WAIT_OBJECT_0)
    {
        return std::unexpected(InjectionError::InjectionTimeout);
    }

    return {};
}

} // namespace phantom::launcher
