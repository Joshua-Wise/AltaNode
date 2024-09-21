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
#include <EEPROM.h>
#include <AES.h>

// Constants and global variables
#define KEY_SIZE 16
const size_t JSON_BUFFER_SIZE = 768;

const char* http_username = "admin";
const char* http_password = "altanode";
const int chipSelect = 15;
const int buttonPins[] = {D1, D2, D3, D4};
const char *setupfile = "/config/setup.json";
const char *wififile = "/config/wifi.json";

String ssid, password, apiUrl;
int entryValues[4];

AsyncWebServer server(80);
AES aes;

String urlEncode(const String& input) {
  const char *hex = "0123456789ABCDEF";
  String output = "";
  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      output += c;
    } else {
      output += '%';
      output += hex[c >> 4];
      output += hex[c & 0xF];
    }
  }
  return output;
}

// Function declarations
void getEncryptionKey(uint8_t* key);
void encryptData(char* input, char* output, size_t inputSize);
void decryptData(const char* input, char* output, size_t inputSize);
void writeEncryptedConfig(File& file, const char* config, size_t configSize);
void readEncryptedConfig(File& file, char* config, size_t configSize);
bool isJsonEncrypted(const char* jsonString, size_t length);
void loadWifi();
void saveWifiToFile(const String& new_ssid, const String& new_pass);
void loadSetup();
void saveSetupToFile(const String& new_apiUrl, const int new_entryValues[4]);
void setupWebServer();
void webRestart(AsyncWebServerRequest *request);
void handleSaveWifi(AsyncWebServerRequest *request);
void handleSaveSetup(AsyncWebServerRequest *request);
void handleSetup(AsyncWebServerRequest *request);
void handleWifi(AsyncWebServerRequest *request);

// Encryption functions
void getEncryptionKey(uint8_t* key) {
  for (int i = 0; i < KEY_SIZE; i++) {
    key[i] = EEPROM.read(i);
  }
}

void encryptData(char* input, char* output, size_t inputSize) {
  uint8_t key[KEY_SIZE];
  getEncryptionKey(key);
  
  aes.set_key(key, sizeof(key));
  
  size_t paddedSize = (inputSize + 15) & ~15;
  for (size_t i = 0; i < paddedSize; i += 16) {
    aes.encrypt((uint8_t*)input + i, (uint8_t*)output + i);
  }
}

void decryptData(const char* input, char* output, size_t inputSize) {
  uint8_t key[KEY_SIZE];
  getEncryptionKey(key);
  
  aes.set_key(key, sizeof(key));
  
  size_t paddedSize = (inputSize + 15) & ~15;
  for (size_t i = 0; i < paddedSize; i += 16) {
    aes.decrypt((uint8_t*)input + i, (uint8_t*)output + i);
  }
  
  while (paddedSize > 0 && output[paddedSize-1] == 0) {
    paddedSize--;
  }
  output[paddedSize] = '\0';
}

void writeEncryptedConfig(File& file, const char* config, size_t configSize) {
  size_t paddedSize = (configSize + 15) & ~15;
  char* paddedConfig = new char[paddedSize];
  memset(paddedConfig, 0, paddedSize);
  memcpy(paddedConfig, config, configSize);
  
  char* encryptedConfig = new char[paddedSize];
  encryptData(paddedConfig, encryptedConfig, paddedSize);
  
  size_t bytesWritten = file.write((uint8_t*)encryptedConfig, paddedSize);
  
  Serial.printf("Original size: %d, Padded size: %d, Bytes written: %d\n", configSize, paddedSize, bytesWritten);
  
  delete[] paddedConfig;
  delete[] encryptedConfig;
}

void readEncryptedConfig(File& file, char* config, size_t configSize) {
  size_t paddedSize = (configSize + 15) & ~15;
  char* encryptedConfig = new char[paddedSize];
  file.read((uint8_t*)encryptedConfig, paddedSize);
  
  decryptData(encryptedConfig, config, paddedSize);
  
  delete[] encryptedConfig;
}

bool isJsonEncrypted(const char* jsonString, size_t length) {
  if (length > 0 && jsonString[0] == '{') {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, jsonString, length);
    return error != DeserializationError::Ok;
  }
  return true;
}

