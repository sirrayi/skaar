// UI module (P1 skeleton): main menu, in-game menu, loading screen.
// Style: VØLUND stage with an ember accent and a faint arena grid, a nod to
// the original arena-shooter menus (docs/ui-style.md).
//
// This file is part of Skaar. GPLv2 or later.
#include <cmath>
#include <cstdarg>
#include <exception>
#include <functional>
#include <string>
#include <vector>

#include "../shared/draw2d.h"
#include "syscalls.h"

namespace ui {
namespace {

namespace kit = skaar::ui;
namespace theme = skaar::ui::theme;
using kit::Align;
using kit::Color;

enum class Screen { kNone, kMain, kInGame };

struct Item {
    const char* label;
    const char* hint;  // shown beside the selected item, mono
    std::function<void()> action;
};

struct Rect {
    float x, y, w, h;
    bool Contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct State {
    glconfig_t glconfig;
    Screen screen = Screen::kNone;
    int cursor = 0;
    int openedAt = 0;
    int realtime = 0;
    float mouseX = 0, mouseY = 0;
    bool mouseUsed = false;
    std::vector<Rect> itemRects;
    std::string lastError;
    std::string version;
    qhandle_t mark = 0, pointer = 0;
} ui;

void Exec(const char* text) { trap::CmdExecuteText(EXEC_APPEND, text); }

void Close() {
    trap::Key_SetCatcher(trap::Key_GetCatcher() & ~KEYCATCH_UI);
    trap::Key_ClearStates();
    ui.screen = Screen::kNone;
}

std::vector<Item>& MainItems() {
    static std::vector<Item> items = {
        {"Test room", "LOADS TESTROOM · DEVMAP", [] { Exec("devmap testroom\n"); }},
        {"Console", "OR PRESS ~", [] { Exec("toggleconsole\n"); }},
        {"Quit", "", [] { Exec("quit\n"); }},
    };
    return items;
}

std::vector<Item>& InGameItems() {
    static std::vector<Item> items = {
        {"Resume", "ESC", [] { Close(); }},
        {"Restart map", "", [] {
             Exec("map_restart 0\n");
             Close();
         }},
        {"Leave", "BACK TO THE MAIN MENU", [] { Exec("disconnect\n"); }},
        {"Quit", "", [] { Exec("quit\n"); }},
    };
    return items;
}

std::vector<Item>& Items() { return ui.screen == Screen::kInGame ? InGameItems() : MainItems(); }

void Open(Screen screen) {
    ui.screen = screen;
    ui.cursor = 0;
    ui.openedAt = ui.realtime;
    ui.mouseUsed = false;
    ui.mouseX = kit::Width() / 2;
    ui.mouseY = kit::Height() / 2;
    trap::Key_SetCatcher(KEYCATCH_UI);

    // surface (once) the error that dropped us back here
    ui.lastError = trap::CvarString("com_errorMessage");
    if (!ui.lastError.empty()) {
        trap::CvarSet("com_errorMessage", "");
    }
    Com_DPrintf("ui: open %s\n", screen == Screen::kMain ? "main" : "ingame");
}

std::string MapName() {
    std::string map = Info_ValueForKey(trap::GetConfigString(CS_SERVERINFO).c_str(), "mapname");
    return map.empty() ? "unknown" : map;
}

// ---- drawing ------------------------------------------------------------------

// 0 -> 1 over the first 220 ms after opening, eased out
float OpenFade() {
    const float t = std::fmin(1.0f, (ui.realtime - ui.openedAt) / 220.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

void DrawHeader(const std::string& left, const std::string& right, float a) {
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, w = kit::Width();
    f.mono.Draw(m, 88, theme::kMonoSize, left, theme::kCreamSoft.Alpha(a), theme::kTrackMono);
    f.mono.Draw(w - m, 88, theme::kMonoSize, right, theme::kCreamFaint.Alpha(a), theme::kTrackMono, Align::kRight);
    kit::HLine(m, 112, w - 2 * m, theme::kHairline.Alpha(a));
}

void DrawFooter(const std::string& left, const std::string& right, float a) {
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, w = kit::Width(), h = kit::Height();
    kit::HLine(m, h - 104, w - 2 * m, theme::kHairline.Alpha(a));
    f.mono.Draw(m, h - 64, theme::kMonoSize, left, theme::kCreamFaint.Alpha(a), theme::kTrackMono);
    f.mono.Draw(w - m, h - 64, theme::kMonoSize, right, theme::kCreamFaint.Alpha(a), theme::kTrackMono, Align::kRight);
}

// Numbered item list; selected item: full cream + ember tick (pulsing gently,
// like the old menus' highlight) + its hint
void DrawItems(float top, float a) {
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, step = 76;
    const float cap = f.text.CapHeight(theme::kItemSize);
    std::vector<Item>& items = Items();
    ui.itemRects.clear();

    for (size_t i = 0; i < items.size(); i++) {
        const float baseline = top + i * step;
        const bool selected = static_cast<int>(i) == ui.cursor;
        const std::string label = kit::Upper(items[i].label);

        f.mono.Draw(m, baseline, theme::kMonoSize, skaar::Va("%02zu", i + 1),
                    (selected ? theme::kCreamSoft : theme::kCreamFaint).Alpha(a), theme::kTrackMono);
        const float labelX = m + 64;
        const float labelW = f.text.Draw(labelX, baseline, theme::kItemSize, label,
                                         (selected ? theme::kCream : theme::kCreamSoft).Alpha(a), theme::kTrackItem);

        if (selected) {
            const float pulse = 0.8f + 0.2f * std::sin(ui.realtime * 0.006f);
            kit::Fill(m - 28, baseline - cap, 4, cap, theme::kEmber.Alpha(a * pulse));
            if (items[i].hint[0]) {
                f.mono.Draw(labelX + labelW + 32, baseline, theme::kMonoSize, items[i].hint,
                            theme::kCreamFaint.Alpha(a), theme::kTrackMono);
            }
        }
        ui.itemRects.push_back({m - 40, baseline - cap - 18, labelX + labelW - m + 60, step});
    }
}

void DrawPointer() {
    if (ui.mouseUsed) {
        kit::Image(ui.mouseX, ui.mouseY, 24, 24, ui.pointer, theme::kCream);
    }
}

void DrawMain() {
    const kit::Fonts& f = kit::GetFonts();
    const float a = OpenFade(), m = theme::kMargin, w = kit::Width(), h = kit::Height();

    kit::Background(true);

    // the mark is the object of the screen, right side
    const float markSize = 600;
    kit::Image(w - m - markSize + 40, (h - markSize) / 2 + 20, markSize, markSize, ui.mark, theme::kCream.Alpha(0.92f * a));

    DrawHeader("SKAAR  ·  AIM / MOVEMENT", kit::Upper(ui.version), a);

    f.display.Draw(m - 6, 336, theme::kStatementSize, "TRAIN THE HAND.", theme::kCream.Alpha(a),
                   theme::kTrackStatement);
    f.text.Draw(m, 400, theme::kLedeSize, "Aim drills and movement, on an arena engine.", theme::kCreamSoft.Alpha(a));

    DrawItems(540, a);

    if (!ui.lastError.empty()) {
        f.mono.Draw(m, h - 148, theme::kMonoSize, "LAST ERROR  ·  " + ui.lastError, theme::kEmber.Alpha(a),
                    theme::kTrackMono * 0.5f);
    }
    DrawFooter("↑ ↓  SELECT     ENTER  CONFIRM     1–3  JUMP     ~  CONSOLE", "GPLV2  ·  BUILT ON IOQUAKE3", a);
    DrawPointer();
}

void DrawInGame() {
    const kit::Fonts& f = kit::GetFonts();
    const float a = OpenFade(), m = theme::kMargin;

    kit::Fill(0, 0, kit::Width(), kit::Height(), theme::kStage.Alpha(0.84f * a));
    DrawHeader("PAUSED  ·  " + kit::Upper(MapName()), kit::Upper(ui.version), a);
    f.display.Draw(m - 4, 296, theme::kTitleSize, "PAUSED", theme::kCream.Alpha(a), theme::kTrackStatement);
    DrawItems(430, a);
    DrawFooter("ESC  RESUME     ↑ ↓  SELECT     ENTER  CONFIRM", "", a);
    DrawPointer();
}

void DrawConnect(bool overlay) {
    if (overlay) {
        return;
    }
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin;

    uiClientState_t cs;
    trap::GetClientState(&cs);

    kit::Background(true);
    DrawHeader("LOADING", kit::Upper(ui.version), 1.0f);
    kit::Image(m - 12, 180, 160, 160, ui.mark, theme::kCream);

    std::string map = Info_ValueForKey(trap::GetConfigString(CS_SERVERINFO).c_str(), "mapname");
    f.display.Draw(m - 4, 470, theme::kTitleSize, kit::Upper(map.empty() ? cs.servername : map), theme::kCream,
                   theme::kTrackStatement);

    const char* status = "";
    switch (cs.connState) {
        case CA_CONNECTING:
        case CA_CHALLENGING:
            status = "CONNECTING";
            break;
        case CA_CONNECTED:
            status = "AWAITING GAMESTATE";
            break;
        case CA_LOADING:
            status = "LOADING MAP";
            break;
        case CA_PRIMED:
            status = "AWAITING FIRST SNAPSHOT";
            break;
        default:
            break;
    }
    std::string line = std::string(status) + "  ·  " + kit::Upper(cs.servername);
    f.mono.Draw(m, 528, theme::kMonoSize, line, theme::kCreamSoft, theme::kTrackMono);
    if (cs.messageString[0]) {
        f.mono.Draw(m, 568, theme::kMonoSize, cs.messageString, theme::kEmber, theme::kTrackMono * 0.5f);
    }
}

// ---- input --------------------------------------------------------------------

void Activate(int index) {
    std::vector<Item>& items = Items();
    if (index >= 0 && index < static_cast<int>(items.size())) {
        ui.cursor = index;
        Com_DPrintf("ui: %s\n", items[index].label);
        items[index].action();
    }
}

void KeyEvent(int key, bool down) {
    Com_DPrintf("ui: key %d %s\n", key, down ? "down" : "up");
    if (!down || (key & K_CHAR_FLAG) || ui.screen == Screen::kNone) {
        return;
    }
    const int count = static_cast<int>(Items().size());
    switch (key) {
        case K_ESCAPE:
            if (ui.screen == Screen::kInGame) {
                Close();
            }
            break;
        case K_UPARROW:
        case K_KP_UPARROW:
            ui.cursor = (ui.cursor + count - 1) % count;
            break;
        case K_DOWNARROW:
        case K_KP_DOWNARROW:
        case K_TAB:
            ui.cursor = (ui.cursor + 1) % count;
            break;
        case K_ENTER:
        case K_KP_ENTER:
            Activate(ui.cursor);
            break;
        case K_MOUSE1:
            for (size_t i = 0; i < ui.itemRects.size(); i++) {
                if (ui.itemRects[i].Contains(ui.mouseX, ui.mouseY)) {
                    Activate(static_cast<int>(i));
                }
            }
            break;
        default:
            if (key >= '1' && key <= '9') {
                Activate(key - '1');
            }
            break;
    }
}

void MouseEvent(int dx, int dy) {
    Com_DPrintf("ui: mouse %d %d\n", dx, dy);
    ui.mouseUsed = true;
    ui.mouseX = std::fmin(std::fmax(0.0f, ui.mouseX + dx / kit::Scale()), kit::Width());
    ui.mouseY = std::fmin(std::fmax(0.0f, ui.mouseY + dy / kit::Scale()), kit::Height());
    for (size_t i = 0; i < ui.itemRects.size(); i++) {
        if (ui.itemRects[i].Contains(ui.mouseX, ui.mouseY)) {
            ui.cursor = static_cast<int>(i);
        }
    }
}

// ---- lifecycle -----------------------------------------------------------------

bool ReadFileForKit(const char* path, std::string& out) { return trap::FileRead(path, out); }

void Init() {
    trap::GetGlconfig(&ui.glconfig);
    kit::Backend backend{};
    backend.registerShader = trap::R_RegisterShaderNoMip;  // scripts/skaar-ui.shader adds mipmaps
    backend.setColor = trap::R_SetColor;
    backend.drawStretchPic = trap::R_DrawStretchPic;
    backend.readFile = ReadFileForKit;
    if (!kit::Init(backend, ui.glconfig.vidWidth, ui.glconfig.vidHeight)) {
        Com_Printf(S_COLOR_YELLOW "WARNING: ui: fonts missing (run tools/gen_assets.py)\n");
    }
    ui.mark = trap::R_RegisterShaderNoMip("gfx/skaar/mark");
    ui.pointer = trap::R_RegisterShaderNoMip("gfx/skaar/cursor");
    ui.version = trap::CvarString("version");
    if (const size_t space = ui.version.find(' '); space != std::string::npos) {
        ui.version = "v" + ui.version.substr(space + 1, ui.version.find(' ', space + 1) - space - 1);
    }
    Com_DPrintf("ui: init %dx%d\n", ui.glconfig.vidWidth, ui.glconfig.vidHeight);
}

void Refresh(int realtime) {
    ui.realtime = realtime;
    if (!(trap::Key_GetCatcher() & KEYCATCH_UI)) {
        return;
    }
    if (ui.screen == Screen::kMain) {
        DrawMain();
    } else if (ui.screen == Screen::kInGame) {
        DrawInGame();
    }
}

void SetActiveMenu(int menu) {
    switch (menu) {
        case UIMENU_NONE:
            Close();
            break;
        case UIMENU_INGAME:
            Open(Screen::kInGame);
            break;
        default:  // UIMENU_MAIN and the legacy ones (cd key, team, postgame)
            Open(Screen::kMain);
            break;
    }
}

intptr_t Dispatch(int command, int arg0, int arg1) {
    switch (command) {
        case UI_GETAPIVERSION:
            return UI_API_VERSION;
        case UI_INIT:
            Init();
            return 0;
        case UI_SHUTDOWN:
            return 0;
        case UI_KEY_EVENT:
            KeyEvent(arg0, arg1 != 0);
            return 0;
        case UI_MOUSE_EVENT:
            MouseEvent(arg0, arg1);
            return 0;
        case UI_REFRESH:
            Refresh(arg0);
            return 0;
        case UI_IS_FULLSCREEN:
            return ui.screen == Screen::kMain;
        case UI_SET_ACTIVE_MENU:
            SetActiveMenu(arg0);
            return 0;
        case UI_CONSOLE_COMMAND:
            return 0;
        case UI_DRAW_CONNECT_SCREEN:
            DrawConnect(arg0 != 0);
            return 0;
        case UI_HASUNIQUECDKEY:
            return 0;
        default:
            Com_Printf(S_COLOR_YELLOW "WARNING: ui: unknown vmMain command %i\n", command);
            return -1;
    }
}

}  // namespace
}  // namespace ui

SKAAR_MODULE_EXPORT intptr_t vmMain(int command, int arg0, int arg1, int, int, int, int, int, int, int, int, int,
                                    int) {
    try {
        return ui::Dispatch(command, arg0, arg1);
    } catch (const std::exception& e) {
        Com_Error(ERR_DROP, "ui: %s", e.what());
    }
}

extern "C" void QDECL Com_Printf(const char* fmt, ...) {
    va_list ap;
    char text[skaar::kMaxPrint];
    va_start(ap, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    ui::trap::Print(text);
}

extern "C" void QDECL Com_DPrintf(const char* fmt, ...) {
    if (ui::trap::CvarString("developer") == "0") {
        return;
    }
    va_list ap;
    char text[skaar::kMaxPrint];
    va_start(ap, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    ui::trap::Print(text);
}

extern "C" void QDECL Com_Error(int level, const char* fmt, ...) {
    (void)level;
    va_list ap;
    char text[skaar::kMaxPrint];
    va_start(ap, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    ui::trap::Error(text);
}
