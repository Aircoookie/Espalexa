## Espalexa allows you to easily control your ESP with the Alexa voice assistant.
It comes in an easy to use Arduino library or as a standalone example sketch.
Now compatible with both ESP8266 and ESP32!

#### What does this do similar projects like Fauxmo don't already?

It allows you to set a ranged value (e.g. Brightness) additionally to standard on/off control.
You can say "Alexa, turn the light to 75%".
If you just need On/Off (eg. for a relay) I'd recommend [arduino-esp8266-alexa-wemo-switch](https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch) instead.

Additionally, it's possible to add up to a total of 20 devices.
Each device has a brightness range from 0 to 255, where 0 is off and 255 is fully on.

#### How do I install the library?

It's a standard Arduino library. Just download it and add it as ZIP library in the IDE.

#### What has to be done to use it?

Espalexa is designed to be as simple to use as possible.

First, you'll need a global object declaration and a prototype for the function that Espalexa will call when the device is changed:
```cpp
#include <Espalexa.h>

void firstDeviceChanged(uint8_t brightness);

Espalexa espalexa;
```

You then want to actually add the callback function (one for each device)
```cpp
void firstDeviceChanged(uint8_t brightness) {
  //brightness parameter contains the new device state (0:off,255:on,1-254:dimmed)
  
  //do what you'd like to happen here (e.g. control an LED)
}
```

In your setup function, after you connected to WiFi, you'd want to add your devices:
```cpp
espalexa.addDevice("Alexa name of the device", firstDeviceChanged);
```
The first parameter of the function is a string with the invocation name, the second is the name of your callback function (the one Espalexa will call when the state of the device was changed)
You may also add a third `uint8_t` parameter that will specify the default brightness at boot.

Below the device definition in setup, add:
```cpp
espalexa.begin();
```

Finally, in the loop() function, add:
```cpp
espalexa.loop();
```

And that's it!


There is a second way to add devices which is more complicated, but allows you to update device values yourself.
In global:
```cpp
EspalexaDevice* d;
```
In setup:
```cpp
d = new EspalexaDevice("Alexa name of the device", firstDeviceChanged);
espalexa.addDevice(d);
```
As you can see, `EspalexaDevice` takes the same parameters. However, you can now do stuff like:
```cpp
d->setValue(22);
uint8_t bri = d->getValue(); //bri will have the device value
String name = d->getName(); //just in case you forget it
```

You can find a complete example implementation in the examples folder. Just change your WiFi info and try it out!

Espalexa uses an internal WebServer. You can got to `http://[yourEspIP]/espalexa` to see all devices and their current state.

#### My devices are not found?!

Confirm your ESP is connected. Go to the /espalexa subpage to confirm all your devices are defined.
Check your router configuration. Espalexa might need to have UPnP enabled for discovery to work.
Then ask Alexa to discover devices again or try it via the Alexa app.
If nothing helps, open a Github issue and we will help.

#### The devices are found but I can't control them! They are always on!

Please try using ESP8266 Arduino core version 2.3.0.
If you have to use 2.4.0, see this [workaround](https://github.com/Aircoookie/Espalexa/issues/6#issuecomment-366533897).

#### I tried to use this in my sketch that already uses an ESP8266WebServer, it doesn't work!

Unfortunately, it is only possible to have one WebServer per network port. Both common browsers and Espalexa need to use port 80.
The workaround is to have Espalexa use your server object instead of creating its own.
See the example `EspalexaWithWebServer` for the complete implementation.

In short, remove `server.handleClient()` and `server.begin()` from your code.
Then, change `espalexa.begin()` to `espalexa.begin(&server)`.
Finally, add this piece of code below your `server.on()` page definitions:
```cpp
server.onNotFound([](){
	if (!espalexa.handleAlexaApiCall(server.uri(),server.arg(0)))
	{
		server.send(404, "text/plain", "Not found");
	}
});
```

#### How does this work?

Espalexa emulates parts of the SSDP protocol and the Philips hue API, just enough so it can be discovered and controlled by Alexa.
This sketch is basically cobbled together from:
- [arduino-esp8266-alexa-wemo-switch](https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch) by kakopappa (Foundation)
- [ESP8266HueEmulator](https://github.com/probonopd/ESP8266HueEmulator) by probonopd
- Several hours of forensic SSDP and Hue protocol Wireshark work

This is a more generalized version of the file wled12_alexa.ino in my main ESP lighting project [WLED](https://github.com/Aircoookie/WLED).

Espalexa only works with a genuine Echo device, it probably wont work with Echo emulators or RPi homebrew devices.
You only need the src/dependencies folder if you compile for ESP32!