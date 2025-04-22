namespace WiFiSetup 
{
  #define WIFI_STARTUP_NORMAL 0
  #define WIFI_STARTUP_AP_ALWAYS 1

  String getSSID();
  String getPASS();
  String getIP();
  String getGATEWAY();
  String getNETMASK();
  String getNODENAME();
  bool getDHCP();

  void setSSID(String);
  void setPASS(String);
  void setIP(String);
  void setGATEWAY(String);
  void setNETMASK(String);
  void setNODENAME(String);
  void setDHCP(bool);

  void setup(int mode);
  bool initWifi(AsyncWebServer *server);
  bool checkSetup();
  void runConfigServer(AsyncWebServer *server);
  void resetConfig();
  void loop();

  String readFile(fs::FS& fs, const char* path);
  void writeFile(fs::FS& fs, const char* path, const char* message);
}