## Espalexa allows you to easily control your ESP with the Alexa voice assistant.
Now compatible with both ESP8266 and ESP32!

#### What does this do similar projects like Fauxmo don't already?

It allows you to set a ranged value (e.g. Brightness) additionally to standard on/off control.
You can say "Alexa, turn the light to 75%".
If you just need On/Off (eg. for a relay) I'd recommend [arduino-esp8266-alexa-wemo-switch](https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch) instead.


Espalexa emulates parts of the SSDP protocol and the Philips hue API, just enough so it can be discovered and controlled by Alexa.
This sketch is basically cobbled together from:
- [arduino-esp8266-alexa-wemo-switch](https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch) by kakopappa (Foundation)
- [ESP8266HueEmulator](https://github.com/probonopd/ESP8266HueEmulator) by probonopd
- Several hours of forensic SSDP and Hue protocol Wireshark work

This is a more generalized version of the file wled12_alexa.ino in my main ESP lighting project [WLED](https://github.com/Aircoookie/WLED).

Espalexa only works with a genuine Echo device, it probably wont work with Echo emulators or RPi homebrew devices.
You only need the src/dependencies folder if you compile for ESP32!

### Usage:

1. After downloading, fill in your WiFi information in Espalexa.ino

2. The default is 3, but depending on how many Alexa devices you'd like to emulate, just change the DEVICES define in line 24 and add/remove name and initial state array entries from line 84 accordingly!

3. Manually compile and upload the Espalexa.ino sketch to your ESP board via the Arduino IDE. Wait until sketch is fully flashed.

4. Tell Alexa to discover devices!

5. This sketch doesn't do anything useful on its own, but you can easily adapt the actionOn/Off/Dim functions to control a PWM led, for example.