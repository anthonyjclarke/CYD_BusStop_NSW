#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

// Call once in setup() — init TFT and backlight
void initDisplay();

// Set backlight brightness (0–255)
void setBrightness(uint8_t brightness);

// Redraw the time/date header bar
void drawHeader(const char* timeStr, const char* dateStr);

// Redraw all four stop panels from current stopData[]
void drawAllStops();

// Redraw a single stop panel. idx = 0–3.
void drawStopPanel(uint8_t idx);

// Draw a status message at the bottom of the screen (used during init)
void drawStatusBar(const char* msg, uint16_t colour);

// Draw last-fetch time in dim grey, bottom-right corner ("upd HH:MM").
// Call after every drawAllStops() — panels clear the area, this repaints it.
void drawLastUpdated(const char* timeStr);
