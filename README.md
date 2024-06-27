# AltaNode
A physical control unit for the Avigilon Alta API powered by NodeMCU.

## Project Details
This project implements an ESP8266 microcontroller (NodeMCU) that provides control of Avigilon Alta entries via API using physical buttons.

## Features:
- Connects to WiFi using credentials stored on the SD card.
- Provides a web interface for configuration of Avigilon Alta API details and WiFi.
- Implements secure access to configuration pages with username and password authentication.
- Saves configuration data (API URL and button entries) to the SD card.
- Provides Over-the-Air (OTA) updates for easy firmware updates.
- Controls entries via button presses that make use of the Avigilon Alta API.

## Hardware Components:
- ESP8266 Microcontroller (NodeMCU)
- SD Card Module & SD Card
- Buttons
- Enclosure

## Software Libraries:
- Arduino core for ESP8266
- ESP8266 WiFi library
- ESPAsyncWebServer library
- ArduinoJson library
- AsyncElegantOTA library
- BearSSL library

## Resources
- [Avigilon Alta API docs](https://openpath.readme.io/)
