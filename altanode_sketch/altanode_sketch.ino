// Import required libraries
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

// Authentication credentials for web access
const char* http_username = "admin";
const char* http_password = "altanode";

// Wireless credential variables
String ssid;
String password;

// SD card chip select for arduino pin
const int chipSelect = 15;

// Variables to store altanode configuration from json
String apiUrl;
int entryValues[4];  // Array to store values for entries 1-4 (0-3)

// Location of setup files
const char *setupfile = "/config/setup.json";
const char *wififile = "/config/wifi.json";

// Configure buttons pins
const int buttonPins[] = {D1, D2, D3, D4}; // Array of button pins

//initiate webserver
AsyncWebServer server(80);

void webRestart(AsyncWebServerRequest *request) {
  Serial.println();
  Serial.println("Restarting...");

  ESP.restart();
}

void saveSetup(AsyncWebServerRequest *request) {
  // Get new data from setup page
  String new_apiUrl = request->getParam("webapiurl")->value();
  String str_entries1 = request->getParam("webentry1")->value();
  String str_entries2 = request->getParam("webentry2")->value();
  String str_entries3 = request->getParam("webentry3")->value();
  String str_entries4 = request->getParam("webentry4")->value();

  // Store entries as int
  int new_entries1 = str_entries1.toInt();
  int new_entries2 = str_entries2.toInt();
  int new_entries3 = str_entries3.toInt();
  int new_entries4 = str_entries4.toInt();

  // Allocate the JSON document
  JsonDocument doc;

  // Add values in the object, apirul
  doc["apiurl"] = new_apiUrl;

  // Add array of entries
  JsonObject entries = doc["entries"].to<JsonObject>();
  entries["1"] = new_entries1;
  entries["2"] = new_entries2;
  entries["3"] = new_entries3;
  entries["4"] = new_entries4;

  // Generate the prettified JSON and send it to the Serial port.
  Serial.println("New Configuration:");
  serializeJsonPretty(doc, Serial);

  // Save the configuration

  // Delete existing file, otherwise the configuration is appended to the file
  SD.remove(setupfile);

  // Open file for writing
  File file = SD.open(setupfile, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create new file"));
    return;
  }

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();

  // serve HTML
  // Open the file for reading
  File htmlFile = SD.open("html/save.html", FILE_READ);

  // Check if the file opened successfully
  if (!htmlFile) {
    Serial.println("Failed to open setup.html");
    request->send(500, "text/plain", "Error: Could not open file");
    return;
  }

  // Read the entire file content into a String
  String htmlContent = htmlFile.readString();

  // Close the file
  htmlFile.close();

  // Send the response with the HTML content
  request->send(200, "text/html", htmlContent);
}

void saveWifi(AsyncWebServerRequest *request) {
  // Get new data from setup page
  String new_ssid = request->getParam("webssid")->value();
  String new_pass = request->getParam("webpass")->value();

  // Allocate the JSON document
  JsonDocument doc;

  // Add values in the object
  doc["ssid"] = new_ssid;
  doc["password"] = new_pass;

  // Generate the prettified JSON and send it to the Serial port.
  Serial.println("New WiFi Configuration:");
  serializeJsonPretty(doc, Serial);

  // Save the configuration

  // Delete existing file, otherwise the configuration is appended to the file
  SD.remove(wififile);

  // Open file for writing
  File file = SD.open(wififile, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create new file"));
    return;
  }

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();

  // serve HTML
  // Open the file for reading
  File htmlFile = SD.open("html/save.html", FILE_READ);

  // Check if the file opened successfully
  if (!htmlFile) {
    Serial.println("Failed to open save.html");
    request->send(500, "text/plain", "Error: Could not open file");
    return;
  }

  // Read the entire file content into a String
  String htmlContent = htmlFile.readString();

  // Close the file
  htmlFile.close();

  // Send the response with the HTML content
  request->send(200, "text/html", htmlContent);
}

void loadWifi() {
  // Open the setup JSON file
  File dataFile = SD.open("config/wifi.json", FILE_READ);
  if (!dataFile) {
    Serial.println("Failed to open wifi.json");
    return;
  }

  // Read JSON data into a character array
  char json[200];
  dataFile.readBytes(json, sizeof(json));
  dataFile.close();

  // Parse the JSON data
  DynamicJsonDocument doc(768);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("Parsing JSON failed: "));
    Serial.println(error.c_str());
    return;
  }

  // Extract data from JSON
  ssid = doc["ssid"].as<String>();
  password = doc["password"].as<String>();
}