// WiFi functions
void loadWifi() {
  File dataFile = SD.open(wififile, FILE_READ);
  if (!dataFile) {
    Serial.println("Failed to open wifi.json");
    return;
  }

  size_t fileSize = dataFile.size();
  char* jsonBuffer = new char[fileSize + 1];
  
  size_t bytesRead = dataFile.readBytes(jsonBuffer, fileSize);
  jsonBuffer[bytesRead] = '\0';
  dataFile.close();

  bool encrypted = isJsonEncrypted(jsonBuffer, bytesRead);
  Serial.printf("WiFi data is %s\n", encrypted ? "encrypted" : "unencrypted");

  DynamicJsonDocument doc(JSON_BUFFER_SIZE);
  DeserializationError error;

  if (encrypted) {
    char* decryptedJson = new char[fileSize];
    decryptData(jsonBuffer, decryptedJson, fileSize);
    error = deserializeJson(doc, decryptedJson);
    delete[] decryptedJson;
  } else {
    error = deserializeJson(doc, jsonBuffer);
  }

  delete[] jsonBuffer;

  if (error) {
    Serial.print(F("Parsing WiFi JSON failed: "));
    Serial.println(error.c_str());
    return;
  }

  ssid = doc["ssid"].as<String>();
  password = doc["password"].as<String>();

  if (!encrypted) {
    Serial.println("Encrypting and saving WiFi configuration...");
    saveWifiToFile(ssid, password);
  }
}

void saveWifiToFile(const String& new_ssid, const String& new_pass) {
  DynamicJsonDocument doc(JSON_BUFFER_SIZE);
  doc["ssid"] = new_ssid;
  doc["password"] = new_pass;

  String jsonString;
  serializeJson(doc, jsonString);

  SD.remove(wififile);
  File file = SD.open(wififile, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create WiFi config file"));
    return;
  }

  writeEncryptedConfig(file, jsonString.c_str(), jsonString.length());
  file.close();

  Serial.println("Encrypted WiFi configuration saved to SD card");
}

// Setup functions
void loadSetup() {
  File dataFile = SD.open(setupfile, FILE_READ);
  if (!dataFile) {
    Serial.println("Failed to open setup.json");
    return;
  }

  size_t fileSize = dataFile.size();
  char* jsonBuffer = new char[fileSize + 1];
  
  dataFile.readBytes(jsonBuffer, fileSize);
  jsonBuffer[fileSize] = '\0';
  dataFile.close();

  bool encrypted = isJsonEncrypted(jsonBuffer, fileSize);
  Serial.printf("Setup data is %s\n", encrypted ? "encrypted" : "unencrypted");

  DynamicJsonDocument doc(JSON_BUFFER_SIZE);
  DeserializationError error;

  if (encrypted) {
    char* decryptedJson = new char[fileSize];
    decryptData(jsonBuffer, decryptedJson, fileSize);
    error = deserializeJson(doc, decryptedJson);
    delete[] decryptedJson;
  } else {
    error = deserializeJson(doc, jsonBuffer);
  }

  delete[] jsonBuffer;

  if (error) {
    Serial.print(F("Parsing setup JSON failed: "));
    Serial.println(error.c_str());
    return;
  }

  apiUrl = doc["apiurl"].as<String>();
  for (int i = 1; i <= 4; i++) {
    entryValues[i - 1] = doc["entries"][String(i)];
  }

  Serial.print("API URL: ");
  Serial.println(apiUrl);
  for (int i = 0; i < 4; i++) {
    Serial.printf("Entry ID %d: %d\n", i + 1, entryValues[i]);
  }

  if (!encrypted) {
    Serial.println("Encrypting and saving setup configuration...");
    saveSetupToFile(apiUrl, entryValues);
  }
}

void saveSetupToFile(const String& new_apiUrl, const int new_entryValues[4]) {
  DynamicJsonDocument doc(JSON_BUFFER_SIZE);
  doc["apiurl"] = new_apiUrl;
  JsonObject entries = doc["entries"].to<JsonObject>();
  for (int i = 0; i < 4; i++) {
    entries[String(i + 1)] = new_entryValues[i];
  }

  String jsonString;
  serializeJson(doc, jsonString);

  SD.remove(setupfile);
  File file = SD.open(setupfile, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create setup file"));
    return;
  }

  writeEncryptedConfig(file, jsonString.c_str(), jsonString.length());
  file.close();

  Serial.println("Encrypted setup configuration saved to SD card");
}

// Web server functions
void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    File htmlFile = SD.open("html/index.html", FILE_READ);
    if (!htmlFile) {
      request->send(500, "text/plain", "Error: Could not open file");
      return;
    }
    String htmlContent = htmlFile.readString();
    htmlFile.close();
    request->send(200, "text/html", htmlContent);
  });

  server.on("/wifi", HTTP_GET, handleWifi);
  server.on("/setup", HTTP_GET, handleSetup);
  server.on("/saveSetup", HTTP_GET, handleSaveSetup);
  server.on("/saveWifi", HTTP_GET, handleSaveWifi);
  server.on("/webRestart", HTTP_GET, webRestart);
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  AsyncElegantOTA.begin(&server);
  server.begin();
  Serial.println("Web server started");
}

