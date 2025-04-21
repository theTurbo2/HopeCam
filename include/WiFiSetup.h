namespace WiFiSetup 
{
  #define WIFI_STARTUP_NORMAL 0
  #define WIFI_STARTUP_AP_ALWAYS 1

  static String ssid;
  static String pass;
  static String ip;
  static String gateway;
  static String nodename;

  void setup(int mode);
  bool initWifi(AsyncWebServer *server);
  bool checkSetup();
  void runConfigServer(AsyncWebServer *server);
  void resetConfig();
  void loop();

  String readFile(fs::FS& fs, const char* path);
  void writeFile(fs::FS& fs, const char* path, const char* message);
}