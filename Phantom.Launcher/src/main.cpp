#include "launcher/injector.h"
#include "launcher/process_finder.h"
#include "launcher/registry_helper.h"
#include "shared/constants.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{

constexpr std::string_view GTA_EXECUTABLE = "GTA5.exe";
constexpr std::string_view CLIENT_DLL = "Phantom.Client.dll";

bool g_verbose = false;

void print_usage()
{
    std::cout << "Phantom Launcher v" << phantom::PHANTOM_VERSION << "\n\n"
              << "Usage:\n"
              << "  phantom_launcher.exe [options]\n\n"
              << "Options:\n"
              << "  --launch    Find GTA V via registry, launch it, and inject client DLL\n"
              << "  --attach    Attach to a running GTA V process and inject client DLL\n"
              << "  --verbose   Enable verbose output\n"
              << "  --help      Show this help message\n\n"
              << "If no options are provided, this usage information is displayed.\n";
}

void log_verbose(std::string_view message)
{
    if (g_verbose)
    {
        std::cout << "[verbose] " << message << "\n";
    }
}

std::filesystem::path get_dll_path()
{
    // Look for the client DLL next to the launcher executable
    auto exe_dir = std::filesystem::current_path();
    return exe_dir / CLIENT_DLL;
}

int do_launch()
{
    using namespace phantom::launcher;

    std::cout << "Phantom Launcher v" << phantom::PHANTOM_VERSION << "\n";
    std::cout << "Finding GTA V installation...\n";

    auto gta_path_result = RegistryHelper::find_gta_install_path();
    if (!gta_path_result.has_value())
    {
        std::cerr << "Error: " << registry_error_to_string(gta_path_result.error()) << "\n";
        return 1;
    }

    std::filesystem::path gta_dir = gta_path_result.value();
    std::filesystem::path gta_exe = gta_dir / GTA_EXECUTABLE;

    log_verbose(std::string("GTA V path: ") + gta_exe.string());

    if (!std::filesystem::exists(gta_exe))
    {
        std::cerr << "Error: GTA5.exe not found at " << gta_exe << "\n";
        return 1;
    }

    std::cout << "Launching GTA V...\n";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::string exe_path_str = gta_exe.string();
    if (!::CreateProcessA(exe_path_str.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr,
                          gta_dir.string().c_str(), &si, &pi))
    {
        std::cerr << "Error: Failed to launch GTA V (error " << ::GetLastError() << ")\n";
        return 1;
    }

    ::CloseHandle(pi.hThread);

    log_verbose("Waiting for process to initialize...");
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::filesystem::path dll_path = get_dll_path();
    std::cout << "Injecting " << CLIENT_DLL << "...\n";
    log_verbose(std::string("DLL path: ") + dll_path.string());

    auto inject_result = Injector::inject(pi.dwProcessId, dll_path);
    ::CloseHandle(pi.hProcess);

    if (!inject_result.has_value())
    {
        std::cerr << "Error: " << injection_error_to_string(inject_result.error()) << "\n";
        return 1;
    }

    std::cout << "Injection successful!\n";
    return 0;
}

int do_attach()
{
    using namespace phantom::launcher;

    std::cout << "Phantom Launcher v" << phantom::PHANTOM_VERSION << "\n";
    std::cout << "Searching for GTA V process...\n";

    auto pid_result = ProcessFinder::find_by_name(GTA_EXECUTABLE);
    if (!pid_result.has_value())
    {
        std::cerr << "Error: " << find_error_to_string(pid_result.error()) << "\n";
        std::cerr << "Make sure GTA V is running.\n";
        return 1;
    }

    DWORD pid = pid_result.value();
    std::cout << "Found GTA V (PID: " << pid << ")\n";

    std::filesystem::path dll_path = get_dll_path();
    std::cout << "Injecting " << CLIENT_DLL << "...\n";
    log_verbose(std::string("DLL path: ") + dll_path.string());

    auto inject_result = Injector::inject(pid, dll_path);
    if (!inject_result.has_value())
    {
        std::cerr << "Error: " << injection_error_to_string(inject_result.error()) << "\n";
        return 1;
    }

    std::cout << "Injection successful!\n";
    return 0;
}

} // anonymous namespace

int main(int argc, char** argv)
{
    std::vector<std::string_view> args(argv + 1, argv + argc);

    bool launch = false;
    bool attach = false;

    for (auto arg : args)
    {
        if (arg == "--verbose")
        {
            g_verbose = true;
        }
        else if (arg == "--launch")
        {
            launch = true;
        }
        else if (arg == "--attach")
        {
            attach = true;
        }
        else if (arg == "--help")
        {
            print_usage();
            return 0;
        }
    }

    if (!launch && !attach)
    {
        print_usage();
        return 0;
    }

    if (launch && attach)
    {
        std::cerr << "Error: --launch and --attach are mutually exclusive.\n";
        return 1;
    }

    if (launch)
    {
        return do_launch();
    }

    return do_attach();
}
