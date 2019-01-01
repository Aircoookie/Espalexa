#ifndef Espalexa_h
#define Espalexa_h

#include "Arduino.h"
//#include <WiFiUDP.h>

#ifdef ARDUINO_ARCH_ESP32
#include "dependencies/webserver/WebServer.h"
#else
#include <ESP8266WebServer.h>
#endif

#define ESPALEXA_MAXDEVICES 20 //this limit only has memory reasons, set it higher should you need to

//#define ESPALEXA_DEBUG

#ifdef ESPALEXA_DEBUG
 #define DEBUG(x)  Serial.print (x)
 #define DEBUGLN(x) Serial.println (x)
#else
 #define DEBUG(x)
 #define DEBUGLN(x)
#endif

typedef void (*CallbackBriFunction) (uint8_t br);
typedef void (*CallbackColFunction) (uint8_t br, uint32_t col);

class EspalexaDevice {
private:
  String _deviceName;
  CallbackBriFunction _callback;
  CallbackColFunction _callbackCol;
  uint8_t _val, _val_last, _sat;
  uint16_t _hue, _ct;
  
public:
  EspalexaDevice();
  ~EspalexaDevice();
  EspalexaDevice(String deviceName, CallbackBriFunction gnCallback, uint8_t initialValue =0);
  EspalexaDevice(String deviceName, CallbackColFunction gnCallback, uint8_t initialValue =0);
  
  bool isColorDevice();
  bool isColorTemperatureMode();
  String getName();
  uint8_t getValue();
  uint16_t getHue();
  uint8_t getSat();
  uint16_t getCt();
  uint32_t getColorRGB();
  
  void setValue(uint8_t bri);
  void setPercent(uint8_t perc);
  void setName(String name);
  void setColor(uint16_t hue, uint8_t sat);
  void setColor(uint16_t ct);
  
  void doCallback();
  
  uint8_t getLastValue(); //last value that was not off (1-255)
};

class Espalexa {
private:
  void startHttpServer();
  String deviceJsonString(uint8_t deviceId);
  void handleDescriptionXml();
  void respondToSearch();
  String boolString(bool st);
  bool connectUDP();
  void alexaOn(uint8_t deviceId);
  void alexaOff(uint8_t deviceId);
  void alexaDim(uint8_t deviceId, uint8_t briL);
  void alexaCol(uint8_t deviceId, uint16_t hue, uint8_t sat);
  void alexaCt(uint8_t deviceId, uint16_t ct);
public:
  Espalexa();
  ~Espalexa();
  #ifdef ARDUINO_ARCH_ESP32
  WebServer* server;
  bool begin(WebServer* externalServer=nullptr);
  #else
  ESP8266WebServer* server;
  bool begin(ESP8266WebServer* externalServer=nullptr);
  #endif

  bool addDevice(EspalexaDevice* d);
  bool addDevice(String deviceName, CallbackBriFunction callback, uint8_t initialValue=0);
  bool addDevice(String deviceName, CallbackColFunction callback, uint8_t initialValue=0);
  
  uint8_t toPercent(uint8_t bri);

  void loop();
  
  bool handleAlexaApiCall(String req, String body);
};

#endif