void webRestart(AsyncWebServerRequest *request) {
  Serial.println("Restarting...");
  request->send(200, "text/plain", "Restarting...");
  delay(1000);
  ESP.restart();
}

void handleSaveWifi(AsyncWebServerRequest *request) {
  String new_ssid = request->getParam("webssid")->value();
  String new_pass = request->getParam("webpass")->value();

  saveWifiToFile(new_ssid, new_pass);

  File htmlFile = SD.open("html/save.html", FILE_READ);
  if (!htmlFile) {
    request->send(500, "text/plain", "Error: Could not open file");
    return;
  }
  String htmlContent = htmlFile.readString();
  htmlFile.close();
  request->send(200, "text/html", htmlContent);
}

void handleSaveSetup(AsyncWebServerRequest *request) {
  String new_apiUrl = request->getParam("webapiurl")->value();
  int new_entryValues[4];
  for (int i = 0; i < 4; i++) {
    new_entryValues[i] = request->getParam("webentry" + String(i+1))->value().toInt();
  }

  saveSetupToFile(new_apiUrl, new_entryValues);

  Serial.println("Saving new setup configuration:");
  Serial.println("API URL: " + new_apiUrl);
  for (int i = 0; i < 4; i++) {
    Serial.println("Entry " + String(i+1) + ": " + String(new_entryValues[i]));
  }

  File htmlFile = SD.open("html/save.html", FILE_READ);
  if (!htmlFile) {
    request->send(500, "text/plain", "Error: Could not open file");
    return;
  }
  String htmlContent = htmlFile.readString();
  htmlFile.close();
  request->send(200, "text/html", htmlContent);
}

void handleSetup(AsyncWebServerRequest *request) {
  if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  
  File htmlFile = SD.open("html/setup.html", FILE_READ);
  if (!htmlFile) {
    request->send(500, "text/plain", "Error: Could not open file");
    return;
  }
  
  String htmlContent = htmlFile.readString();
  htmlFile.close();
  
  // Debug output
  Serial.println("API URL before replacement: " + apiUrl);
  
  // Replace placeholders with actual data, encoding the API URL
  htmlContent.replace("%%API_URL%%", urlEncode(apiUrl));
  for (int i = 0; i < 4; i++) {
    htmlContent.replace("%%ENTRY" + String(i+1) + "%%", String(entryValues[i]));
    Serial.println("Entry " + String(i+1) + " value: " + String(entryValues[i]));
  }
  
  // Check if replacement occurred
  Serial.println("HTML content after replacement (first 200 chars): " + htmlContent.substring(0, 200));
  
  request->send(200, "text/html", htmlContent);
}

void handleWifi(AsyncWebServerRequest *request) {
  if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  
  File htmlFile = SD.open("html/wifi.html", FILE_READ);
  if (!htmlFile) {
    request->send(500, "text/plain", "Error: Could not open file");
    return;
  }
  
  String htmlContent = htmlFile.readString();
  htmlFile.close();
  
  // Replace placeholders with actual data
  htmlContent.replace("%%SSID%%", ssid);
  htmlContent.replace("%%PASSWORD%%", password);
  
  request->send(200, "text/html", htmlContent);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(KEY_SIZE);

  for (int pin : buttonPins) {
    pinMode(pin, INPUT_PULLUP);
  }

  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed");
    return;
  }
  Serial.println("SD card initialized.");

  loadWifi();

  if(password.isEmpty()) {
    Serial.println("Using Open SSID " + ssid);
    WiFi.begin(ssid.c_str());
  } else {
    Serial.println("Using Secure SSID " + ssid);
    WiFi.begin(ssid.c_str(), password.c_str());
  }

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }
  Serial.println("WiFi Connected");
  Serial.printf("WiFi IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("WiFi MAC: %s\n", WiFi.macAddress().c_str());

  loadSetup();  // This function should print the configuration

  setupWebServer();
}

void loop() {
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  HTTPClient https;
  https.begin(*client, apiUrl);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String httpRequestData;
  int httpsResponseCode;
  String response;

  for (int i = 0; i < 4; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      Serial.printf("Button %d pressed!\n", i + 1);
      httpRequestData = "entryId=" + String(entryValues[i]);
      
      httpsResponseCode = https.POST(httpRequestData);

      if (httpsResponseCode > 0) {
        response = https.getString();
        Serial.println(httpsResponseCode);
        Serial.println(response);
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpsResponseCode);
      }
      delay(500); // Debounce delay
    }
  }
}