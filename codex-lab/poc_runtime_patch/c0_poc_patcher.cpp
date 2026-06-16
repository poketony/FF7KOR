#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kGlobalFontStructPtrRva = 0x207CE08;
constexpr uint64_t kGlobalFontStructHandleRva = 0x207CDEC;
constexpr uint64_t kGlobalVmStackPtrRva = 0x20395C8;
constexpr uint32_t kFontFilenameScratchOffset = 0xB8;
constexpr size_t kFontFilenameScratchSize = 0x80;

struct Options {
    std::wstring action = L"install";
    std::wstring process_name = L"FFVII.exe";
    std::wstring font_path;
    std::string native_font_name = "korean_c0_page.tim";
    DWORD pid = 0;
    DWORD wait_ms = 120000;
    bool suspend_threads = true;
    std::wstring log_path = L"first_korean_glyph_patch.log";
};

enum class PatchKind {
    CommonRender,
    CommonWidth,
    FieldRender,
    FieldLayout,
    GlyphRenderer,
    FontLoaderHook,
};

struct PatchSite {
    const char* id;
    uint64_t rva;
    std::vector<uint8_t> full_signature;
    std::vector<uint8_t> overwrite_original;
    size_t overwrite_len;
    PatchKind kind;
};

struct RemoteStateLocal {
    uint32_t korean_c0_handle;
    uint32_t filename_handle;
    uint32_t direct_arg0;
    uint32_t direct_arg1;
    uint32_t direct_arg2;
    uint32_t direct_arg3;
    uint32_t saved_vm_stack;
    uint32_t direct_result;
    uint32_t direct_status;
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

std::wstring WidenAscii(const std::string& s) {
    std::wstring out;
    for (char c : s) out.push_back(static_cast<unsigned char>(c));
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

uint32_t ParseU32(const std::wstring& s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 0));
}

std::wstring SelfDirectory() {
    std::vector<wchar_t> buf(MAX_PATH);
    DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    while (len == buf.size()) {
        buf.resize(buf.size() * 2);
        len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    }
    std::wstring path(buf.data(), len);
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

bool FileExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring ParentDirectory(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    wchar_t last = a.back();
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring DefaultFontPath() {
    std::wstring dir = SelfDirectory();
    std::wstring packaged = JoinPath(dir, L"resources\\korean_font\\korean_c0_page.tim");
    if (FileExists(packaged)) return packaged;
    return JoinPath(dir, L"korean_c0_page.tim");
}

std::wstring FullPath(const std::wstring& path) {
    DWORD need = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!need) return path;
    std::vector<wchar_t> buf(need + 1);
    DWORD got = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buf.size()), buf.data(), nullptr);
    if (!got) return path;
    return std::wstring(buf.data(), got);
}

std::wstring ReplaceExtension(const std::wstring& path, const std::wstring& extension) {
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return path + extension;
    }
    return path.substr(0, dot) + extension;
}

bool GetProcessImagePath(HANDLE process, std::wstring* out) {
    std::vector<wchar_t> buf(MAX_PATH);
    DWORD size = static_cast<DWORD>(buf.size());
    while (true) {
        if (QueryFullProcessImageNameW(process, 0, buf.data(), &size)) {
            *out = std::wstring(buf.data(), size);
            return true;
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return false;
        buf.resize(buf.size() * 2);
        size = static_cast<DWORD>(buf.size());
    }
}

void AppendU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xff));
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

void AppendMovRaxImm64(std::vector<uint8_t>& out, uint64_t value) {
    out.push_back(0x48);
    out.push_back(0xB8);
    AppendU64(out, value);
}

void AppendMovRdxImm64(std::vector<uint8_t>& out, uint64_t value) {
    out.push_back(0x48);
    out.push_back(0xBA);
    AppendU64(out, value);
}

void AppendMovR10Imm64(std::vector<uint8_t>& out, uint64_t value) {
    out.push_back(0x49);
    out.push_back(0xBA);
    AppendU64(out, value);
}

size_t AppendJcc8(std::vector<uint8_t>& out, uint8_t opcode) {
    out.push_back(opcode);
    out.push_back(0x00);
    return out.size() - 1;
}

void PatchJcc8(std::vector<uint8_t>& out, size_t disp_pos, size_t target_pos) {
    const int delta = static_cast<int>(target_pos) - static_cast<int>(disp_pos + 1);
    if (delta < -128 || delta > 127) {
        throw std::runtime_error("short conditional jump target out of range");
    }
    out[disp_pos] = static_cast<uint8_t>(delta & 0xff);
}

std::vector<uint8_t> MakeAbsDetour(uint64_t target, size_t overwrite_len) {
    std::vector<uint8_t> out;
    AppendAbsJmp(out, target);
    while (out.size() < overwrite_len) out.push_back(0x90);
    return out;
}

