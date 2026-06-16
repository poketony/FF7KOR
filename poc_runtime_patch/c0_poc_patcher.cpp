#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::wstring action = L"install";
    std::wstring process_name = L"FFVII.exe";
    DWORD pid = 0;
    DWORD wait_ms = 120000;
    bool suspend_threads = true;
    std::wstring log_path = L"poc_c000_patch.log";
};

struct PatchSite {
    const char* id;
    uint64_t rva;
    uint64_t full_signature_rva;
    std::vector<uint8_t> full_signature;
    std::vector<uint8_t> overwrite_original;
    size_t overwrite_len;
    uint64_t render_return_rva;
    uint64_t not_prefix_rva;
    uint64_t c0_continue_rva;
    enum class Kind { Render, Width } kind;
};

std::wofstream g_log;

void Log(const std::wstring& s) {
    std::wcout << s << L"\n";
    if (g_log.is_open()) {
        g_log << s << L"\n";
        g_log.flush();
    }
}

std::wstring Utf8ish(const char* s) {
    std::wstring out;
    while (*s) out.push_back(static_cast<unsigned char>(*s++));
    return out;
}

std::wstring Hex64(uint64_t v) {
    std::wostringstream oss;
    oss << L"0x" << std::uppercase << std::hex << v;
    return oss.str();
}

std::wstring HexBytes(const std::vector<uint8_t>& bytes) {
    std::wostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i) oss << L' ';
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0')
            << static_cast<unsigned>(bytes[i]);
    }
    return oss.str();
}

std::wstring HexBytes(const uint8_t* bytes, size_t size) {
    return HexBytes(std::vector<uint8_t>(bytes, bytes + size));
}

uint32_t ParseU32(const std::wstring& s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 0));
}

void AppendU64(std::vector<uint8_t>& out, uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xff));
}

void AppendAbsJmp(std::vector<uint8_t>& out, uint64_t target) {
    out.push_back(0xff);
    out.push_back(0x25);
    out.push_back(0x00);
    out.push_back(0x00);
    out.push_back(0x00);
    out.push_back(0x00);
    AppendU64(out, target);
}

std::vector<uint8_t> MakeAbsDetour(uint64_t target, size_t overwrite_len) {
    std::vector<uint8_t> out;
    AppendAbsJmp(out, target);
    while (out.size() < overwrite_len) out.push_back(0x90);
    return out;
}

std::vector<uint8_t> MakeRenderStub(uint64_t module_base) {
    const uint64_t return_to_shl = module_base + 0x1572585;
    const uint64_t not_prefix = module_base + 0x157258e;
    const uint64_t loop_continue = module_base + 0x15726d7;
    std::vector<uint8_t> b = {
        0x80, 0xf9, 0xc0,             // cmp cl,0xC0
        0x74, 0x2a,                   // je c0
        0x8d, 0x41, 0x06,             // lea eax,[rcx+6]
        0x44, 0x0f, 0xb7, 0xc9,       // movzx r9d,cx
        0x3c, 0x04,                   // cmp al,4
        0x77, 0x11,                   // ja not_prefix
        0x0f, 0xb7, 0xd9              // movzx ebx,cx
    };
    AppendAbsJmp(b, return_to_shl);
    AppendAbsJmp(b, not_prefix);
    b.push_back(0xbb);                // mov ebx,0xFA00
    b.push_back(0x00);
    b.push_back(0xfa);
    b.push_back(0x00);
    b.push_back(0x00);
    AppendAbsJmp(b, loop_continue);
    return b;
}

