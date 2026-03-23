/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : sv_render.cpp
 *  Module : OSD rendering engine — green phosphor UI using TFT_eSPI built-in fonts
 * ============================================================
 */

/*
 * sv_render.cpp - OSD rendering engine
 *
 * Green phosphor aesthetic. Uses TFT_eSPI built-in font 2 (16px).
 * Coordinates derived from DISPLAY_WIDTH/DISPLAY_HEIGHT in config.h.
 */

#include "sv_render.h"

static TFT_eSPI* g_tft = nullptr;

void sv_render_init(TFT_eSPI* tft) {
    g_tft = tft;
}

void sv_render_frame(const char* title, const char* footer) {
    if (!g_tft) return;

    g_tft->startWrite();

    // Fill background
    g_tft->fillRect(SV_BORDER_X, SV_BORDER_Y, SV_BORDER_W, SV_BORDER_H, SV_COLOR_BG);

    // Border
    g_tft->drawRect(SV_BORDER_X, SV_BORDER_Y, SV_BORDER_W, SV_BORDER_H, SV_COLOR_BORDER);

    // Title bar (inverse)
    g_tft->fillRect(SV_BORDER_X + 1, SV_BORDER_Y + 1, SV_BORDER_W - 2, SV_TITLE_H, SV_COLOR_TITLE_BG);
    g_tft->setTextFont(2);
    g_tft->setTextColor(SV_COLOR_TITLE_TEXT, SV_COLOR_TITLE_BG);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->drawString(title, SV_BORDER_X + SV_BORDER_W / 2, SV_BORDER_Y + 2);

    // Footer (smaller font)
    if (footer) {
        int footer_y = SV_BORDER_Y + SV_BORDER_H - SV_FOOTER_H - 1;
        g_tft->drawFastHLine(SV_BORDER_X + 1, footer_y, SV_BORDER_W - 2, SV_COLOR_DIM);
        g_tft->setTextFont(1);
        g_tft->setTextColor(SV_COLOR_DIM, SV_COLOR_BG);
        g_tft->setTextDatum(TC_DATUM);
        g_tft->drawString(footer, SV_BORDER_X + SV_BORDER_W / 2, footer_y + 4);
    }

    g_tft->setTextDatum(TL_DATUM);
    g_tft->endWrite();
}

void sv_render_menu_item(int index, const char* label, const char* value,
                         bool highlighted) {
    if (!g_tft) return;

    int y = SV_CONTENT_Y + index * SV_ITEM_H;
    int x = SV_CONTENT_X;

    g_tft->startWrite();

    if (highlighted) {
        g_tft->fillRect(SV_BORDER_X + 2, y, SV_BORDER_W - 4, SV_ITEM_H, SV_COLOR_HIGHLIGHT);
        g_tft->setTextColor(SV_COLOR_HL_TEXT, SV_COLOR_HIGHLIGHT);
    } else {
        g_tft->fillRect(SV_BORDER_X + 2, y, SV_BORDER_W - 4, SV_ITEM_H, SV_COLOR_BG);
        g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_BG);
    }

    g_tft->setTextFont(2);
    g_tft->setTextDatum(TL_DATUM);

    // Cursor glyph
    if (highlighted) {
        g_tft->drawString(">", x - 2, y + 2);
    }

    // Label
    g_tft->drawString(label, x + 10, y + 2);

    // Value (right-aligned)
    if (value) {
        g_tft->setTextDatum(TR_DATUM);
        g_tft->drawString(value, SV_VALUE_RIGHT, y + 2);
        g_tft->setTextDatum(TL_DATUM);
    }

    g_tft->endWrite();
}

void sv_render_file_entry(int index, const char* name, uint32_t size,
                          bool is_dir, bool is_supported, bool highlighted) {
    if (!g_tft) return;

    int y = SV_CONTENT_Y + index * SV_ITEM_H;
    int x = SV_CONTENT_X;

    g_tft->startWrite();

    if (highlighted) {
        g_tft->fillRect(SV_BORDER_X + 2, y, SV_BORDER_W - 4, SV_ITEM_H, SV_COLOR_HIGHLIGHT);
        g_tft->setTextColor(SV_COLOR_HL_TEXT, SV_COLOR_HIGHLIGHT);
    } else {
        g_tft->fillRect(SV_BORDER_X + 2, y, SV_BORDER_W - 4, SV_ITEM_H, SV_COLOR_BG);
        g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_BG);
    }

    g_tft->setTextFont(2);
    g_tft->setTextDatum(TL_DATUM);

    // Cursor
    if (highlighted) {
        g_tft->drawString(">", x - 2, y + 2);
    }

    // Icon character
    uint16_t icon_color;
    const char* icon;
    if (is_dir) {
        icon = "+";
        icon_color = highlighted ? SV_COLOR_HL_TEXT : SV_COLOR_DIR;
    } else if (is_supported) {
        icon = "*";
        icon_color = highlighted ? SV_COLOR_HL_TEXT : SV_COLOR_DISK;
    } else {
        icon = "!";
        icon_color = highlighted ? SV_COLOR_HL_TEXT : SV_COLOR_WARN;
    }

    uint16_t saved_fg = g_tft->textcolor;
    uint16_t saved_bg = g_tft->textbgcolor;
    g_tft->setTextColor(icon_color, highlighted ? SV_COLOR_HIGHLIGHT : SV_COLOR_BG);
    g_tft->drawString(icon, x + 10, y + 2);
    g_tft->setTextColor(saved_fg, saved_bg);

    // Filename
    g_tft->drawString(name, x + 22, y + 2);

    // File size (right-aligned, not for dirs)
    if (!is_dir) {
        char size_str[16];
        if (size >= 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%lu MB", size / (1024 * 1024));
        } else {
            snprintf(size_str, sizeof(size_str), "%lu KB", size / 1024);
        }
        g_tft->setTextDatum(TR_DATUM);
        g_tft->drawString(size_str, SV_VALUE_RIGHT, y + 2);
        g_tft->setTextDatum(TL_DATUM);
    }

    g_tft->endWrite();
}