std::vector<uint8_t> MakeCommonRenderStub(uint64_t module_base) {
    const uint64_t return_to_shl = module_base + 0x1572585;
    const uint64_t not_prefix = module_base + 0x157258e;
    const uint64_t loop_continue = module_base + 0x15726d7;
    std::vector<uint8_t> b = {
        0x80, 0xf9, 0xc0,             // cmp cl,0xC0
    };
    size_t jb_original = AppendJcc8(b, 0x72);
    b.insert(b.end(), {0x80, 0xf9, 0xcc}); // cmp cl,0xCC
    size_t ja_original = AppendJcc8(b, 0x77);
    b.insert(b.end(), {
        0x0f, 0xb6, 0xd9,             // movzx ebx,cl
        0x66, 0xc1, 0xe3, 0x08        // shl bx,8
    });
    AppendAbsJmp(b, loop_continue);
    size_t original = b.size();
    b.insert(b.end(), {
        0x8d, 0x41, 0x06,             // lea eax,[rcx+6]
        0x44, 0x0f, 0xb7, 0xc9,       // movzx r9d,cx
        0x3c, 0x04                    // cmp al,4
    });
    size_t ja_not_prefix = AppendJcc8(b, 0x77);
    b.insert(b.end(), {0x0f, 0xb7, 0xd9}); // movzx ebx,cx
    AppendAbsJmp(b, return_to_shl);
    size_t not_prefix_label = b.size();
    AppendAbsJmp(b, not_prefix);
    PatchJcc8(b, jb_original, original);
    PatchJcc8(b, ja_original, original);
    PatchJcc8(b, ja_not_prefix, not_prefix_label);
    return b;
}

std::vector<uint8_t> MakeCommonWidthStub(uint64_t module_base) {
    const uint64_t not_prefix = module_base + 0x157134c;
    const uint64_t loop_continue = module_base + 0x157144f;
    std::vector<uint8_t> b = {
        0x3c, 0xc0                    // cmp al,0xC0
    };
    size_t jb_original = AppendJcc8(b, 0x72);
    b.insert(b.end(), {0x3c, 0xcc});  // cmp al,0xCC
    size_t ja_original = AppendJcc8(b, 0x77);
    b.insert(b.end(), {
        0x44, 0x0f, 0xb6, 0xc0,       // movzx r8d,al
        0x66, 0x41, 0xc1, 0xe0, 0x08  // shl r8w,8
    });
    AppendAbsJmp(b, loop_continue);
    size_t original = b.size();
    b.insert(b.end(), {
        0x8d, 0x48, 0x06,             // lea ecx,[rax+6]
        0x80, 0xf9, 0x04              // cmp cl,4
    });
    size_t ja_not_prefix = AppendJcc8(b, 0x77);
    b.insert(b.end(), {
        0x44, 0x0f, 0xb7, 0xc0,       // movzx r8d,ax
        0x66, 0x41, 0xc1, 0xe0, 0x08  // shl r8w,8
    });
    AppendAbsJmp(b, loop_continue);
    size_t not_prefix_label = b.size();
    AppendAbsJmp(b, not_prefix);
    PatchJcc8(b, jb_original, original);
    PatchJcc8(b, ja_original, original);
    PatchJcc8(b, ja_not_prefix, not_prefix_label);
    return b;
}

std::vector<uint8_t> MakeFieldRenderStub(uint64_t module_base) {
    const uint64_t not_prefix = module_base + 0x156f9ab;
    const uint64_t loop_continue = module_base + 0x156fc1a;
    std::vector<uint8_t> b = {
        0x80, 0xf9, 0xc0,             // cmp cl,0xC0
    };
    size_t jb_original = AppendJcc8(b, 0x72);
    b.insert(b.end(), {0x80, 0xf9, 0xcc}); // cmp cl,0xCC
    size_t ja_original = AppendJcc8(b, 0x77);
    b.insert(b.end(), {
        0x44, 0x0f, 0xb6, 0xf1,       // movzx r14d,cl
        0x66, 0x41, 0xc1, 0xe6, 0x08  // shl r14w,8
    });
    AppendAbsJmp(b, loop_continue);
    size_t original = b.size();
    b.insert(b.end(), {
        0x8d, 0x41, 0x06,             // lea eax,[rcx+6]
        0x3c, 0x04                    // cmp al,4
    });
    size_t ja_not_prefix = AppendJcc8(b, 0x77);
    b.insert(b.end(), {
        0x44, 0x0f, 0xb7, 0xf1,       // movzx r14d,cx
        0x66, 0x41, 0xc1, 0xe6, 0x08  // shl r14w,8
    });
    AppendAbsJmp(b, loop_continue);
    size_t not_prefix_label = b.size();
    AppendAbsJmp(b, not_prefix);
    PatchJcc8(b, jb_original, original);
    PatchJcc8(b, ja_original, original);
    PatchJcc8(b, ja_not_prefix, not_prefix_label);
    return b;
}

