#pragma once

// Initialise AsyncWebServer routes and ArduinoOTA.
// Call after WiFi is connected.
void initWebServer();

// Call in loop() — drives ArduinoOTA.handle().
void handleWebServer();

// Returns true once for each queued stop-data refresh requested by the WebUI.
bool consumeStopRefreshRequest();
