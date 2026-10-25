// Unpacks the Qt runtime stored at the end of this exe, then starts pokersoc_gui.exe.
// The export step appends: [this program][zip of the app]["POKERPK1"][uint64 zip size].

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr char kMagic[8] = {'P', 'O', 'K', 'E', 'R', 'P', 'K', '1'};

void fail(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"PokerSoc", MB_OK | MB_ICONERROR);
}

std::wstring exePath() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return path;
}

bool readPayload(const std::wstring& path, std::vector<char>& zip) {
    std::ifstream in(fs::path(path), std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff fileSize = in.tellg();
    if (fileSize < 16) return false;
    in.seekg(fileSize - 16);
    char magic[8];
    std::uint64_t zipSize = 0;
    in.read(magic, 8);
    in.read(reinterpret_cast<char*>(&zipSize), 8);
    if (!in || std::memcmp(magic, kMagic, 8) != 0) return false;
    if (zipSize == 0 || zipSize > static_cast<std::uint64_t>(fileSize - 16)) return false;
    zip.resize(static_cast<std::size_t>(zipSize));
    in.seekg(static_cast<std::streamoff>(fileSize - 16 - static_cast<std::streamoff>(zipSize)));
    in.read(zip.data(), static_cast<std::streamsize>(zip.size()));
    return static_cast<bool>(in);
}

fs::path runtimeDir() {
    wchar_t local[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    fs::path base = n > 0 && n < MAX_PATH ? fs::path(local) : fs::temp_directory_path();
    return base / "PokerSoc" / "runtime";
}

bool stampMatches(const fs::path& dir, std::uint64_t zipSize) {
    std::ifstream stamp(dir / ".stamp");
    std::uint64_t stored = 0;
    stamp >> stored;
    return stamp && stored == zipSize && fs::exists(dir / "pokersoc_gui.exe");
}

bool writeStamp(const fs::path& dir, std::uint64_t zipSize) {
    std::ofstream stamp(dir / ".stamp", std::ios::trunc);
    stamp << zipSize;
    return static_cast<bool>(stamp);
}

bool runAndWait(const std::wstring& command) {
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &startup, &process)) {
        return false;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code == 0;
}

bool extract(const fs::path& dir, const std::vector<char>& zip) {
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir, error);
    if (error) return false;

    const fs::path zipPath = fs::temp_directory_path() / "pokersoc-payload.zip";
    {
        std::ofstream out(zipPath, std::ios::binary | std::ios::trunc);
        out.write(zip.data(), static_cast<std::streamsize>(zip.size()));
        if (!out) return false;
    }

    wchar_t system[MAX_PATH];
    if (GetSystemDirectoryW(system, MAX_PATH) == 0) return false;
    const fs::path tar = fs::path(system) / "tar.exe";
    const std::wstring command = L"\"" + tar.wstring() + L"\" -xf \"" + zipPath.wstring() +
                                 L"\" -C \"" + dir.wstring() + L"\"";
    const bool extracted = runAndWait(command);
    fs::remove(zipPath, error);
    return extracted && fs::exists(dir / "pokersoc_gui.exe");
}

bool startApp(const fs::path& dir) {
    const std::wstring exe = (dir / "pokersoc_gui.exe").wstring();
    std::wstring command = L"\"" + exe + L"\"";
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::wstring work = dir.wstring();
    if (!CreateProcessW(exe.c_str(), buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, work.c_str(),
                        &startup, &process)) {
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    const std::wstring self = exePath();
    std::vector<char> zip;
    if (!readPayload(self, zip)) {
        fail(L"This program file is incomplete.");
        return 1;
    }
    const fs::path dir = runtimeDir();
    if (!stampMatches(dir, zip.size()) && !extract(dir, zip)) {
        fail(L"Couldn't unpack PokerSoc.");
        return 1;
    }
    if (!writeStamp(dir, zip.size())) {
        fail(L"Couldn't unpack PokerSoc.");
        return 1;
    }
    if (!startApp(dir)) {
        fail(L"Couldn't start PokerSoc.");
        return 1;
    }
    return 0;
}