std::vector<uint8_t> MakeFieldLayoutStub(uint64_t module_base) {
    const uint64_t not_prefix = module_base + 0x15716e7;
    const uint64_t loop_continue = module_base + 0x157193f;
    std::vector<uint8_t> b = {
        0x80, 0xfb, 0xc0,             // cmp bl,0xC0
    };
    size_t jb_original = AppendJcc8(b, 0x72);
    b.insert(b.end(), {0x80, 0xfb, 0xcc}); // cmp bl,0xCC
    size_t ja_original = AppendJcc8(b, 0x77);
    b.insert(b.end(), {
        0x0f, 0xb6, 0xf3,             // movzx esi,bl
        0x66, 0xc1, 0xe6, 0x08        // shl si,8
    });
    AppendAbsJmp(b, loop_continue);
    size_t original = b.size();
    b.insert(b.end(), {
        0x8d, 0x43, 0x06,             // lea eax,[rbx+6]
        0x3c, 0x04                    // cmp al,4
    });
    size_t ja_not_prefix = AppendJcc8(b, 0x77);
    b.insert(b.end(), {
        0x0f, 0xb7, 0xf3,             // movzx esi,bx
        0x66, 0xc1, 0xe6, 0x08        // shl si,8
    });
    AppendAbsJmp(b, loop_continue);
    size_t not_prefix_label = b.size();
    AppendAbsJmp(b, not_prefix);
    PatchJcc8(b, jb_original, original);
    PatchJcc8(b, ja_original, original);
    PatchJcc8(b, ja_not_prefix, not_prefix_label);
    return b;
}

std::vector<uint8_t> MakeGlyphRendererStub(uint64_t module_base, uint64_t remote_state) {
    const uint64_t original_branch = module_base + 0x15720c8;
    const uint64_t common_handle_test = module_base + 0x1572131;
    const uint64_t default_return = module_base + 0x157233d;
    std::vector<uint8_t> b = {
        0x41, 0x0f, 0xb7, 0xc0,                         // movzx eax,r8w
        0x4c, 0x89, 0xbc, 0x24, 0xe0, 0x00, 0x00, 0x00,  // mov [rsp+0xe0],r15
        0x3d, 0xc0, 0x00, 0x00, 0x00                    // cmp eax,0xC0
    };
    size_t jne_original = AppendJcc8(b, 0x75);
    AppendMovRaxImm64(b, remote_state);
    b.insert(b.end(), {
        0x8b, 0x38,                                      // mov edi,[rax]
        0x85, 0xff                                       // test edi,edi
    });
    size_t jz_default = AppendJcc8(b, 0x74);
    AppendAbsJmp(b, common_handle_test);
    size_t original = b.size();
    b.insert(b.end(), {0x3d, 0xfe, 0x00, 0x00, 0x00});   // cmp eax,0xFE
    AppendAbsJmp(b, original_branch);
    size_t default_label = b.size();
    AppendAbsJmp(b, default_return);
    PatchJcc8(b, jne_original, original);
    PatchJcc8(b, jz_default, default_label);
    return b;
}

void AppendReadVmArgToState(std::vector<uint8_t>& b, uint64_t module_base, uint64_t remote_state,
                            uint8_t vm_offset, uint8_t state_offset) {
    AppendMovRdxImm64(b, remote_state);
    b.insert(b.end(), {
        0x8b, 0x4a, 0x18,        // mov ecx,[rdx+0x18] ; saved DAT_1420395C8
        0x83, 0xc1, vm_offset    // add ecx,vm_offset
    });
    AppendMovRaxImm64(b, module_base + 0x3f0a0);
    b.insert(b.end(), {
        0xff, 0xd0,              // call rax ; FUN_14003F0A0
        0x31, 0xc9,              // xor ecx,ecx
        0x48, 0x85, 0xc0,        // test rax,rax
        0x74, 0x02,              // jz +2
        0x8b, 0x08               // mov ecx,[rax]
    });
    AppendMovRdxImm64(b, remote_state);
    b.insert(b.end(), {
        0x89, 0x4a, state_offset // mov [rdx+state_offset],ecx
    });
}

std::vector<uint8_t> MakeDirectFontLoadStub(uint64_t module_base, uint64_t remote_state) {
    const uint64_t vm_stack_ptr = module_base + kGlobalVmStackPtrRva;
    const uint64_t load_command = module_base + 0x4ab00;
    std::vector<uint8_t> b = {
        0x48, 0x83, 0xec, 0x48   // sub rsp,0x48 ; shadow space + alignment
    };

    AppendMovRdxImm64(b, remote_state);
    b.insert(b.end(), {
        0xc7, 0x42, 0x20, 0x01, 0x00, 0x00, 0x00 // mov dword ptr [rdx+0x20],1
    });
    AppendMovRaxImm64(b, vm_stack_ptr);
    b.insert(b.end(), {
        0x8b, 0x08,              // mov ecx,[rax] ; DAT_1420395C8
    });
    AppendMovRdxImm64(b, remote_state);
    b.insert(b.end(), {
        0x89, 0x4a, 0x18         // mov [rdx+0x18],ecx
    });

    AppendReadVmArgToState(b, module_base, remote_state, 0x04, 0x08);
    AppendReadVmArgToState(b, module_base, remote_state, 0x08, 0x0c);
    AppendReadVmArgToState(b, module_base, remote_state, 0x0c, 0x10);
    AppendReadVmArgToState(b, module_base, remote_state, 0x14, 0x14);

    AppendMovR10Imm64(b, remote_state);
    b.insert(b.end(), {
        0x45, 0x8b, 0x42, 0x08,  // mov r8d,[r10+0x08]
        0x45, 0x8b, 0x4a, 0x0c,  // mov r9d,[r10+0x0c]
        0x41, 0x8b, 0x42, 0x10,  // mov eax,[r10+0x10]
        0x89, 0x44, 0x24, 0x20,  // mov [rsp+0x20],eax
        0x41, 0x8b, 0x42, 0x04,  // mov eax,[r10+0x04] ; filename handle
        0x89, 0x44, 0x24, 0x28,  // mov [rsp+0x28],eax
        0x41, 0x8b, 0x42, 0x14,  // mov eax,[r10+0x14]
        0x89, 0x44, 0x24, 0x30,  // mov [rsp+0x30],eax
        0xba, 0x05, 0x00, 0x00, 0x00, // mov edx,5
        0xb9, 0xac, 0x10, 0x67, 0x00  // mov ecx,0x6710AC
    });
    AppendMovRaxImm64(b, load_command);
    b.insert(b.end(), {
        0xff, 0xd0               // call rax
    });

    AppendMovRdxImm64(b, remote_state);
    b.insert(b.end(), {
        0x89, 0x02,              // mov [rdx],eax
        0x89, 0x42, 0x1c,        // mov [rdx+0x1c],eax
        0xc7, 0x42, 0x20, 0x02, 0x00, 0x00, 0x00, // mov dword ptr [rdx+0x20],2
        0x8b, 0x4a, 0x18         // mov ecx,[rdx+0x18]
    });
    AppendMovRaxImm64(b, vm_stack_ptr);
    b.insert(b.end(), {
        0x89, 0x08               // mov [rax],ecx ; restore DAT_1420395C8
    });
    AppendMovRdxImm64(b, remote_state);
    b.insert(b.end(), {
        0x8b, 0x02,              // mov eax,[rdx]
        0x48, 0x83, 0xc4, 0x48,  // add rsp,0x48
        0xc3                     // ret
    });
    return b;
}

