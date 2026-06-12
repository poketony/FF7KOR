#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

extern "C" volatile unsigned char g_ff7korFieldTextHookHandled = 0;

namespace {

constexpr std::uintptr_t kImageBase = 0x400000;
constexpr std::uintptr_t kLoaderPatchSite = 0x676619;
constexpr std::uintptr_t kEngineLoadMenuGraphicsObjects = 0x6C1468;
constexpr std::uintptr_t kFieldSubmitDrawText = 0x6E706D;
constexpr std::uintptr_t kEngineLoadGraphicsObject = 0x6710AC;
constexpr std::uintptr_t kEnginePrepareGraphicsObject = 0x66E272;
constexpr std::uintptr_t kEngineDrawGraphicsObject = 0x66E641;
constexpr std::uintptr_t kEngineResetGraphicsObject = 0x66E62C;
constexpr std::uintptr_t kEngineSetBlendMode = 0x674659;
constexpr std::uintptr_t kMenuFontFirstLoadCallSite = 0x6C160C;
constexpr std::uintptr_t kMenuDrawFontBCallSite = 0x6CCAB8;
constexpr std::uintptr_t kFieldDrawFontBCallSite = 0x6ECB8B;
constexpr std::uintptr_t kFieldTextDecodeHookSite = 0x6E70F5;

char g_logPath[MAX_PATH] = {};
void *g_hangulFontObjects[13] = {};
bool g_hangulFontLoadAttempted = false;
bool g_missingHangulFontLogged = false;
bool g_prepareHangulFontSkippedLogged = false;
bool g_hangulDrawLogged = false;
bool g_hangulFieldDrawLogged = false;

const char *const g_hangulFontNames[13] = {
    "hangulfont_1.tim",
    "hangulfont_2.tim",
    "hangulfont_3.tim",
    "hangulfont_4.tim",
    "hangulfont_5.tim",
    "hangulfont_6.tim",
    "hangulfont_7.tim",
    "hangulfont_8.tim",
    "hangulfont_9.tim",
    "hangulfont_10.tim",
    "hangulfont_11.tim",
    "hangulfont_12.tim",
    "hangulfont_13.tim",
};

const char *const g_hangulFontFallbackNames[13] = {
    "hangulfont_1.tex",
    "hangulfont_2.tex",
    "hangulfont_3.tex",
    "hangulfont_4.tex",
    "hangulfont_5.tex",
    "hangulfont_6.tex",
    "hangulfont_7.tex",
    "hangulfont_8.tex",
    "hangulfont_9.tex",
    "hangulfont_10.tex",
    "hangulfont_11.tex",
    "hangulfont_12.tex",
    "hangulfont_13.tex",
};

using LoadGraphicsObjectFn = void *(__cdecl *)(int, int, void *, const char *, void *);
LoadGraphicsObjectFn engine_load_graphics_object =
    reinterpret_cast<LoadGraphicsObjectFn>(kEngineLoadGraphicsObject);

using PrepareGraphicsObjectFn = int(__cdecl *)(int, void *);
PrepareGraphicsObjectFn engine_prepare_graphics_object =
    reinterpret_cast<PrepareGraphicsObjectFn>(kEnginePrepareGraphicsObject);

using SetBlendModeFn = void(__cdecl *)(int, void *);
SetBlendModeFn engine_set_blend_mode =
    reinterpret_cast<SetBlendModeFn>(kEngineSetBlendMode);

using DrawGraphicsObjectFn = void(__cdecl *)(void *, void *);
DrawGraphicsObjectFn engine_draw_graphics_object =
    reinterpret_cast<DrawGraphicsObjectFn>(kEngineDrawGraphicsObject);

using ResetGraphicsObjectFn = void(__cdecl *)(void *);
ResetGraphicsObjectFn engine_reset_graphics_object =
    reinterpret_cast<ResetGraphicsObjectFn>(kEngineResetGraphicsObject);

struct TexturedVertex {
    float x;
    float y;
    std::uint32_t z;
    float rhw;
    std::uint32_t diffuse;
    std::uint32_t specular;
    float u;
    float v;
};

struct GraphicsObject {
    unsigned char pad0[0x70];
    TexturedVertex *vertices;
    unsigned char pad74[0x78 - 0x74];
    std::uint8_t *paletteIndex;
    std::uint32_t activePaletteIndex;
};

volatile std::int16_t *const field_remaining_character_length =
    reinterpret_cast<volatile std::int16_t *>(0xDC3CCC);
volatile int *const field_current_window_pos_x =
    reinterpret_cast<volatile int *>(0xDC3CB4);
volatile int *const field_text_line_row =
    reinterpret_cast<volatile int *>(0xDC3CB8);
volatile int *const field_text_box_current_n_characters =
    reinterpret_cast<volatile int *>(0xDC3CB0);
volatile int *const field_spaced_characters =
    reinterpret_cast<volatile int *>(0xDC3CD4);
volatile std::int16_t *const current_font_color =
    reinterpret_cast<volatile std::int16_t *>(0x91F028);
volatile int *const menu_text_dirty =
    reinterpret_cast<volatile int *>(0xDC3CEC);

constexpr int kHangulPrefixStart = 0xC0;
constexpr int kHangulPrefixEnd = 0xCC;
constexpr int kHangulTextureSize = 1024;
constexpr int kHangulColumns = 16;
constexpr int kHangulCellSize = kHangulTextureSize / kHangulColumns;
constexpr int kHangulDisplayWidth = 31;
constexpr int kHangulDisplayHeight = 24;

void init_log_path(HMODULE module)
{
    char modulePath[MAX_PATH] = {};
    DWORD len = GetModuleFileNameA(module, modulePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        lstrcpynA(g_logPath, "ff7kor.log", MAX_PATH);
        return;
    }

    char *slash = strrchr(modulePath, '\\');
    if (slash) {
        slash[1] = '\0';
        lstrcpynA(g_logPath, modulePath, MAX_PATH);
        lstrcatA(g_logPath, "ff7kor.log");
    } else {
        lstrcpynA(g_logPath, "ff7kor.log", MAX_PATH);
    }
}

void log_line(const char *fmt, ...)
{
    FILE *f = nullptr;
    fopen_s(&f, g_logPath[0] ? g_logPath : "ff7kor.log", "ab");
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

bool bytes_equal(std::uintptr_t address, const unsigned char *expected, std::size_t size)
{
    return std::memcmp(reinterpret_cast<const void *>(address), expected, size) == 0;
}

bool write_memory(std::uintptr_t address, const void *data, std::size_t size)
{
    DWORD oldProtect = 0;
    void *target = reinterpret_cast<void *>(address);
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        log_line("VirtualProtect failed at 0x%08lX", static_cast<unsigned long>(address));
        return false;
    }

    std::memcpy(target, data, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);

    DWORD ignored = 0;
    VirtualProtect(target, size, oldProtect, &ignored);
    return true;
}

bool patch_call(std::uintptr_t callSite, void *target)
{
    const auto src = static_cast<std::intptr_t>(callSite);
    const auto dst = reinterpret_cast<std::intptr_t>(target);
    const auto rel = dst - (src + 5);
    if (rel < std::numeric_limits<std::int32_t>::min()
        || rel > std::numeric_limits<std::int32_t>::max()) {
        log_line("call target is out of rel32 range: site=0x%08lX target=%p",
                 static_cast<unsigned long>(callSite), target);
        return false;
    }

    unsigned char patch[5] = {0xE8, 0, 0, 0, 0};
    const auto rel32 = static_cast<std::int32_t>(rel);
    std::memcpy(patch + 1, &rel32, sizeof(rel32));
    return write_memory(callSite, patch, sizeof(patch));
}

bool patch_jump(std::uintptr_t jumpSite, void *target)
{
    const auto src = static_cast<std::intptr_t>(jumpSite);
    const auto dst = reinterpret_cast<std::intptr_t>(target);
    const auto rel = dst - (src + 5);
    if (rel < std::numeric_limits<std::int32_t>::min()
        || rel > std::numeric_limits<std::int32_t>::max()) {
        log_line("jump target is out of rel32 range: site=0x%08lX target=%p",
                 static_cast<unsigned long>(jumpSite), target);
        return false;
    }

    unsigned char patch[5] = {0xE9, 0, 0, 0, 0};
    const auto rel32 = static_cast<std::int32_t>(rel);
    std::memcpy(patch + 1, &rel32, sizeof(rel32));
    return write_memory(jumpSite, patch, sizeof(patch));
}

void fill_hangul_vertices(GraphicsObject *object, int glyph, std::int16_t x, std::int16_t y,
                          std::uint32_t z)
{
    if (object == nullptr || object->vertices == nullptr) {
        return;
    }

    const int cellX = glyph % kHangulColumns;
    const int cellY = glyph / kHangulColumns;
    const float u0 = static_cast<float>(cellX * kHangulCellSize) / kHangulTextureSize;
    const float v0 = static_cast<float>(cellY * kHangulCellSize) / kHangulTextureSize;
    const float u1 = static_cast<float>((cellX + 1) * kHangulCellSize) / kHangulTextureSize;
    const float v1 = static_cast<float>((cellY + 1) * kHangulCellSize) / kHangulTextureSize;
    const float left = static_cast<float>(x);
    const float top = static_cast<float>(y);
    const float right = static_cast<float>(x + kHangulDisplayWidth);
    const float bottom = static_cast<float>(y + kHangulDisplayHeight);

    TexturedVertex *vertices = object->vertices;
    vertices[0] = {left, top, z, 1.0f, 0xFFFFFFFFu, 0xFF000000u, u0, v0};
    vertices[1] = {left, bottom, z, 1.0f, 0xFFFFFFFFu, 0xFF000000u, u0, v1};
    vertices[2] = {right, top, z, 1.0f, 0xFFFFFFFFu, 0xFF000000u, u1, v0};
    vertices[3] = {right, bottom, z, 1.0f, 0xFFFFFFFFu, 0xFF000000u, u1, v1};

    const std::uint8_t paletteIndex = static_cast<std::uint8_t>((*current_font_color & 0x7) << 1);
    if (object->paletteIndex != nullptr) {
        *object->paletteIndex = paletteIndex;
    }
    object->activePaletteIndex = paletteIndex;
    *menu_text_dirty = 1;
}

extern "C" bool __cdecl handle_hangul_field_glyph(std::int16_t *x, std::int16_t *y,
                                                   std::uint8_t **textPtr, int maxWidth,
                                                   std::uint32_t z)
{
    if (x == nullptr || y == nullptr || textPtr == nullptr || *textPtr == nullptr) {
        return false;
    }

    std::uint8_t *text = *textPtr;
    const std::uint8_t prefix = text[0];
    if (prefix < kHangulPrefixStart || prefix > kHangulPrefixEnd) {
        return false;
    }
    if (*field_remaining_character_length < 2) {
        return false;
    }

    const int page = prefix - kHangulPrefixStart;
    if (page < 0 || page >= 13 || g_hangulFontObjects[page] == nullptr) {
        if (!g_missingHangulFontLogged) {
            g_missingHangulFontLogged = true;
            log_line("hangul glyph requested before font page was loaded: prefix=0x%02X",
                     static_cast<unsigned int>(prefix));
        }
        return false;
    }

    const int lineStart = *field_current_window_pos_x + 0x10;
    if (*x - *field_current_window_pos_x + kHangulDisplayWidth > maxWidth) {
        *x = static_cast<std::int16_t>(lineStart);
        *y = static_cast<std::int16_t>(*y + 0x20);
        *field_text_line_row += 1;
    }

    GraphicsObject *object = static_cast<GraphicsObject *>(g_hangulFontObjects[page]);
    const std::uint8_t glyph = text[1];
    if (engine_prepare_graphics_object(1, object)) {
        fill_hangul_vertices(object, glyph, *x, *y, z);
    } else if (!g_prepareHangulFontSkippedLogged) {
        g_prepareHangulFontSkippedLogged = true;
        log_line("hangul font page was not drawable yet: prefix=0x%02X glyph=0x%02X",
                 static_cast<unsigned int>(prefix),
                 static_cast<unsigned int>(glyph));
    }

    *textPtr = text + 2;
    *field_remaining_character_length = static_cast<std::int16_t>(*field_remaining_character_length - 2);
    *field_text_box_current_n_characters += 1;
    *x = static_cast<std::int16_t>(*x + (*field_spaced_characters ? 0x1A : kHangulDisplayWidth));
    return true;
}

#if defined(_M_IX86)
extern "C" __declspec(naked) void field_text_decode_hook()
{
    __asm {
        pushfd
        pushad

        push dword ptr [ebp + 0x18]
        push dword ptr [ebp + 0x10]
        lea eax, [ebp + 0x14]
        push eax
        lea eax, [ebp + 0x0C]
        push eax
        lea eax, [ebp + 0x08]
        push eax
        call handle_hangul_field_glyph
        add esp, 0x14
        mov byte ptr [g_ff7korFieldTextHookHandled], al

        popad
        popfd

        cmp byte ptr [g_ff7korFieldTextHookHandled], 0
        jne handled

        mov eax, dword ptr [ebp + 0x14]
        xor ecx, ecx
        mov edx, 0x006E70FA
        jmp edx

    handled:
        mov edx, 0x006E7086
        jmp edx
    }
}
#else
#error ff7kor.dll field text hook requires a 32-bit x86 build.
#endif

void * __cdecl load_graphics_object_hook(int type, int category, void *textureInfo,
                                         const char *filename, void *archiveContext)
{
    void *original = engine_load_graphics_object(type, category, textureInfo, filename, archiveContext);

    if (!g_hangulFontLoadAttempted) {
        g_hangulFontLoadAttempted = true;
        log_line("loading hangul font pages using context from %s",
                 filename != nullptr ? filename : "(null)");
        for (int i = 0; i < 13; ++i) {
            engine_set_blend_mode(4, textureInfo);
            g_hangulFontObjects[i] = engine_load_graphics_object(
                type, category, textureInfo, g_hangulFontNames[i], archiveContext);
            if (g_hangulFontObjects[i] == nullptr) {
                engine_set_blend_mode(4, textureInfo);
                g_hangulFontObjects[i] = engine_load_graphics_object(
                    type, category, textureInfo, g_hangulFontFallbackNames[i], archiveContext);
                log_line("  %s -> null, %s -> %p",
                         g_hangulFontNames[i],
                         g_hangulFontFallbackNames[i],
                         g_hangulFontObjects[i]);
            } else {
                log_line("  %s -> %p", g_hangulFontNames[i], g_hangulFontObjects[i]);
            }
        }
    }

    return original;
}

int draw_hangul_font_pages(void *gameObject, bool resetAfterDraw)
{
    int drawn = 0;
    for (void *hangulFontObject : g_hangulFontObjects) {
        if (hangulFontObject != nullptr) {
            engine_draw_graphics_object(hangulFontObject, gameObject);
            if (resetAfterDraw) {
                engine_reset_graphics_object(hangulFontObject);
            }
            ++drawn;
        }
    }
    return drawn;
}

void __cdecl draw_graphics_object_hook(void *object, void *gameObject)
{
    engine_draw_graphics_object(object, gameObject);

    const int drawn = draw_hangul_font_pages(gameObject, false);

    if (!g_hangulDrawLogged) {
        g_hangulDrawLogged = true;
        log_line("hangul font draw hook ran: drawn_pages=%d", drawn);
    }
}

void __cdecl draw_field_text_graphics_object_hook(void *object, void *gameObject)
{
    engine_draw_graphics_object(object, gameObject);

    const int drawn = draw_hangul_font_pages(gameObject, true);

    if (!g_hangulFieldDrawLogged) {
        g_hangulFieldDrawLogged = true;
        log_line("hangul field draw hook ran: drawn_pages=%d", drawn);
    }
}

void install_hooks()
{
    const unsigned char firstFontLoadCall[] = {0xE8, 0x9B, 0xFA, 0xFA, 0xFF};
    const unsigned char menuDrawFontBCall[] = {0xE8, 0x84, 0x1B, 0xFA, 0xFF};
    const unsigned char fieldDrawFontBCall[] = {0xE8, 0xB1, 0x1A, 0xF8, 0xFF};
    const unsigned char fieldDecodeBytes[] = {0x8B, 0x45, 0x14, 0x33, 0xC9};
    if (!bytes_equal(kMenuFontFirstLoadCallSite, firstFontLoadCall, sizeof(firstFontLoadCall))) {
        log_line("font load call site mismatch at 0x%08lX",
                 static_cast<unsigned long>(kMenuFontFirstLoadCallSite));
    } else if (patch_call(kMenuFontFirstLoadCallSite, reinterpret_cast<void *>(&load_graphics_object_hook))) {
        log_line("font load hook installed at 0x%08lX",
                 static_cast<unsigned long>(kMenuFontFirstLoadCallSite));
    }

    if (!bytes_equal(kMenuDrawFontBCallSite, menuDrawFontBCall, sizeof(menuDrawFontBCall))) {
        log_line("font draw call site mismatch at 0x%08lX",
                 static_cast<unsigned long>(kMenuDrawFontBCallSite));
    } else if (patch_call(kMenuDrawFontBCallSite, reinterpret_cast<void *>(&draw_graphics_object_hook))) {
        log_line("font draw hook installed at 0x%08lX",
                 static_cast<unsigned long>(kMenuDrawFontBCallSite));
    }

    if (!bytes_equal(kFieldDrawFontBCallSite, fieldDrawFontBCall, sizeof(fieldDrawFontBCall))) {
        log_line("field font draw call site mismatch at 0x%08lX",
                 static_cast<unsigned long>(kFieldDrawFontBCallSite));
    } else if (patch_call(kFieldDrawFontBCallSite, reinterpret_cast<void *>(&draw_field_text_graphics_object_hook))) {
        log_line("field font draw hook installed at 0x%08lX",
                 static_cast<unsigned long>(kFieldDrawFontBCallSite));
    }

    if (!bytes_equal(kFieldTextDecodeHookSite, fieldDecodeBytes, sizeof(fieldDecodeBytes))) {
        log_line("field text decode hook site mismatch at 0x%08lX",
                 static_cast<unsigned long>(kFieldTextDecodeHookSite));
    } else if (patch_jump(kFieldTextDecodeHookSite, reinterpret_cast<void *>(&field_text_decode_hook))) {
        log_line("field text decode hook installed at 0x%08lX",
                 static_cast<unsigned long>(kFieldTextDecodeHookSite));
    }
}

void probe_engine()
{
    const unsigned char restoredEntry[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x54};
    const unsigned char engineLoadPrologue[] = {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xA4, 0x00, 0x00, 0x00};
    const unsigned char fieldDrawPrologue[] = {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xC8, 0x00, 0x00, 0x00};
    const unsigned char firstFontLoadCall[] = {0xE8, 0x9B, 0xFA, 0xFA, 0xFF};
    const unsigned char menuDrawFontBCall[] = {0xE8, 0x84, 0x1B, 0xFA, 0xFF};
    const unsigned char fieldDrawFontBCall[] = {0xE8, 0xB1, 0x1A, 0xF8, 0xFF};
    const unsigned char fieldDecodeBytes[] = {0x8B, 0x45, 0x14, 0x33, 0xC9};

    log_line("ff7kor.dll loaded");
    log_line("image base expected: 0x%08lX", static_cast<unsigned long>(kImageBase));
    log_line("loader site restored: %s",
             bytes_equal(kLoaderPatchSite, restoredEntry, sizeof(restoredEntry)) ? "yes" : "no");
    log_line("engine_load_menu_graphics_objects_6C1468: %s",
             bytes_equal(kEngineLoadMenuGraphicsObjects, engineLoadPrologue, sizeof(engineLoadPrologue)) ? "ok" : "mismatch");
    log_line("field_submit_draw_text_640x480_6E706D: %s",
             bytes_equal(kFieldSubmitDrawText, fieldDrawPrologue, sizeof(fieldDrawPrologue)) ? "ok" : "mismatch");
    log_line("engine_load_graphics_object_6710AC at 0x%08lX",
             static_cast<unsigned long>(kEngineLoadGraphicsObject));
    log_line("engine_prepare_graphics_object_66E272 at 0x%08lX",
             static_cast<unsigned long>(kEnginePrepareGraphicsObject));
    log_line("engine_draw_graphics_object_66E641 at 0x%08lX",
             static_cast<unsigned long>(kEngineDrawGraphicsObject));
    log_line("engine_reset_graphics_object_66E62C at 0x%08lX",
             static_cast<unsigned long>(kEngineResetGraphicsObject));
    log_line("engine_set_blend_mode_674659 at 0x%08lX",
             static_cast<unsigned long>(kEngineSetBlendMode));
    log_line("menu_font_first_load_call_6C160C: %s",
             bytes_equal(kMenuFontFirstLoadCallSite, firstFontLoadCall, sizeof(firstFontLoadCall))
                 ? "ok"
                 : "mismatch");
    log_line("menu_draw_font_b_call_6CCAB8: %s",
             bytes_equal(kMenuDrawFontBCallSite, menuDrawFontBCall, sizeof(menuDrawFontBCall))
                 ? "ok"
                 : "mismatch");
    log_line("field_draw_font_b_call_6ECB8B: %s",
             bytes_equal(kFieldDrawFontBCallSite, fieldDrawFontBCall, sizeof(fieldDrawFontBCall))
                 ? "ok"
                 : "mismatch");
    log_line("field_text_decode_hook_6E70F5: %s",
             bytes_equal(kFieldTextDecodeHookSite, fieldDecodeBytes, sizeof(fieldDecodeBytes))
                 ? "ok"
                 : "mismatch");
}

} // namespace

extern "C" __declspec(dllexport) int FF7KOR_Ping()
{
    return 0xFF7;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        init_log_path(module);
        probe_engine();
        install_hooks();
    }
    return TRUE;
}