std::vector<uint8_t> MakeWidthStub(uint64_t module_base) {
    const uint64_t not_prefix = module_base + 0x157134c;
    const uint64_t loop_continue = module_base + 0x157144f;
    std::vector<uint8_t> b = {
        0x3c, 0xc0,                   // cmp al,0xC0
        0x74, 0x2d,                   // je c0
        0x8d, 0x48, 0x06,             // lea ecx,[rax+6]
        0x80, 0xf9, 0x04,             // cmp cl,4
        0x77, 0x17,                   // ja not_prefix
        0x44, 0x0f, 0xb7, 0xc0,       // movzx r8d,ax
        0x66, 0x41, 0xc1, 0xe0, 0x08  // shl r8w,8
    };
    AppendAbsJmp(b, loop_continue);
    AppendAbsJmp(b, not_prefix);
    b.push_back(0x41);                // mov r8d,0xFA00
    b.push_back(0xb8);
    b.push_back(0x00);
    b.push_back(0xfa);
    b.push_back(0x00);
    b.push_back(0x00);
    AppendAbsJmp(b, loop_continue);
    return b;
}

std::vector<PatchSite> Sites() {
    return {
        {
            "common_render_scanner_prefix",
            0x1572577,
            0x1572577,
            {0x8d,0x41,0x06,0x44,0x0f,0xb7,0xc9,0x3c,0x04,0x77,0x0c,0x0f,
             0xb7,0xd9,0x66,0xc1,0xe3,0x08,0xe9,0x49,0x01,0x00,0x00},
            {0x8d,0x41,0x06,0x44,0x0f,0xb7,0xc9,0x3c,0x04,0x77,0x0c,0x0f,0xb7,0xd9},
            14,
            0x1572585,
            0x157258e,
            0x15726d7,
            PatchSite::Kind::Render
        },
        {
            "common_width_scanner_prefix",
            0x1571336,
            0x1571336,
            {0x8d,0x48,0x06,0x80,0xf9,0x04,0x77,0x0e,0x44,0x0f,0xb7,0xc0,
             0x66,0x41,0xc1,0xe0,0x08,0xe9,0x03,0x01,0x00,0x00},
            {0x8d,0x48,0x06,0x80,0xf9,0x04,0x77,0x0e,0x44,0x0f,0xb7,0xc0,0x66,0x41,0xc1,0xe0,0x08},
            17,
            0,
            0x157134c,
            0x157144f,
            PatchSite::Kind::Width
        }
    };
}

bool ReadMem(HANDLE process, uint64_t address, std::vector<uint8_t>& out, size_t size) {
    out.assign(size, 0);
    SIZE_T read = 0;
    return ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), out.data(), size, &read) &&
           read == size;
}

bool WriteMem(HANDLE process, uint64_t address, const std::vector<uint8_t>& bytes) {
    SIZE_T written = 0;
    return WriteProcessMemory(process, reinterpret_cast<LPVOID>(address), bytes.data(), bytes.size(), &written) &&
           written == bytes.size();
}

DWORD FindProcessByName(const std::wstring& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    DWORD found = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name.c_str()) == 0) {
                found = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

uint64_t FindModuleBase(DWORD pid, const std::wstring& module_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    uint64_t base = 0;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, module_name.c_str()) == 0) {
                base = reinterpret_cast<uint64_t>(me.modBaseAddr);
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

struct SuspendedThreads {
    std::vector<HANDLE> handles;
    explicit SuspendedThreads(DWORD pid, bool enabled) {
        if (!enabled) return;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return;
        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) {
                    HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                    if (th) {
                        if (SuspendThread(th) != static_cast<DWORD>(-1)) {
                            handles.push_back(th);
                        } else {
                            CloseHandle(th);
                        }
                    }
                }
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
        Log(L"suspended target threads: " + std::to_wstring(handles.size()));
    }
    ~SuspendedThreads() {
        for (HANDLE th : handles) {
            ResumeThread(th);
            CloseHandle(th);
        }
        if (!handles.empty()) Log(L"resumed target threads");
    }
};

bool ProtectWriteFlush(HANDLE process, uint64_t address, const std::vector<uint8_t>& bytes) {
    DWORD old_protect = 0;
    if (!VirtualProtectEx(process, reinterpret_cast<LPVOID>(address), bytes.size(),
                          PAGE_EXECUTE_READWRITE, &old_protect)) {
        Log(L"VirtualProtectEx failed at " + Hex64(address) + L" error=" + std::to_wstring(GetLastError()));
        return false;
    }
    bool ok = WriteMem(process, address, bytes);
    DWORD ignored = 0;
    VirtualProtectEx(process, reinterpret_cast<LPVOID>(address), bytes.size(), old_protect, &ignored);
    FlushInstructionCache(process, reinterpret_cast<LPCVOID>(address), bytes.size());
    if (!ok) {
        Log(L"WriteProcessMemory failed at " + Hex64(address) + L" error=" + std::to_wstring(GetLastError()));
    }
    return ok;
}