std::vector<uint8_t> MakeFontLoaderHookStub(uint64_t module_base, uint64_t remote_state) {
    const uint64_t load_command = module_base + 0x4ab00;
    const uint64_t vm_stack_ptr = module_base + kGlobalVmStackPtrRva;
    const uint64_t epilogue_continue = module_base + 0x156e10f;
    std::vector<uint8_t> b;
    AppendMovRaxImm64(b, remote_state);
    b.insert(b.end(), {0x83, 0x38, 0x00});              // cmp dword ptr [rax],0
    size_t jne_skip_load = AppendJcc8(b, 0x75);
    AppendMovRdxImm64(b, remote_state);
    AppendMovRaxImm64(b, vm_stack_ptr);
    b.insert(b.end(), {
        0x8b, 0x08,                                      // mov ecx,[rax]
        0x89, 0x4a, 0x18                                 // mov [rdx+0x18],ecx
    });
    AppendMovRaxImm64(b, remote_state);
    b.insert(b.end(), {
        0x44, 0x8b, 0xce,                                // mov r9d,esi
        0x44, 0x8b, 0xc7,                                // mov r8d,edi
        0xba, 0x05, 0x00, 0x00, 0x00,                    // mov edx,5
        0xb9, 0xac, 0x10, 0x67, 0x00,                    // mov ecx,0x6710ac
        0x89, 0x6c, 0x24, 0x20,                          // mov [rsp+0x20],ebp
        0x8b, 0x40, 0x04,                                // mov eax,[rax+4]
        0x89, 0x44, 0x24, 0x28,                          // mov [rsp+0x28],eax
        0x89, 0x5c, 0x24, 0x30                           // mov [rsp+0x30],ebx
    });
    AppendMovR10Imm64(b, load_command);
    b.insert(b.end(), {0x41, 0xff, 0xd2});               // call r10
    AppendMovRdxImm64(b, remote_state);
    b.insert(b.end(), {
        0x89, 0x02,                                      // mov [rdx],eax
        0x89, 0x42, 0x1c,                                // mov [rdx+0x1c],eax
        0x8b, 0x4a, 0x18                                 // mov ecx,[rdx+0x18]
    });
    AppendMovRaxImm64(b, vm_stack_ptr);
    b.insert(b.end(), {0x89, 0x08});                     // mov [rax],ecx
    size_t skip_load = b.size();
    b.insert(b.end(), {
        0x48, 0x8b, 0x6c, 0x24, 0x58,                    // mov rbp,[rsp+0x58]
        0x48, 0x8b, 0x74, 0x24, 0x60,                    // mov rsi,[rsp+0x60]
        0x48, 0x8b, 0x7c, 0x24, 0x40                     // mov rdi,[rsp+0x40]
    });
    AppendAbsJmp(b, epilogue_continue);
    PatchJcc8(b, jne_skip_load, skip_load);
    return b;
}

