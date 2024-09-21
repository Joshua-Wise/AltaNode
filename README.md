# AltaNode
A physical control unit for the Avigilon Alta API powered by NodeMCU.

<img src="https://jwise.dev/content/images/size/w1000/2024/06/altanode.png" width="500" >

### Project Details
This project implements an ESP8266 microcontroller (NodeMCU) that provides control of Avigilon Alta entries via API using physical buttons.

### Features:
- Connects to WiFi using credentials stored on the SD card.
- Provides a web interface for configuration of Avigilon Alta API details and WiFi.
- Implements secure access to configuration pages with username and password authentication.
- Saves configuration data (API URL and button entries) to the SD card.
- Encrypts SD card data & stores key in EEPROM via AES
- Provides Over-the-Air (OTA) updates for easy firmware updates.
- Controls entries via button presses that make use of the Avigilon Alta API.

### Hardware Components:
- ESP8266 Microcontroller (NodeMCU)
- SD Card Module & SD Card
- Buttons
- Enclosure

### Software Libraries:
- Arduino core for ESP8266
- ESP8266 WiFi library
- ESPAsyncWebServer library
- ArduinoJson library
- AsyncElegantOTA library
- BearSSL library
- AES library
- EEPROM library

### Resources
- [Avigilon Alta API docs](https://openpath.readme.io/)
- [Project Blog Post](https://jwise.dev/aviligon-alta-api/)

### Amazon Hardware List
- [Enclosure](https://www.amazon.com/uxcell-Button-Control-Station-Aperture/dp/B07WKJM1NJ)
- [Buttons](https://www.amazon.com/Waterproof-Momentary-Mushroom-Terminal-EJ22-241A/dp/B098FGVVFZ)
- [NodeMCU](https://www.amazon.com/HiLetgo-Internet-Development-Wireless-Micropython/dp/B081CSJV2V)
- [SD Card Reader](https://www.amazon.com/dp/B0B7WZQVHS)
- [Panel Mount Micro-USB (optional)](https://www.amazon.com/QIANRENON-Threaded-Charging-Connector-Dashboard/dp/B0D17N33TX)
- [Breakout Board (optional)](https://www.amazon.com/dp/B0B85C98FX)
- [JST Connectors (optional)](https://www.amazon.com/dp/B076HLQ4FX)
- [6-pin Dupont (optional)](https://www.amazon.com/dp/B0B8YWWXS3)