bool WaitForRuntimeBytes(HANDLE process, uint64_t module_base, const std::vector<PatchSite>& sites,
                         DWORD wait_ms) {
    const DWORD step_ms = 500;
    DWORD waited = 0;
    while (true) {
        bool all_match = true;
        for (const auto& site : sites) {
            std::vector<uint8_t> got;
            uint64_t addr = module_base + site.full_signature_rva;
            if (!ReadMem(process, addr, got, site.full_signature.size()) || got != site.full_signature) {
                all_match = false;
                if (waited == 0 || waited + step_ms >= wait_ms) {
                    Log(L"signature not ready for " + Utf8ish(site.id) + L" at " + Hex64(addr));
                    if (!got.empty()) Log(L"  saw: " + HexBytes(got));
                    Log(L"  expected: " + HexBytes(site.full_signature));
                }
                break;
            }
        }
        if (all_match) return true;
        if (waited >= wait_ms) return false;
        Sleep(step_ms);
        waited += step_ms;
    }
}

bool Install(HANDLE process, DWORD pid, uint64_t module_base, bool suspend_threads) {
    auto sites = Sites();
    SuspendedThreads suspended(pid, suspend_threads);
    bool all_ok = true;
    bool wrote_any_detour = false;
    for (const auto& site : sites) {
        uint64_t target = module_base + site.rva;
        std::vector<uint8_t> got;
        if (!ReadMem(process, target, got, site.full_signature.size()) || got != site.full_signature) {
            Log(L"abort: original full signature mismatch for " + Utf8ish(site.id));
            all_ok = false;
            continue;
        }

        std::vector<uint8_t> stub =
            site.kind == PatchSite::Kind::Render ? MakeRenderStub(module_base) : MakeWidthStub(module_base);
        LPVOID remote_stub = VirtualAllocEx(process, nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remote_stub) {
            Log(L"VirtualAllocEx stub failed for " + Utf8ish(site.id) + L" error=" + std::to_wstring(GetLastError()));
            all_ok = false;
            continue;
        }
        uint64_t stub_addr = reinterpret_cast<uint64_t>(remote_stub);
        if (!WriteMem(process, stub_addr, stub)) {
            Log(L"stub write failed for " + Utf8ish(site.id));
            all_ok = false;
            continue;
        }
        DWORD old_protect = 0;
        if (!VirtualProtectEx(process, remote_stub, stub.size(), PAGE_EXECUTE_READ, &old_protect)) {
            Log(L"VirtualProtectEx stub failed for " + Utf8ish(site.id) +
                L" error=" + std::to_wstring(GetLastError()));
            all_ok = false;
            continue;
        }
        FlushInstructionCache(process, remote_stub, stub.size());

        std::vector<uint8_t> detour = MakeAbsDetour(stub_addr, site.overwrite_len);
        Log(L"install " + Utf8ish(site.id));
        Log(L"  target=" + Hex64(target) + L" rva=" + Hex64(site.rva));
        Log(L"  original overwrite bytes=" + HexBytes(site.overwrite_original));
        Log(L"  stub=" + Hex64(stub_addr) + L" stub bytes=" + HexBytes(stub));
        Log(L"  detour bytes=" + HexBytes(detour));

        if (!ProtectWriteFlush(process, target, detour)) {
            all_ok = false;
            continue;
        }
        wrote_any_detour = true;
        std::vector<uint8_t> verify;
        if (!ReadMem(process, target, verify, detour.size()) || verify != detour) {
            Log(L"post-write verification failed for " + Utf8ish(site.id));
            all_ok = false;
        } else {
            Log(L"  patch succeeded");
        }
    }
    if (!all_ok && wrote_any_detour) {
        Log(L"install failed after at least one write; restoring mandatory overwrite bytes");
        for (const auto& site : sites) {
            ProtectWriteFlush(process, module_base + site.rva, site.overwrite_original);
        }
    }
    return all_ok;
}