std::vector<PatchSite> Sites() {
    return {
        {
            "common_render_scanner_prefix",
            0x1572577,
            {0x8d,0x41,0x06,0x44,0x0f,0xb7,0xc9,0x3c,0x04,0x77,0x0c,0x0f,
             0xb7,0xd9,0x66,0xc1,0xe3,0x08,0xe9,0x49,0x01,0x00,0x00},
            {0x8d,0x41,0x06,0x44,0x0f,0xb7,0xc9,0x3c,0x04,0x77,0x0c,0x0f,0xb7,0xd9},
            14,
            PatchKind::CommonRender
        },
        {
            "common_width_scanner_prefix",
            0x1571336,
            {0x8d,0x48,0x06,0x80,0xf9,0x04,0x77,0x0e,0x44,0x0f,0xb7,0xc0,
             0x66,0x41,0xc1,0xe0,0x08,0xe9,0x03,0x01,0x00,0x00},
            {0x8d,0x48,0x06,0x80,0xf9,0x04,0x77,0x0e,0x44,0x0f,0xb7,0xc0,0x66,0x41,0xc1,0xe0,0x08},
            17,
            PatchKind::CommonWidth
        },
        {
            "field_render_scanner_prefix",
            0x156f996,
            {0x8d,0x41,0x06,0x3c,0x04,0x77,0x0e,0x44,0x0f,0xb7,0xf1,0x66,
             0x41,0xc1,0xe6,0x08,0xe9,0x6f,0x02,0x00,0x00},
            {0x8d,0x41,0x06,0x3c,0x04,0x77,0x0e,0x44,0x0f,0xb7,0xf1,0x66,0x41,0xc1,0xe6,0x08},
            16,
            PatchKind::FieldRender
        },
        {
            "field_layout_scanner_prefix",
            0x15716d4,
            {0x8d,0x43,0x06,0x3c,0x04,0x77,0x0c,0x0f,0xb7,0xf3,0x66,0xc1,
             0xe6,0x08,0xe9,0x58,0x02,0x00,0x00},
            {0x8d,0x43,0x06,0x3c,0x04,0x77,0x0c,0x0f,0xb7,0xf3,0x66,0xc1,0xe6,0x08},
            14,
            PatchKind::FieldLayout
        },
        {
            "glyph_renderer_c0_page_select",
            0x15720b7,
            {0x41,0x0f,0xb7,0xc0,0x4c,0x89,0xbc,0x24,0xe0,0x00,0x00,0x00,
             0x3d,0xfe,0x00,0x00,0x00,0x0f,0x87,0x6f,0x02,0x00,0x00},
            {0x41,0x0f,0xb7,0xc0,0x4c,0x89,0xbc,0x24,0xe0,0x00,0x00,0x00,0x3d,0xfe,0x00,0x00,0x00},
            17,
            PatchKind::GlyphRenderer
        },
        {
            "font_loader_korean_c0_extra_page",
            0x156e100,
            {0x48,0x8b,0x6c,0x24,0x58,0x48,0x8b,0x74,0x24,0x60,0x48,0x8b,0x7c,0x24,0x40},
            {0x48,0x8b,0x6c,0x24,0x58,0x48,0x8b,0x74,0x24,0x60,0x48,0x8b,0x7c,0x24,0x40},
            15,
            PatchKind::FontLoaderHook
        }
    };
}

const PatchSite* FindSite(PatchKind kind, const std::vector<PatchSite>& sites) {
    for (const auto& site : sites) {
        if (site.kind == kind) return &site;
    }
    return nullptr;
}

bool ReadMem(HANDLE process, uint64_t address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), out, size, &read) && read == size;
}

bool ReadMem(HANDLE process, uint64_t address, std::vector<uint8_t>& out, size_t size) {
    out.assign(size, 0);
    return ReadMem(process, address, out.data(), size);
}

bool WriteMem(HANDLE process, uint64_t address, const void* bytes, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(process, reinterpret_cast<LPVOID>(address), bytes, size, &written) && written == size;
}

bool WriteMem(HANDLE process, uint64_t address, const std::vector<uint8_t>& bytes) {
    return WriteMem(process, address, bytes.data(), bytes.size());
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
            uint64_t addr = module_base + site.rva;
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

bool ValidateFontFile(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        Log(L"abort: Korean font resource not found: " + path);
        return false;
    }
    std::vector<uint8_t> head(68, 0);
    f.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
    std::streamsize got = f.gcount();
    f.seekg(0, std::ios::end);
    std::streamoff size = f.tellg();
    if (size <= 0 || got < 68) {
        Log(L"abort: Korean font resource is too small: " + path);
        return false;
    }
    auto u32 = [&head](size_t index) -> uint32_t {
        size_t i = index * 4;
        return static_cast<uint32_t>(head[i]) |
               (static_cast<uint32_t>(head[i + 1]) << 8) |
               (static_cast<uint32_t>(head[i + 2]) << 16) |
               (static_cast<uint32_t>(head[i + 3]) << 24);
    };
    Log(L"Korean font file: " + path);
    Log(L"Korean font file size: " + std::to_wstring(static_cast<long long>(size)));
    Log(L"Korean font TEX header probe: magic=" + std::to_wstring(u32(0)) +
        L" width=" + std::to_wstring(u32(15)) + L" height=" + std::to_wstring(u32(16)));
    if (u32(0) != 1 || u32(15) != 1024 || u32(16) != 1024) {
        Log(L"abort: Korean font resource header does not match expected 1024x1024 TEX page");
        return false;
    }
    return true;
}

