// Tabelle der ESP32-P4-Boards – siehe board_p4.h. Neues Board: Eintrag unten anfuegen.
#include "board_p4.h"

#if BOARD_P4
#include <Preferences.h>
#include <string.h>
#include <string>

// ---------------------------------------------------------------- ELECROW CrowPanel Advanced 7"
// ESP32-P4 + ESP32-C6, 1024x600 IPS, MIPI-DSI 2 Lanes (EK79007), Touch GT911.
// Quelle: Elecrow-RD/CrowPanel-Advanced-7inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen
//   example/V1.x/ESPHome/Code/advance-p4-7-inch.yaml, Arduino_Code/Lesson07 (board_config.h, esp_panel_board_custom_conf.h)
// Unterschiede: V1.0 -> V1.1 SDIO D0..D3 zum C6 umgekehrt; V1.2 Funkmodul-Steckplatz 53/54 -> 27/28 (hier nicht genutzt).
#define CROWPANEL7(ID, NAME, D0, D1, D2, D3)                                                              \
  {ID, NAME, P4_LCD_EK79007, 1024, 600, 2, 1000, 52, 10, 160, 160, 1, 23, 12, /*lcd_rst*/ 41,             \
   /*LDO MIPI*/ 3, 2500, /*LDO aux*/ 4, 2500, /*bl_pwm*/ 31, /*bl_power*/ 29, true, P4_TOUCH_GT911,        \
   /*SDA*/ 45, /*SCL*/ 46, /*INT*/ 42, /*RST*/ 40, /*C6 CLK*/ 18, /*CMD*/ 19, D0, D1, D2, D3, /*C6 RST*/ 32, \
   /*SD CMD*/ 44, /*CLK*/ 43, /*D0*/ 39}

const P4BoardDef P4_BOARDS[] = {
    CROWPANEL7("crowpanel7-v10", "ELECROW CrowPanel Advanced 7\" (V1.0)", 14, 15, 16, 17),
    CROWPANEL7("crowpanel7-v11", "ELECROW CrowPanel Advanced 7\" (V1.1)", 17, 16, 15, 14),
    CROWPANEL7("crowpanel7-v12", "ELECROW CrowPanel Advanced 7\" (V1.2)", 17, 16, 15, 14),
    // Weitere P4-Boards hier anfuegen (gleiche Chip-Revision -> kein Framework-Neubau noetig)
};
const size_t P4_BOARD_COUNT = sizeof(P4_BOARDS) / sizeof(P4_BOARDS[0]);

const P4BoardDef *p4_board_find(const char *id) {
  for (size_t i = 0; i < P4_BOARD_COUNT; i++)
    if (!strcmp(P4_BOARDS[i].id, id)) return &P4_BOARDS[i];
  return nullptr;
}

const P4BoardDef &p4_board() {
  static const P4BoardDef *s_cur = nullptr;
  if (s_cur) return *s_cur;
  Preferences p;
  p.begin("panel", true);
  std::string id = p.getString("p4_board", P4_DEFAULT_BOARD).c_str();
  p.end();
  s_cur = p4_board_find(id.c_str());
  if (!s_cur) s_cur = p4_board_find(P4_DEFAULT_BOARD);
  if (!s_cur) s_cur = &P4_BOARDS[0];
  return *s_cur;
}

bool p4_board_select(const char *id) {
  if (!p4_board_find(id)) return false;
  Preferences p;
  p.begin("panel", false);
  p.putString("p4_board", id);
  p.end();
  return true;
}
#endif