void sv_render_scrollbar(int visible_start, int visible_count, int total_count) {
    if (!g_tft || total_count <= visible_count) return;

    int sb_x = SV_BORDER_X + SV_BORDER_W - 4;
    int sb_y = SV_CONTENT_Y;
    int sb_h = visible_count * SV_ITEM_H;

    // Background
    g_tft->drawFastVLine(sb_x, sb_y, sb_h, SV_COLOR_BG);

    // Thumb
    int thumb_h = (visible_count * sb_h) / total_count;
    if (thumb_h < 4) thumb_h = 4;
    int thumb_y = sb_y + (visible_start * sb_h) / total_count;

    g_tft->drawFastVLine(sb_x, thumb_y, thumb_h, SV_COLOR_DIM);
}

void sv_render_status_line(const char* text, uint16_t color) {
    if (!g_tft) return;

    int y = SV_CONTENT_Y + 8 * SV_ITEM_H;
    g_tft->startWrite();
    g_tft->fillRect(SV_CONTENT_X, y, SV_CONTENT_W, SV_ITEM_H, SV_COLOR_BG);
    g_tft->setTextFont(2);
    g_tft->setTextColor(color, SV_COLOR_BG);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->drawString(text, SV_BORDER_X + SV_BORDER_W / 2, y + 2);
    g_tft->setTextDatum(TL_DATUM);
    g_tft->endWrite();
}

void sv_render_confirm_dialog(const char* message, bool yes_highlighted) {
    if (!g_tft) return;

    int dw = 200, dh = 80;
    int dx = (DISPLAY_WIDTH - dw) / 2;
    int dy = (DISPLAY_HEIGHT - dh) / 2;

    g_tft->startWrite();
    g_tft->fillRect(dx, dy, dw, dh, SV_COLOR_DIALOG_BG);
    g_tft->drawRect(dx, dy, dw, dh, SV_COLOR_BORDER);

    g_tft->setTextFont(2);
    g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_DIALOG_BG);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->drawString(message, dx + dw / 2, dy + 10);

    // Yes button
    int btn_y = dy + 45;
    if (yes_highlighted) {
        g_tft->fillRect(dx + 20, btn_y, 60, 20, SV_COLOR_HIGHLIGHT);
        g_tft->setTextColor(SV_COLOR_HL_TEXT, SV_COLOR_HIGHLIGHT);
    } else {
        g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_DIALOG_BG);
    }
    g_tft->drawString("Yes", dx + 50, btn_y + 2);

    // No button
    if (!yes_highlighted) {
        g_tft->fillRect(dx + 120, btn_y, 60, 20, SV_COLOR_HIGHLIGHT);
        g_tft->setTextColor(SV_COLOR_HL_TEXT, SV_COLOR_HIGHLIGHT);
    } else {
        g_tft->setTextColor(SV_COLOR_TEXT, SV_COLOR_DIALOG_BG);
    }
    g_tft->drawString("No", dx + 150, btn_y + 2);

    g_tft->setTextDatum(TL_DATUM);
    g_tft->endWrite();
}

void sv_render_centered_item(int index, const char* text, uint16_t color) {
    if (!g_tft) return;
    int y = SV_CONTENT_Y + index * SV_ITEM_H;
    g_tft->startWrite();
    g_tft->fillRect(SV_BORDER_X + 2, y, SV_BORDER_W - 4, SV_ITEM_H, SV_COLOR_BG);
    g_tft->setTextFont(2);
    g_tft->setTextColor(color, SV_COLOR_BG);
    g_tft->setTextDatum(TC_DATUM);
    g_tft->drawString(text, SV_BORDER_X + SV_BORDER_W / 2, y + 2);
    g_tft->setTextDatum(TL_DATUM);
    g_tft->endWrite();
}

void sv_render_clear_content(void) {
    if (!g_tft) return;
    g_tft->fillRect(SV_BORDER_X + 2, SV_CONTENT_Y,
                    SV_BORDER_W - 4, SV_BORDER_H - SV_TITLE_H - SV_FOOTER_H - 6,
                    SV_COLOR_BG);
}