bool StageFontBesideGame(HANDLE process, const Options& opt, std::wstring* staged_path) {
    std::wstring process_path;
    if (!GetProcessImagePath(process, &process_path)) {
        Log(L"abort: could not query target process path error=" + std::to_wstring(GetLastError()));
        return false;
    }
    std::wstring game_dir = ParentDirectory(process_path);
    std::wstring target = JoinPath(game_dir, WidenAscii(opt.native_font_name));
    std::wstring source_full = FullPath(opt.font_path);
    std::wstring target_full = FullPath(target);

    Log(L"target game executable: " + process_path);
    Log(L"target game directory: " + game_dir);
    Log(L"Korean font staging target: " + target_full);

    if (_wcsicmp(source_full.c_str(), target_full.c_str()) != 0) {
        if (FileExists(target_full)) {
            Log(L"Korean font staging target already exists; validating without overwrite");
        } else {
            if (!CopyFileW(source_full.c_str(), target_full.c_str(), TRUE)) {
                Log(L"abort: could not copy Korean font resource beside game error=" +
                    std::to_wstring(GetLastError()));
                return false;
            }
            Log(L"copied Korean font resource beside game");
        }
    }
    if (!ValidateFontFile(target_full)) {
        Log(L"abort: staged Korean font resource is invalid");
        return false;
    }

    std::wstring source_tex = ReplaceExtension(source_full, L".tex");
    std::wstring target_tex = ReplaceExtension(target_full, L".tex");
    if (FileExists(source_tex) && _wcsicmp(source_tex.c_str(), target_tex.c_str()) != 0) {
        if (FileExists(target_tex)) {
            Log(L"Korean TEX alias already exists beside game; leaving it in place");
        } else if (CopyFileW(source_tex.c_str(), target_tex.c_str(), TRUE)) {
            Log(L"copied Korean TEX alias beside game: " + target_tex);
        } else {
            Log(L"warning: could not copy Korean TEX alias beside game error=" +
                std::to_wstring(GetLastError()));
        }
    }
    *staged_path = target_full;
    return true;
}

bool PrepareRemoteFontName(HANDLE process, uint64_t module_base, const Options& opt,
                           uint64_t* remote_state_out) {
    if (opt.native_font_name.empty() || opt.native_font_name.size() + 1 > kFontFilenameScratchSize) {
        Log(L"abort: native font name must be 1.." + std::to_wstring(kFontFilenameScratchSize - 1) +
            L" ASCII bytes");
        return false;
    }
    uint64_t font_struct_ptr = 0;
    uint32_t font_struct_handle = 0;
    if (!ReadMem(process, module_base + kGlobalFontStructPtrRva, &font_struct_ptr, sizeof(font_struct_ptr)) ||
        !ReadMem(process, module_base + kGlobalFontStructHandleRva, &font_struct_handle, sizeof(font_struct_handle))) {
        Log(L"abort: could not read native font globals");
        return false;
    }
    if (font_struct_ptr == 0 || font_struct_handle == 0) {
        Log(L"abort: native font struct is not initialized yet");
        return false;
    }

    std::vector<uint8_t> name(kFontFilenameScratchSize, 0);
    std::copy(opt.native_font_name.begin(), opt.native_font_name.end(), name.begin());
    uint64_t remote_name_ptr = font_struct_ptr + kFontFilenameScratchOffset;
    if (!WriteMem(process, remote_name_ptr, name.data(), name.size())) {
        Log(L"abort: could not write Korean font filename scratch slot");
        return false;
    }

    RemoteStateLocal state{};
    state.korean_c0_handle = 0;
    state.filename_handle = font_struct_handle + kFontFilenameScratchOffset;
    LPVOID remote_state = VirtualAllocEx(process, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_state) {
        Log(L"abort: VirtualAllocEx remote state failed error=" + std::to_wstring(GetLastError()));
        return false;
    }
    if (!WriteMem(process, reinterpret_cast<uint64_t>(remote_state), &state, sizeof(state))) {
        Log(L"abort: could not write remote state");
        return false;
    }

    *remote_state_out = reinterpret_cast<uint64_t>(remote_state);
    Log(L"native font struct ptr: " + Hex64(font_struct_ptr));
    Log(L"native font struct handle: " + Hex64(font_struct_handle));
    Log(L"Korean filename scratch ptr: " + Hex64(remote_name_ptr));
    Log(L"Korean filename scratch handle: " + Hex64(state.filename_handle));
    Log(L"Korean native filename: " + WidenAscii(opt.native_font_name));
    Log(L"remote patch state: " + Hex64(*remote_state_out));
    return true;
}

std::vector<uint8_t> MakeStub(const PatchSite& site, uint64_t module_base, uint64_t remote_state) {
    switch (site.kind) {
    case PatchKind::CommonRender:
        return MakeCommonRenderStub(module_base);
    case PatchKind::CommonWidth:
        return MakeCommonWidthStub(module_base);
    case PatchKind::FieldRender:
        return MakeFieldRenderStub(module_base);
    case PatchKind::FieldLayout:
        return MakeFieldLayoutStub(module_base);
    case PatchKind::GlyphRenderer:
        return MakeGlyphRendererStub(module_base, remote_state);
    case PatchKind::FontLoaderHook:
        return MakeFontLoaderHookStub(module_base, remote_state);
    }
    return {};
}