bool Restore(HANDLE process, DWORD pid, uint64_t module_base, bool suspend_threads) {
    auto sites = Sites();
    SuspendedThreads suspended(pid, suspend_threads);
    bool all_ok = true;
    for (const auto& site : sites) {
        uint64_t target = module_base + site.rva;
        Log(L"restore " + Utf8ish(site.id) + L" at " + Hex64(target));
        if (!ProtectWriteFlush(process, target, site.overwrite_original)) {
            all_ok = false;
            continue;
        }
        std::vector<uint8_t> verify;
        if (!ReadMem(process, target, verify, site.overwrite_original.size()) || verify != site.overwrite_original) {
            Log(L"restore verification failed for " + Utf8ish(site.id));
            all_ok = false;
        } else {
            Log(L"  restored original overwrite bytes");
        }
    }
    Log(L"note: restore disables detours; remote stub allocations remain inert until FFVII.exe exits");
    return all_ok;
}

void PrintUsage() {
    std::wcout
        << L"usage: c0_poc_patcher.exe install|restore [--pid N] [--process FFVII.exe]\\n"
        << L"                              [--wait-ms N] [--log path] [--no-suspend]\\n";
}

Options ParseArgs(int argc, wchar_t** argv) {
    Options opt;
    if (argc >= 2) opt.action = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--pid" && i + 1 < argc) opt.pid = ParseU32(argv[++i]);
        else if (a == L"--process" && i + 1 < argc) opt.process_name = argv[++i];
        else if (a == L"--wait-ms" && i + 1 < argc) opt.wait_ms = ParseU32(argv[++i]);
        else if (a == L"--log" && i + 1 < argc) opt.log_path = argv[++i];
        else if (a == L"--no-suspend") opt.suspend_threads = false;
        else {
            PrintUsage();
            ExitProcess(2);
        }
    }
    return opt;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    Options opt = ParseArgs(argc, argv);
    g_log.open(opt.log_path.c_str(), std::ios::app);
    Log(L"==== c0_poc_patcher " + opt.action + L" ====");

    DWORD pid = opt.pid ? opt.pid : FindProcessByName(opt.process_name);
    if (!pid) {
        Log(L"target process not found: " + opt.process_name);
        return 1;
    }
    Log(L"process id: " + std::to_wstring(pid));

    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                                     PROCESS_VM_OPERATION,
                                 FALSE, pid);
    if (!process) {
        Log(L"OpenProcess failed error=" + std::to_wstring(GetLastError()));
        return 1;
    }

    uint64_t module_base = FindModuleBase(pid, opt.process_name);
    if (!module_base) {
        Log(L"module base not found for " + opt.process_name);
        CloseHandle(process);
        return 1;
    }
    Log(L"module base: " + Hex64(module_base));

    bool ok = false;
    if (_wcsicmp(opt.action.c_str(), L"install") == 0) {
        Log(L"waiting for runtime-decrypted mandatory signatures, wait_ms=" + std::to_wstring(opt.wait_ms));
        if (!WaitForRuntimeBytes(process, module_base, Sites(), opt.wait_ms)) {
            Log(L"abort: mandatory runtime signatures did not match before timeout");
            CloseHandle(process);
            return 1;
        }
        Log(L"runtime signatures validated");
        ok = Install(process, pid, module_base, opt.suspend_threads);
    } else if (_wcsicmp(opt.action.c_str(), L"restore") == 0) {
        ok = Restore(process, pid, module_base, opt.suspend_threads);
    } else {
        PrintUsage();
        CloseHandle(process);
        return 2;
    }

    CloseHandle(process);
    Log(ok ? L"completed successfully" : L"completed with errors");
    return ok ? 0 : 1;
}
