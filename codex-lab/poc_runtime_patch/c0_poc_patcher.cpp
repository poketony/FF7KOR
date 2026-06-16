#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kGlobalFontStructPtrRva = 0x207CE08;
constexpr uint64_t kGlobalFontStructHandleRva = 0x207CDEC;
constexpr uint64_t kGlobalVmStackPtrRva = 0x20395C8;
constexpr uint32_t kFirstKoreanLead = 0xC0;
constexpr uint32_t kLastKoreanLead = 0xCC;
constexpr uint32_t kFirstExtraJafontPage = 7;
constexpr size_t kExtraJafontPageCount = kLastKoreanLead - kFirstKoreanLead + 1;
constexpr size_t kNativeNameSlotSize = 0x14;

struct Options {
    std::wstring action = L"install";
    std::wstring process_name = L"FFVII.exe";
    DWORD pid = 0;
    DWORD wait_ms = 120000;
    bool suspend_threads = true;
    std::wstring log_path = L"menu_jafont_extension_patch.log";
};

enum class PatchKind {
    CommonRender,
    CommonWidth,
    FieldRender,
    FieldLayout,
    GlyphRenderer,
    ExtraJafontLoaderHook,
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
    uint32_t extra_jafont_handles[kExtraJafontPageCount];
    uint32_t filename_handle;
    uint32_t direct_arg0;
    uint32_t direct_arg1;
    uint32_t direct_arg2;
    uint32_t direct_arg3;
    uint32_t saved_vm_stack;
    uint32_t direct_result;
    uint32_t direct_status;
    uint8_t reserved_to_filename[0x1c];
    char extra_jafont_names[kExtraJafontPageCount][kNativeNameSlotSize];
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

void AppendMovRsiImm64(std::vector<uint8_t>& out, uint64_t value) {
    out.push_back(0x48);
    out.push_back(0xBE);
    AppendU64(out, value);
}

void AppendMovRdiImm64(std::vector<uint8_t>& out, uint64_t value) {
    out.push_back(0x48);
    out.push_back(0xBF);
    AppendU64(out, value);
}

void AppendMovR10Imm64(std::vector<uint8_t>& out, uint64_t value) {
    out.push_back(0x49);
    out.push_back(0xBA);
    AppendU64(out, value);
}

void AppendMovR11Imm64(std::vector<uint8_t>& out, uint64_t value) {
    out.push_back(0x49);
    out.push_back(0xBB);
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

size_t AppendJcc32(std::vector<uint8_t>& out, uint8_t second_opcode) {
    out.push_back(0x0f);
    out.push_back(second_opcode);
    size_t disp_pos = out.size();
    AppendU32(out, 0);
    return disp_pos;
}

void PatchJcc32(std::vector<uint8_t>& out, size_t disp_pos, size_t target_pos) {
    const int64_t delta = static_cast<int64_t>(target_pos) - static_cast<int64_t>(disp_pos + 4);
    if (delta < (std::numeric_limits<int32_t>::min)() ||
        delta > (std::numeric_limits<int32_t>::max)()) {
        throw std::runtime_error("near conditional jump target out of range");
    }
    const uint32_t encoded = static_cast<uint32_t>(static_cast<int32_t>(delta));
    for (int i = 0; i < 4; ++i) {
        out[disp_pos + i] = static_cast<uint8_t>((encoded >> (i * 8)) & 0xff);
    }
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
        0x44, 0x0f, 0xb7, 0xc9,       // movzx r9d,cx
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
    b.insert(b.end(), {0x3c, 0xcc});   // cmp al,0xCC
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
    size_t jb_original = AppendJcc8(b, 0x72);
    b.insert(b.end(), {
        0x3d, 0xcc, 0x00, 0x00, 0x00                    // cmp eax,0xCC
    });
    size_t ja_original = AppendJcc8(b, 0x77);
    b.insert(b.end(), {
        0x2d, 0xc0, 0x00, 0x00, 0x00                    // sub eax,0xC0
    });
    AppendMovR10Imm64(b, remote_state);
    b.insert(b.end(), {
        0x41, 0x8b, 0x3c, 0x82,                          // mov edi,[r10+rax*4]
        0x85, 0xff                                       // test edi,edi
    });
    size_t jz_default = AppendJcc8(b, 0x74);
    AppendAbsJmp(b, common_handle_test);
    size_t original = b.size();
    b.insert(b.end(), {0x3d, 0xfe, 0x00, 0x00, 0x00});   // cmp eax,0xFE
    AppendAbsJmp(b, original_branch);
    size_t default_label = b.size();
    AppendAbsJmp(b, default_return);
    PatchJcc8(b, jb_original, original);
    PatchJcc8(b, ja_original, original);
    PatchJcc8(b, jz_default, default_label);
    return b;
}

std::vector<uint8_t> MakeFontLoaderHookStub(uint64_t module_base, uint64_t remote_state) {
    const uint64_t load_command = module_base + 0x4ab00;
    const uint64_t vm_stack_ptr = module_base + kGlobalVmStackPtrRva;
    const uint64_t font_struct_ptr_global = module_base + kGlobalFontStructPtrRva;
    const uint64_t font_struct_handle_global = module_base + kGlobalFontStructHandleRva;
    const uint64_t epilogue_continue = module_base + 0x156e10f;
    const uint32_t status_off = static_cast<uint32_t>(offsetof(RemoteStateLocal, direct_status));
    const uint32_t saved_vm_stack_off = static_cast<uint32_t>(offsetof(RemoteStateLocal, saved_vm_stack));
    const uint32_t arg0_off = static_cast<uint32_t>(offsetof(RemoteStateLocal, direct_arg0));
    const uint32_t arg1_off = static_cast<uint32_t>(offsetof(RemoteStateLocal, direct_arg1));
    const uint32_t arg2_off = static_cast<uint32_t>(offsetof(RemoteStateLocal, direct_arg2));
    const uint32_t arg3_off = static_cast<uint32_t>(offsetof(RemoteStateLocal, direct_arg3));
    const uint32_t names_off = static_cast<uint32_t>(offsetof(RemoteStateLocal, extra_jafont_names));
    std::vector<uint8_t> b;
    AppendMovRaxImm64(b, remote_state);
    b.insert(b.end(), {
        0x83, 0x78, static_cast<uint8_t>(status_off), 0x02 // cmp dword ptr [rax+status],2
    });
    size_t je_skip_loaded = AppendJcc32(b, 0x84);
    b.insert(b.end(), {
        0xc7, 0x40, static_cast<uint8_t>(status_off), 0x01, 0x00, 0x00, 0x00, // status=1
        0x89, 0x78, static_cast<uint8_t>(arg0_off),  // mov [rax+arg0],edi
        0x89, 0x70, static_cast<uint8_t>(arg1_off),  // mov [rax+arg1],esi
        0x89, 0x68, static_cast<uint8_t>(arg2_off),  // mov [rax+arg2],ebp
        0x89, 0x58, static_cast<uint8_t>(arg3_off)   // mov [rax+arg3],ebx
    });
    AppendMovRaxImm64(b, font_struct_ptr_global);
    b.insert(b.end(), {
        0x48, 0x8b, 0x00,                                // mov rax,[rax] ; font struct ptr
        0x48, 0x85, 0xc0                                 // test rax,rax
    });
    size_t jz_skip_no_struct = AppendJcc32(b, 0x84);
    AppendMovRdxImm64(b, font_struct_handle_global);
    b.insert(b.end(), {
        0x8b, 0x12,                                      // mov edx,[rdx] ; font struct handle
        0x85, 0xd2                                       // test edx,edx
    });
    size_t jz_skip_no_handle = AppendJcc32(b, 0x84);
    AppendMovRaxImm64(b, vm_stack_ptr);
    b.insert(b.end(), {
        0x8b, 0x08                                       // mov ecx,[rax]
    });
    AppendMovRaxImm64(b, remote_state);
    b.insert(b.end(), {
        0x89, 0x48, static_cast<uint8_t>(saved_vm_stack_off) // mov [rax+saved_vm_stack],ecx
    });

    for (size_t i = 0; i < kExtraJafontPageCount; ++i) {
        const uint32_t handle_off =
            static_cast<uint32_t>(offsetof(RemoteStateLocal, extra_jafont_handles)) +
            static_cast<uint32_t>(i * sizeof(uint32_t));
        const uint64_t source_name = remote_state + names_off + i * kNativeNameSlotSize;
        AppendMovRsiImm64(b, source_name);
        AppendMovRdiImm64(b, font_struct_ptr_global);
        b.insert(b.end(), {
            0x48, 0x8b, 0x3f,                              // mov rdi,[rdi]
            0x48, 0x81, 0xc7, 0xb8, 0x00, 0x00, 0x00,      // add rdi,0xB8
            0xb9, 0x14, 0x00, 0x00, 0x00,                  // mov ecx,0x14
            0xf3, 0xa4                                     // rep movsb
        });

        AppendMovRaxImm64(b, remote_state);
        b.insert(b.end(), {
            0x8b, 0x40, static_cast<uint8_t>(arg2_off),    // mov eax,[rax+arg2]
            0x89, 0x44, 0x24, 0x20                         // mov [rsp+0x20],eax
        });
        AppendMovRdxImm64(b, font_struct_handle_global);
        b.insert(b.end(), {
            0x8b, 0x02,                                    // mov eax,[rdx]
            0x05, 0xb8, 0x00, 0x00, 0x00,                  // add eax,0xB8
            0x89, 0x44, 0x24, 0x28                         // mov [rsp+0x28],eax
        });
        AppendMovRaxImm64(b, remote_state);
        b.insert(b.end(), {
            0x8b, 0x40, static_cast<uint8_t>(arg3_off),    // mov eax,[rax+arg3]
            0x89, 0x44, 0x24, 0x30                         // mov [rsp+0x30],eax
        });
        AppendMovRaxImm64(b, remote_state);
        b.insert(b.end(), {
            0x44, 0x8b, 0x48, static_cast<uint8_t>(arg1_off), // mov r9d,[rax+arg1]
            0x44, 0x8b, 0x40, static_cast<uint8_t>(arg0_off), // mov r8d,[rax+arg0]
            0xba, 0x05, 0x00, 0x00, 0x00,                    // mov edx,5
            0xb9, 0xac, 0x10, 0x67, 0x00                    // mov ecx,0x6710ac
        });
        AppendMovR10Imm64(b, load_command);
        b.insert(b.end(), {0x41, 0xff, 0xd2});             // call r10
        AppendMovRdxImm64(b, remote_state);
        b.insert(b.end(), {
            0x89, 0x82                                      // mov [rdx+handle_off],eax
        });
        AppendU32(b, handle_off);
    }

    AppendMovRdxImm64(b, remote_state);
    b.insert(b.end(), {
        0x8b, 0x4a, static_cast<uint8_t>(saved_vm_stack_off) // mov ecx,[rdx+saved_vm_stack]
    });
    AppendMovRaxImm64(b, vm_stack_ptr);
    b.insert(b.end(), {0x89, 0x08});                     // mov [rax],ecx
    AppendMovRaxImm64(b, remote_state);
    b.insert(b.end(), {
        0xc7, 0x40, static_cast<uint8_t>(status_off), 0x02, 0x00, 0x00, 0x00 // status=2
    });
    size_t skip_load = b.size();
    b.insert(b.end(), {
        0x48, 0x8b, 0x6c, 0x24, 0x58,                    // mov rbp,[rsp+0x58]
        0x48, 0x8b, 0x74, 0x24, 0x60,                    // mov rsi,[rsp+0x60]
        0x48, 0x8b, 0x7c, 0x24, 0x40                     // mov rdi,[rsp+0x40]
    });
    AppendAbsJmp(b, epilogue_continue);
    PatchJcc32(b, je_skip_loaded, skip_load);
    PatchJcc32(b, jz_skip_no_struct, skip_load);
    PatchJcc32(b, jz_skip_no_handle, skip_load);
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
            "menu_jafont_7_19_loader_hook",
            0x156e100,
            {0x48,0x8b,0x6c,0x24,0x58,0x48,0x8b,0x74,0x24,0x60,0x48,0x8b,0x7c,0x24,0x40},
            {0x48,0x8b,0x6c,0x24,0x58,0x48,0x8b,0x74,0x24,0x60,0x48,0x8b,0x7c,0x24,0x40},
            15,
            PatchKind::ExtraJafontLoaderHook
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

DWORD WaitForProcessByName(const std::wstring& name, DWORD wait_ms) {
    const DWORD step_ms = 250;
    DWORD waited = 0;
    DWORD pid = FindProcessByName(name);
    if (pid != 0) return pid;

    Log(L"waiting for process " + name + L", wait_ms=" + std::to_wstring(wait_ms));
    while (waited < wait_ms) {
        Sleep(step_ms);
        waited += step_ms;
        pid = FindProcessByName(name);
        if (pid != 0) {
            Log(L"found process after " + std::to_wstring(waited) + L" ms");
            return pid;
        }
    }
    return 0;
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

bool PrepareRemoteMenuJafontState(HANDLE process, uint64_t* remote_state_out) {
    RemoteStateLocal state{};
    for (size_t i = 0; i < kExtraJafontPageCount; ++i) {
        const unsigned page = kFirstExtraJafontPage + static_cast<unsigned>(i);
        std::snprintf(state.extra_jafont_names[i], kNativeNameSlotSize, "jafont_%u.tim", page);
    }

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
    Log(L"remote patch state: " + Hex64(*remote_state_out));
    Log(L"extra menu jafont logical names: jafont_7.tim .. jafont_19.tim");
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
    case PatchKind::ExtraJafontLoaderHook:
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

bool WaitForExtraJafontHandles(HANDLE process, uint64_t remote_state, DWORD wait_ms) {
    const DWORD step_ms = 500;
    DWORD waited = 0;
    while (waited <= wait_ms) {
        RemoteStateLocal state{};
        if (ReadMem(process, remote_state, &state, sizeof(state)) && state.direct_status == 2) {
            bool all_ready = true;
            std::wstring summary = L"extra menu jafont handles:";
            for (size_t i = 0; i < kExtraJafontPageCount; ++i) {
                const uint32_t lead = kFirstKoreanLead + static_cast<uint32_t>(i);
                const uint32_t page = kFirstExtraJafontPage + static_cast<uint32_t>(i);
                const uint32_t handle = state.extra_jafont_handles[i];
                if (handle == 0) all_ready = false;
                summary += L" lead=" + Hex64(lead);
                summary += L"/jafont_" + std::to_wstring(page);
                summary += L"=" + Hex64(handle);
            }
            Log(summary);
            return all_ready;
        }
        Sleep(step_ms);
        waited += step_ms;
    }
    return false;
}

bool Install(HANDLE process, DWORD pid, uint64_t module_base, const Options& opt) {
    std::vector<PatchSite> sites = Sites();
    uint64_t remote_state = 0;
    if (!PrepareRemoteMenuJafontState(process, &remote_state)) {
        return false;
    }

    const PatchSite* loader = FindSite(PatchKind::ExtraJafontLoaderHook, sites);
    if (!loader) return false;

    {
        SuspendedThreads suspended(pid, opt.suspend_threads);
        if (!InstallOneSite(process, module_base, *loader, remote_state)) {
            return false;
        }
    }

    Log(L"waiting for native loader to create menu_ja.lgp jafont_7..19 handles, wait_ms=" +
        std::to_wstring(opt.wait_ms));
    if (!WaitForExtraJafontHandles(process, remote_state, opt.wait_ms)) {
        Log(L"abort: one or more extra menu jafont handles were not created before timeout");
        Log(L"run the patcher before launching FFVII.exe, and confirm jafont_7.tex..jafont_19.tex are inside menu_ja.lgp");
        RestoreSites(process, pid, module_base, opt.suspend_threads, {*loader});
        return false;
    }

    std::vector<PatchSite> text_sites;
    for (const auto& site : sites) {
        if (site.kind != PatchKind::ExtraJafontLoaderHook) text_sites.push_back(site);
    }
    Log(L"waiting for runtime-decrypted scanner/renderer signatures, wait_ms=" +
        std::to_wstring(opt.wait_ms));
    if (!WaitForRuntimeBytes(process, module_base, text_sites, opt.wait_ms)) {
        Log(L"abort: scanner/renderer runtime signatures did not match before timeout");
        RestoreSites(process, pid, module_base, opt.suspend_threads, {*loader});
        return false;
    }
    Log(L"scanner/renderer runtime signatures validated");

    bool all_ok = true;
    bool wrote_any = false;
    {
        SuspendedThreads suspended(pid, opt.suspend_threads);
        for (const auto& site : sites) {
            if (site.kind == PatchKind::ExtraJafontLoaderHook) continue;
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
    Log(L"==== menu_ja jafont extension patcher " + opt.action + L" ====");

    DWORD pid = opt.pid ? opt.pid : WaitForProcessByName(opt.process_name, opt.wait_ms);
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
        const PatchSite* loader = FindSite(PatchKind::ExtraJafontLoaderHook, sites);
        if (!loader) {
            Log(L"abort: loader patch site is not defined");
            CloseHandle(process);
            return 1;
        }
        Log(L"waiting for runtime-decrypted loader signature, wait_ms=" + std::to_wstring(opt.wait_ms));
        if (!WaitForRuntimeBytes(process, module_base, std::vector<PatchSite>{*loader}, opt.wait_ms)) {
            Log(L"abort: loader runtime signature did not match before timeout");
            CloseHandle(process);
            return 1;
        }
        Log(L"loader runtime signature validated");
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
