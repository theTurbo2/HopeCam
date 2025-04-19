/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"

#include "WiFiSetup.h"
#include "cam.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  initLittleFS();

  WiFiSetup::setup(WIFI_STARTUP_AP_ALWAYS);
  //WiFiSetup::setup(WIFI_STARTUP_NORMAL);

  if (WiFiSetup::initWifi(&server)) 
  {

    Cam::setup();

    //install page handlers
    Serial.println("install handlers");

    server.serveStatic("/", LittleFS, "/");

    server.begin();
  }
}

void loop() {
}