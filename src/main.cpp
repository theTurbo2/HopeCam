/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include <Arduino.h>
#include <WiFi.h>
#include "LITTLEFS.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include "WiFiSetup.h"
#include "cam.h"
#include "Webserver.h"

#define ESP_DRD_USE_LITTLEFS    true
#define ESP_DRD_USE_SPIFFS      false
#define ESP_DRD_USE_EEPROM      false
#define DRD_TIMEOUT             10
#define DRD_ADDRESS             0
#define DOUBLERESETDETECTOR_DEBUG       true
#include <ESP_DoubleResetDetector.h>            //https://github.com/khoih-prog/ESP_DoubleResetDetector

#define CAM_ASYNC

AsyncWebServer server(80);

DoubleResetDetector* drd;

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(false)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(19200);

  Serial.print("Runninng Build Version from ");
  Serial.println(CURRENT_TIME) ;

  initLittleFS();

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset()) { 
    //WiFiSetup::resetConfig();  
  }

  WiFiSetup::setup();

  if (WiFiSetup::initWifi(&server))
  {
    CAM::StartCamera();
    WEBSERVER::StartServer(&server);
  }
}

void loop() {
  drd->loop();
  WiFiSetup::loop();
}