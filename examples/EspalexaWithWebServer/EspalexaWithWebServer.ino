/*
 * This is an example on how to use Espalexa alongside an ESP8266WebServer.
 */ 
#include <Espalexa.h>
 #ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include <WebServer.h> //if you get an error here please update to ESP32 arduino core 1.0.0
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

// prototypes
boolean connectWifi();

//callback functions
void firstLightChanged(uint8_t brightness);

// Change this!!
const char* ssid = "...";
const char* password = "wifipassword";

boolean wifiConnected = false;

Espalexa espalexa;
#ifdef ARDUINO_ARCH_ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif

void setup()
{
  Serial.begin(115200);
  // Initialise wifi connection
  wifiConnected = connectWifi();
  
  if(wifiConnected){
    server.on("/", HTTP_GET, [](){
    server.send(200, "text/plain", "This is an example index page your server may send.");
    });
    server.on("/test", HTTP_GET, [](){
    server.send(200, "text/plain", "This is a second subpage you may have.");
    });
    server.onNotFound([](){
      if (!espalexa.handleAlexaApiCall(&server)) //if you don't know the URI, ask espalexa whether it is an Alexa control request
      {
        //whatever you want to do with 404s
        server.send(404, "text/plain", "Not found");
      }
    });

    // Define your devices here.
    espalexa.addDevice("My Light 1", firstLightChanged); //simplest definition, default state off

    espalexa.begin(&server); //give espalexa a pointer to your server object so it can use your server instead of creating its own
    //server.begin(); //omit this since it will be done by espalexa.begin(&server)
  } else
  {
    while (1)
    {
      Serial.println("Cannot connect to WiFi. Please check data and reset the ESP.");
      delay(2500);
    }
  }
}
 
void loop()
{
   //server.handleClient() //you can omit this line from your code since it will be called in espalexa.loop()
   espalexa.loop();
   delay(1);
}

//our callback functions
void firstLightChanged(uint8_t brightness) {
    Serial.print("Device 1 changed to ");
    
    //do what you need to do here

    //EXAMPLE
    if (brightness == 255) {
      Serial.println("ON");
    }
    else if (brightness == 0) {
      Serial.println("OFF");
    }
    else {
      Serial.print("DIM "); Serial.println(brightness);
    }
}

// connect to wifi – returns true if successful or false if not
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false; break;
    }
    i++;
  }
  Serial.println("");
  if (state){
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("Connection failed.");
  }
  delay(100);
  return state;
}
