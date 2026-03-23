/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : sv_render.h
 *  Module : OSD rendering engine interface
 * ============================================================
 */

/*
 * sv_render.h - OSD rendering engine for supervisor
 *
 * CoCo green phosphor aesthetic. Renders directly to TFT.
 */

#ifndef SV_RENDER_H
#define SV_RENDER_H

#include <stdint.h>
#include <TFT_eSPI.h>

// Color theme — blue
#define SV_COLOR_BG          0x0000  // black
#define SV_COLOR_TEXT         0xFFE0  //Yellow 
#define SV_COLOR_HIGHLIGHT   0x5DDF  // Bright blue-white bg
#define SV_COLOR_HL_TEXT     0xFFE0  // Yellow
#define SV_COLOR_DIM         0xFFFF  // white
#define SV_COLOR_BORDER      0x5DDF  // Bright blue-white
#define SV_COLOR_TITLE_BG    0x5DDF
#define SV_COLOR_TITLE_TEXT  0xFFFF // whithe 
#define SV_COLOR_DIR         0xFFE0  // Yellow
#define SV_COLOR_DISK        0x07FF  // Cyan
#define SV_COLOR_WARN        0xF800  // Red
#define SV_COLOR_DIALOG_BG   0x0000  // Black

// Layout constants — centered on display, derived from config.h
#include "../../config.h"

// OSD area: when scaled, use more of the display; when 1:1, match VDG area
#if DISPLAY_SCALE_MODE == 1
  // Scaled: use ~90% of display, centered
  #define SV_BORDER_W     (DISPLAY_WIDTH * 9 / 10)
  #define SV_BORDER_H     (DISPLAY_HEIGHT * 9 / 10)
  #define SV_ITEM_H       ((SV_BORDER_H - 40) / 10)  // fit ~8-9 items + title/footer
#else
  // 1:1: match the 256x192 VDG sprite area
  #define SV_BORDER_W     256
  #define SV_BORDER_H     192
  #define SV_ITEM_H       18
#endif
#define SV_BORDER_X     ((DISPLAY_WIDTH - SV_BORDER_W) / 2)
#define SV_BORDER_Y     ((DISPLAY_HEIGHT - SV_BORDER_H) / 2)
#define SV_TITLE_H      18
#define SV_FOOTER_H     16
#define SV_CONTENT_X    (SV_BORDER_X + 8)
#define SV_CONTENT_Y    (SV_BORDER_Y + SV_TITLE_H + 4)
#define SV_CONTENT_W    (SV_BORDER_W - 16)
#define SV_VALUE_RIGHT  (SV_BORDER_X + SV_BORDER_W - 8)

void sv_render_init(TFT_eSPI* tft);
void sv_render_frame(const char* title, const char* footer);
void sv_render_menu_item(int index, const char* label, const char* value,
                         bool highlighted);
void sv_render_file_entry(int index, const char* name, uint32_t size,
                          bool is_dir, bool is_supported, bool highlighted);
void sv_render_scrollbar(int visible_start, int visible_count, int total_count);
void sv_render_status_line(const char* text, uint16_t color);
void sv_render_confirm_dialog(const char* message, bool yes_highlighted);
void sv_render_clear_content(void);
void sv_render_centered_item(int index, const char* text, uint16_t color);

#endif // SV_RENDER_H
