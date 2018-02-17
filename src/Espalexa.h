#ifndef Espalexa_h
#define Espalexa_h

#include "Arduino.h"
//#include <WiFiUDP.h>

#ifdef ARDUINO_ARCH_ESP32
#include "dependencies/webserver/WebServer.h"
#else
#include <ESP8266WebServer.h>
#endif

typedef void (*CallbackBriFunction) (uint8_t br);

class EspalexaDevice {
private:
		String _deviceName;
		CallbackBriFunction _callback;
		uint8_t _val, _val_last;
public:
		EspalexaDevice();
		~EspalexaDevice();
		EspalexaDevice(String deviceName, CallbackBriFunction gnCallback, uint8_t initialValue =0);
		
		String getName();
		uint8_t getValue();
		
		void setValue(uint8_t bri);
		
		void doCallback();
		
		uint8_t getLastValue(); //last value that was not off (1-255)
};

class Espalexa {
private:
		String escapedMac;
		bool udpConnected;
		
		void startHttpServer();
		String deviceJsonString(uint8_t deviceId);
        void handleDescriptionXml();
		void respondToSearch();
		String boolString(bool st);
		bool connectUDP();
		void alexaOn(uint8_t deviceId);
		void alexaOff(uint8_t deviceId);
		void alexaDim(uint8_t deviceId, uint8_t briL);
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

		void loop();
		
		bool handleAlexaApiCall(String req, String body);
};

#endif

