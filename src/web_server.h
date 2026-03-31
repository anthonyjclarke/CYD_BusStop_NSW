#pragma once

// Initialise AsyncWebServer, attach routes and ElegantOTA.
// Call after WiFi is connected.
void initWebServer();

// Call in loop() — required for ElegantOTA progress tracking.
void handleWebServer();
