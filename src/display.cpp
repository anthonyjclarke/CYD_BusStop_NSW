#include "display.h"
#include "bus_api.h"
#include "../include/config.h"
#include "../include/debug.h"

// Using TFT_eSPI built-in fonts:
//   Font 4 (26px) — header time
//   Font 2 (16px) — all other text
// Upgrade path: generate NotoSansBold VLW fonts via Processing and swap in.

// ---------------------------------------------------------------------------
// Colour palette
// ---------------------------------------------------------------------------
#define COL_BG        TFT_BLACK
#define COL_HEADER_FG TFT_WHITE
#define COL_DATE_FG   0xC618       // light grey (RGB565)
#define COL_STOP_NAME TFT_CYAN
#define COL_ROUTE     TFT_WHITE
#define COL_TIME_FG   TFT_WHITE
#define COL_MINS_NEAR TFT_GREEN    // < 10 min
#define COL_MINS_FAR  TFT_YELLOW   // >= 10 min
#define COL_DIVIDER   0x2104       // dark grey (RGB565)
#define COL_STATUS_BG 0x2104

// ---------------------------------------------------------------------------
// Layout
// Landscape: 320×240
// Panels:  [0] top-left   [1] top-right
//          [2] bot-left   [3] bot-right
// ---------------------------------------------------------------------------
static const int PANEL_X[4] = { 0,      PANEL_W, 0,      PANEL_W };
static const int PANEL_Y[4] = { HEADER_H, HEADER_H,
                                 HEADER_H + PANEL_H, HEADER_H + PANEL_H };

#define ROW_NAME_H  18   // px — stop name row height
#define ROW_DEP_H   26   // px — each departure row
#define PAD_X        4   // px — left padding inside panel

TFT_eSPI tft = TFT_eSPI();

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

static void drawDividers() {
  tft.drawFastHLine(0,      HEADER_H - 1,        320, COL_DIVIDER);  // below header
  tft.drawFastHLine(0,      HEADER_H + PANEL_H,  320, COL_DIVIDER);  // mid horizontal
  tft.drawFastVLine(PANEL_W, HEADER_H,            240, COL_DIVIDER);  // mid vertical
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void initDisplay() {
  tft.init();
  tft.setRotation(1);  // landscape
  tft.fillScreen(COL_BG);

  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, BRIGHTNESS_DEFAULT);

  drawDividers();
  DBG_INFO("Display initialised — %dx%d", tft.width(), tft.height());
}

void setBrightness(uint8_t brightness) {
  ledcWrite(0, brightness);
}

void drawHeader(const char* timeStr, const char* dateStr) {
  tft.fillRect(0, 0, 320, HEADER_H - 1, COL_BG);

  // Time — Font 4 (26px), left-aligned, 1px top margin
  tft.setTextColor(COL_HEADER_FG, COL_BG);
  tft.drawString(timeStr, PAD_X, 1, 4);

  // Date — Font 2 (16px), right-aligned, vertically centred in header
  tft.setTextColor(COL_DATE_FG, COL_BG);
  tft.drawRightString(dateStr, 320 - PAD_X, 6, 2);
}

void drawStopPanel(uint8_t idx) {
  if (idx >= STOP_COUNT) return;

  int px = PANEL_X[idx];
  int py = PANEL_Y[idx];

  tft.fillRect(px, py, PANEL_W - 1, PANEL_H - 1, COL_BG);

  // Stop name — Font 2, cyan
  tft.setTextColor(COL_STOP_NAME, COL_BG);
  tft.drawString(STOP_NAMES[idx], px + PAD_X, py + 2, 2);

  const StopData& sd = stopData[idx];

  if (!sd.valid || sd.count == 0) {
    tft.setTextColor(TFT_DARKGREY, COL_BG);
    tft.drawString("No data", px + PAD_X, py + ROW_NAME_H + 4, 2);
    return;
  }

  for (uint8_t i = 0; i < sd.count && i < DEPARTURES_PER_STOP; i++) {
    const Departure& dep = sd.departures[i];
    int rowY = py + ROW_NAME_H + (i * ROW_DEP_H) + 4;

    // Route
    tft.setTextColor(COL_ROUTE, COL_BG);
    tft.drawString(dep.route, px + PAD_X, rowY, 2);

    // Minutes until — colour-coded, "Now" for 0
    char minsStr[8];
    if (dep.minutesUntil <= 0)
      strncpy(minsStr, "Now", sizeof(minsStr));
    else
      snprintf(minsStr, sizeof(minsStr), "%dm", dep.minutesUntil);
    tft.setTextColor((dep.minutesUntil <= 0) ? TFT_RED
                     : (dep.minutesUntil < 10) ? COL_MINS_NEAR : COL_MINS_FAR, COL_BG);
    tft.drawString(minsStr, px + PAD_X + 36, rowY, 2);

    // Clock time
    tft.setTextColor(COL_TIME_FG, COL_BG);
    tft.drawString(dep.clockTime, px + PAD_X + 80, rowY, 2);
  }
}

void drawAllStops() {
  for (uint8_t i = 0; i < STOP_COUNT; i++) {
    drawStopPanel(i);
  }
  drawDividers();  // redraw after fillRect ops may clip divider lines
}

void drawStatusBar(const char* msg, uint16_t colour) {
  tft.fillRect(0, 220, 320, 20, COL_STATUS_BG);
  tft.setTextColor(colour, COL_STATUS_BG);
  tft.drawString(msg, PAD_X, 222, 2);
}