void loadSetup() {
  // Open the setup JSON file
  File dataFile = SD.open("config/setup.json", FILE_READ);
  if (!dataFile) {
    Serial.println("Failed to open setup.json");
    return;
  }

  // Read JSON data into a character array
  char json[768];
  dataFile.readBytes(json, sizeof(json));
  dataFile.close();

  // Parse the JSON data
  DynamicJsonDocument doc(768);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("Parsing JSON failed: "));
    Serial.println(error.c_str());
    return;
  }

  // Extract data from JSON
  apiUrl = doc["apiurl"].as<String>();
  for (int i = 1; i <= 4; i++) {
    entryValues[i - 1] = doc["entries"][String(i)];
  }
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Set all button pins as input with internal pull-up resistors
  for (int pin : buttonPins) {
    pinMode(pin, INPUT_PULLUP);
  }

  Serial.println();

  // Initialize SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed");
    return;
  }

  Serial.println("SD card initialized.");

  // Load wireless configuration from JSON
  loadWifi();

  // Connect to WiFi
  if(password == ""){
    Serial.println("Using Open SSID " + ssid);
    WiFi.begin(ssid.c_str());
  } else {
    Serial.println("Using Secure SSID " + ssid);
    WiFi.begin(ssid.c_str(), password.c_str());
  }

  // Report WiFi status
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }
  Serial.println("WiFi Connected");

  // Print IP Address & MAC
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());

  // Load setup configuration from JSON
  loadSetup();

  // Print API URL
  Serial.println("API URL: " + String(apiUrl));

  // Print Entries
  Serial.println("Entry ID 1: " + String(entryValues[0]));
  Serial.println("Entry ID 2: " + String(entryValues[1]));
  Serial.println("Entry ID 3: " + String(entryValues[2]));
  Serial.println("Entry ID 4: " + String(entryValues[3]));

  // Serve HTML Pages
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Open the file for reading
    File htmlFile = SD.open("html/index.html", FILE_READ);

    // Check if the file opened successfully
    if (!htmlFile) {
      Serial.println("Failed to open setup.html");
      request->send(500, "text/plain", "Error: Could not open file");
      return;
    }

    // Read the entire file content into a String
    String htmlContent = htmlFile.readString();

    // Close the file
    htmlFile.close();

    // Send the response with the HTML content
    request->send(200, "text/html", htmlContent);
  });

  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Require authentication
    if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();

    // Open the file for reading
    File htmlFile = SD.open("html/wifi.html", FILE_READ);

    // Check if the file opened successfully
    if (!htmlFile) {
      Serial.println("Failed to open setup.html");
      request->send(500, "text/plain", "Error: Could not open file");
      return;
    }

    // Read the entire file content into a String
    String htmlContent = htmlFile.readString();

    // Close the file
    htmlFile.close();

    // Send the response with the HTML content
    request->send(200, "text/html", htmlContent);
  });

  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Require authentication
    if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();

    // Open the file for reading
    File htmlFile = SD.open("html/setup.html", FILE_READ);

    // Check if the file opened successfully
    if (!htmlFile) {
      Serial.println("Failed to open setup.html");
      request->send(500, "text/plain", "Error: Could not open file");
      return;
    }

    // Read the entire file content into a String
    String htmlContent = htmlFile.readString();

    // Close the file
    htmlFile.close();

    // Send the response with the HTML content
    request->send(200, "text/html", htmlContent);
  });

  server.on("/saveSetup", HTTP_GET, saveSetup);

  server.on("/saveWifi", HTTP_GET, saveWifi);

  server.on("/webRestart", HTTP_GET, webRestart);

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  AsyncElegantOTA.begin(&server);  // Start ElegantOTA
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  // Ignore SSL certificate validation
  client->setInsecure();

  //create an HTTPClient instance using https
  HTTPClient https;

  //begin https connection & set the post header
  https.begin(*client, apiUrl);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header

  // Create https request variables
  String httpRequestData;
  int httpsResponseCode;
  String response;

  // Monitor buttons for input
  for (int pin : buttonPins) {
    // Check if specific button is pressed (LOW state)
    if (digitalRead(pin) == LOW) {
      // Print message based on the pressed button
      switch (pin) {
        case D1:
          Serial.println("Button 1 pressed!");
          httpRequestData = "entryId=" + String(entryValues[0]);
          break;
        case D2:
          Serial.println("Button 2 pressed!");
          httpRequestData = "entryId=" + String(entryValues[1]);
          break;
        case D3:
          Serial.println("Button 3 pressed!");
          httpRequestData = "entryId=" + String(entryValues[2]);
          break;
        case D4:
          Serial.println("Button 4 pressed!");
          httpRequestData = "entryId=" + String(entryValues[3]);
          break;
      }
      // Send HTTP POST request
      httpsResponseCode = https.POST(httpRequestData);

      if (httpsResponseCode > 0) {
        response = https.getString();  //Get the response to the request
        Serial.println(httpsResponseCode);  //Print return code
        Serial.println(response);           //Print request answer
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpsResponseCode);
      }
      delay(500); // Debounce delay
    }
  }
}