#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

char g_log_path[MAX_PATH] = {};

struct Pattern {
    const char *name;
    const unsigned char *bytes;
    std::size_t size;
};

const unsigned char kPatchedFieldDecode[] = {
    0xE9, 0x5F, 0xE5, 0x0C, 0x00, 0x8A, 0x08, 0x81,
    0xF9, 0xE7, 0x00, 0x00, 0x00, 0x75, 0x43,
};

const unsigned char kOriginalFieldDecode[] = {
    0x8B, 0x45, 0x14, 0x33, 0xC9, 0x8A, 0x08, 0x81,
    0xF9, 0xE7, 0x00, 0x00, 0x00, 0x75, 0x43,
};

const unsigned char kResourcesFormat[] = {
    '%', 's', '/', 'r', 'e', 's', 'o', 'u', 'r', 'c', 'e', 's', '/',
    'f', 'f', '7', '_', '1', '.', '0', '2', '/', 'f', 'f', '7', '_',
    '%', 's',
};

const Pattern kPatterns[] = {
    {"patched ff7_ja field decode jump", kPatchedFieldDecode, sizeof(kPatchedFieldDecode)},
    {"original ff7_ja field decode code", kOriginalFieldDecode, sizeof(kOriginalFieldDecode)},
    {"FFVII resources/ff7_1.02 format string", kResourcesFormat, sizeof(kResourcesFormat)},
};

void log_line(const char *fmt, ...)
{
    FILE *f = nullptr;
    fopen_s(&f, g_log_path[0] ? g_log_path : "ff7kor-xaudio-proxy.log", "ab");
    if (!f) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputs("\r\n", f);
    fclose(f);
}

void init_log_path(HMODULE module)
{
    char module_path[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(module, module_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        lstrcpynA(g_log_path, "ff7kor-xaudio-proxy.log", MAX_PATH);
        return;
    }

    char *slash = strrchr(module_path, '\\');
    if (slash) {
        slash[1] = '\0';
        lstrcpynA(g_log_path, module_path, MAX_PATH);
        lstrcatA(g_log_path, "ff7kor-xaudio-proxy.log");
    } else {
        lstrcpynA(g_log_path, "ff7kor-xaudio-proxy.log", MAX_PATH);
    }
}

bool is_readable_page(DWORD protect)
{
    if (protect & PAGE_GUARD) {
        return false;
    }
    if (protect & PAGE_NOACCESS) {
        return false;
    }

    const DWORD base_protect = protect & 0xFF;
    return base_protect == PAGE_READONLY
        || base_protect == PAGE_READWRITE
        || base_protect == PAGE_WRITECOPY
        || base_protect == PAGE_EXECUTE_READ
        || base_protect == PAGE_EXECUTE_READWRITE
        || base_protect == PAGE_EXECUTE_WRITECOPY;
}

void scan_buffer_for_pattern(const unsigned char *buffer, std::size_t size,
                             std::uintptr_t base, const Pattern &pattern,
                             int &hits)
{
    if (size < pattern.size || hits >= 16) {
        return;
    }

    for (std::size_t i = 0; i + pattern.size <= size && hits < 16; ++i) {
        if (std::memcmp(buffer + i, pattern.bytes, pattern.size) == 0) {
            log_line("probe hit: %s at 0x%p", pattern.name,
                     reinterpret_cast<void *>(base + i));
            ++hits;
        }
    }
}

DWORD WINAPI probe_thread_proc(void *)
{
    Sleep(3000);
    log_line("probe started, pid=%lu", static_cast<unsigned long>(GetCurrentProcessId()));

    SYSTEM_INFO info = {};
    GetSystemInfo(&info);

    HANDLE process = GetCurrentProcess();
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
    const std::uintptr_t max_address =
        reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);

    int hits[sizeof(kPatterns) / sizeof(kPatterns[0])] = {};
    std::vector<unsigned char> buffer(1024 * 1024);

    while (address < max_address) {
        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<const void *>(address), &mbi, sizeof(mbi)) != sizeof(mbi)) {
            address += 0x10000;
            continue;
        }

        const std::uintptr_t region_base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const std::uintptr_t region_end = region_base + mbi.RegionSize;

        if (mbi.State == MEM_COMMIT && is_readable_page(mbi.Protect)) {
            for (std::uintptr_t pos = region_base; pos < region_end;) {
                const std::uintptr_t remaining = region_end - pos;
                const std::size_t chunk = static_cast<std::size_t>(
                    remaining < buffer.size() ? remaining : buffer.size());
                SIZE_T bytes_read = 0;
                if (ReadProcessMemory(process, reinterpret_cast<const void *>(pos),
                                      buffer.data(), chunk, &bytes_read) && bytes_read > 0) {
                    for (std::size_t i = 0; i < sizeof(kPatterns) / sizeof(kPatterns[0]); ++i) {
                        scan_buffer_for_pattern(buffer.data(), bytes_read, pos, kPatterns[i], hits[i]);
                    }
                }
                pos += chunk;
            }
        }

        address = region_end > address ? region_end : address + 0x10000;
    }

    for (std::size_t i = 0; i < sizeof(kPatterns) / sizeof(kPatterns[0]); ++i) {
        log_line("probe summary: %s hits=%d", kPatterns[i].name, hits[i]);
    }
    log_line("probe finished");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        init_log_path(module);
        log_line("ff7kor XAudio2 proxy loaded");

        HANDLE thread = CreateThread(nullptr, 0, probe_thread_proc, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        } else {
            log_line("CreateThread for probe failed: %lu", static_cast<unsigned long>(GetLastError()));
        }
    }
    return TRUE;
}