bool InstallOneSite(HANDLE process, uint64_t module_base, const PatchSite& site, uint64_t remote_state) {
    uint64_t target = module_base + site.rva;
    std::vector<uint8_t> got;
    if (!ReadMem(process, target, got, site.full_signature.size()) || got != site.full_signature) {
        Log(L"abort: original full signature mismatch for " + Utf8ish(site.id));
        if (!got.empty()) Log(L"  saw: " + HexBytes(got));
        Log(L"  expected: " + HexBytes(site.full_signature));
        return false;
    }

    std::vector<uint8_t> stub = MakeStub(site, module_base, remote_state);
    LPVOID remote_stub = VirtualAllocEx(process, nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_stub) {
        Log(L"VirtualAllocEx stub failed for " + Utf8ish(site.id) + L" error=" + std::to_wstring(GetLastError()));
        return false;
    }
    uint64_t stub_addr = reinterpret_cast<uint64_t>(remote_stub);
    if (!WriteMem(process, stub_addr, stub)) {
        Log(L"stub write failed for " + Utf8ish(site.id));
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtectEx(process, remote_stub, stub.size(), PAGE_EXECUTE_READ, &old_protect)) {
        Log(L"VirtualProtectEx stub failed for " + Utf8ish(site.id) +
            L" error=" + std::to_wstring(GetLastError()));
        return false;
    }
    FlushInstructionCache(process, remote_stub, stub.size());

    std::vector<uint8_t> detour = MakeAbsDetour(stub_addr, site.overwrite_len);
    Log(L"install " + Utf8ish(site.id));
    Log(L"  target=" + Hex64(target) + L" rva=" + Hex64(site.rva));
    Log(L"  original overwrite bytes=" + HexBytes(site.overwrite_original));
    Log(L"  stub=" + Hex64(stub_addr) + L" stub bytes=" + HexBytes(stub));
    Log(L"  detour bytes=" + HexBytes(detour));

    if (!ProtectWriteFlush(process, target, detour)) {
        return false;
    }
    std::vector<uint8_t> verify;
    if (!ReadMem(process, target, verify, detour.size()) || verify != detour) {
        Log(L"post-write verification failed for " + Utf8ish(site.id));
        return false;
    }
    Log(L"  patch succeeded");
    return true;
}

bool RestoreSites(HANDLE process, DWORD pid, uint64_t module_base, bool suspend_threads,
                  const std::vector<PatchSite>& sites) {
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
    Log(L"note: restore disables detours; remote stub/state allocations remain inert until FFVII.exe exits");
    return all_ok;
}

bool WaitForKoreanHandle(HANDLE process, uint64_t remote_state, DWORD wait_ms, uint32_t* handle_out) {
    const DWORD step_ms = 500;
    DWORD waited = 0;
    while (waited <= wait_ms) {
        RemoteStateLocal state{};
        if (ReadMem(process, remote_state, &state, sizeof(state)) && state.korean_c0_handle != 0) {
            *handle_out = state.korean_c0_handle;
            return true;
        }
        Sleep(step_ms);
        waited += step_ms;
    }
    return false;
}

bool TryDirectKoreanLoad(HANDLE process, uint64_t module_base, uint64_t remote_state, uint32_t* handle_out) {
    std::vector<uint8_t> stub = MakeDirectFontLoadStub(module_base, remote_state);
    LPVOID remote_stub = VirtualAllocEx(process, nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_stub) {
        Log(L"direct Korean load skipped: VirtualAllocEx failed error=" + std::to_wstring(GetLastError()));
        return false;
    }
    uint64_t stub_addr = reinterpret_cast<uint64_t>(remote_stub);
    if (!WriteMem(process, stub_addr, stub)) {
        Log(L"direct Korean load skipped: stub write failed error=" + std::to_wstring(GetLastError()));
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtectEx(process, remote_stub, stub.size(), PAGE_EXECUTE_READ, &old_protect)) {
        Log(L"direct Korean load skipped: VirtualProtectEx stub failed error=" +
            std::to_wstring(GetLastError()));
        return false;
    }
    FlushInstructionCache(process, remote_stub, stub.size());

    Log(L"attempt direct Korean C0 page load via current VM stack args");
    Log(L"  direct stub=" + Hex64(stub_addr) + L" stub bytes=" + HexBytes(stub));

    HANDLE thread = CreateRemoteThread(process, nullptr, 0,
                                       reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_stub),
                                       nullptr, 0, nullptr);
    if (!thread) {
        Log(L"direct Korean load skipped: CreateRemoteThread failed error=" +
            std::to_wstring(GetLastError()));
        return false;
    }
    DWORD wait = WaitForSingleObject(thread, 10000);
    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    if (wait != WAIT_OBJECT_0) {
        Log(L"direct Korean load did not return within 10000 ms; leaving fallback loader hook path");
        return false;
    }

    RemoteStateLocal state{};
    if (!ReadMem(process, remote_state, &state, sizeof(state))) {
        Log(L"direct Korean load finished, but remote state read failed");
        return false;
    }
    Log(L"direct Korean load VM args: saved_sp=" + Hex64(state.saved_vm_stack) +
        L" arg0=" + Hex64(state.direct_arg0) +
        L" arg1=" + Hex64(state.direct_arg1) +
        L" arg2=" + Hex64(state.direct_arg2) +
        L" arg3=" + Hex64(state.direct_arg3));
    Log(L"direct Korean load result: eax=" + Hex64(exit_code) +
        L" stored_handle=" + Hex64(state.korean_c0_handle) +
        L" status=" + std::to_wstring(state.direct_status));
    if (state.korean_c0_handle == 0) {
        return false;
    }
    *handle_out = state.korean_c0_handle;
    return true;
}

bool Install(HANDLE process, DWORD pid, uint64_t module_base, const Options& opt) {
    std::vector<PatchSite> sites = Sites();
    uint64_t remote_state = 0;
    if (!PrepareRemoteFontName(process, module_base, opt, &remote_state)) {
        return false;
    }

    const PatchSite* loader = FindSite(PatchKind::FontLoaderHook, sites);
    if (!loader) return false;

    uint32_t korean_handle = 0;
    if (!TryDirectKoreanLoad(process, module_base, remote_state, &korean_handle)) {
        Log(L"direct Korean C0 page load did not produce a handle; installing loader hook fallback");
        {
            SuspendedThreads suspended(pid, opt.suspend_threads);
            if (!InstallOneSite(process, module_base, *loader, remote_state)) {
                return false;
            }
        }

        Log(L"waiting for native font loader to create Korean C0 page handle, wait_ms=" +
            std::to_wstring(opt.wait_ms));
        if (!WaitForKoreanHandle(process, remote_state, opt.wait_ms, &korean_handle)) {
            Log(L"abort: Korean C0 page handle was not created before timeout");
            RestoreSites(process, pid, module_base, opt.suspend_threads, {*loader});
            return false;
        }
    }
    Log(L"Korean C0 page native handle: " + Hex64(korean_handle));

    bool all_ok = true;
    bool wrote_any = false;
    {
        SuspendedThreads suspended(pid, opt.suspend_threads);
        for (const auto& site : sites) {
            if (site.kind == PatchKind::FontLoaderHook) continue;
            if (!InstallOneSite(process, module_base, site, remote_state)) {
                all_ok = false;
                break;
            }
            wrote_any = true;
        }
    }
    if (!all_ok) {
        Log(L"install failed after loader hook; restoring all overwrite bytes");
        RestoreSites(process, pid, module_base, opt.suspend_threads, sites);
        return false;
    }
    if (!wrote_any) return false;
    return true;
}

void PrintUsage() {
    std::wcout
        << L"usage: c0_poc_patcher.exe install|restore [--pid N] [--process FFVII.exe]\\n"
        << L"                              [--font path] [--native-font-name korean_c0_page.tim]\\n"
        << L"                              [--wait-ms N] [--log path] [--no-suspend]\\n";
}

Options ParseArgs(int argc, wchar_t** argv) {
    Options opt;
    opt.font_path = DefaultFontPath();
    if (argc >= 2) opt.action = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--pid" && i + 1 < argc) opt.pid = ParseU32(argv[++i]);
        else if (a == L"--process" && i + 1 < argc) opt.process_name = argv[++i];
        else if (a == L"--font" && i + 1 < argc) opt.font_path = argv[++i];
        else if (a == L"--native-font-name" && i + 1 < argc) {
            std::wstring w = argv[++i];
            opt.native_font_name.clear();
            for (wchar_t ch : w) {
                if (ch > 0x7f) {
                    PrintUsage();
                    ExitProcess(2);
                }
                opt.native_font_name.push_back(static_cast<char>(ch));
            }
        }
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
    Log(L"==== first Korean glyph patcher " + opt.action + L" ====");

    if (_wcsicmp(opt.action.c_str(), L"install") == 0 && !ValidateFontFile(opt.font_path)) {
        return 1;
    }

    DWORD pid = opt.pid ? opt.pid : FindProcessByName(opt.process_name);
    if (!pid) {
        Log(L"target process not found: " + opt.process_name);
        return 1;
    }
    Log(L"process id: " + std::to_wstring(pid));

    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                                     PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD,
                                 FALSE, pid);
    if (!process) {
        Log(L"OpenProcess failed error=" + std::to_wstring(GetLastError()));
        return 1;
    }

    if (_wcsicmp(opt.action.c_str(), L"install") == 0) {
        std::wstring staged_font;
        if (!StageFontBesideGame(process, opt, &staged_font)) {
            CloseHandle(process);
            return 1;
        }
        Log(L"staged Korean font resource: " + staged_font);
    }

    uint64_t module_base = FindModuleBase(pid, opt.process_name);
    if (!module_base) {
        Log(L"module base not found for " + opt.process_name);
        CloseHandle(process);
        return 1;
    }
    Log(L"module base: " + Hex64(module_base));

    bool ok = false;
    std::vector<PatchSite> sites = Sites();
    if (_wcsicmp(opt.action.c_str(), L"install") == 0) {
        Log(L"waiting for runtime-decrypted signatures, wait_ms=" + std::to_wstring(opt.wait_ms));
        if (!WaitForRuntimeBytes(process, module_base, sites, opt.wait_ms)) {
            Log(L"abort: runtime signatures did not match before timeout");
            CloseHandle(process);
            return 1;
        }
        Log(L"runtime signatures validated");
        ok = Install(process, pid, module_base, opt);
    } else if (_wcsicmp(opt.action.c_str(), L"restore") == 0) {
        ok = RestoreSites(process, pid, module_base, opt.suspend_threads, sites);
    } else {
        PrintUsage();
        CloseHandle(process);
        return 2;
    }

    CloseHandle(process);
    Log(ok ? L"completed successfully" : L"completed with errors");
    return ok ? 0 : 1;
}
