//Thanks to JG and based on the code from https://espgo.be/index-en.html

#include <WiFiManager.h>   // WiFi manager library for easy WiFi connection management
#include <TFT_eSPI.h>      // TFT display library
#include <Preferences.h>   // Preferences library for saving data to flash memory
#include <esp_sntp.h>      // ESP SNTP (Simple Network Time Protocol) library
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Initialize WiFiManager, Preferences, TFT display objects
WiFiManager myWiFi;
Preferences prefs;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite animat = TFT_eSprite(&tft);

const char* apiKey = "cee8677874679b33b7d2488b99073587"; // OpenWeatherMap API key
const unsigned long weatherUpdateInterval = 60000; // Update weather every 1 minute

// Define a structure for city data
struct City {
  const char* name;
  const char* id;
};
// Define a structure for Time Zone
struct tm tInfo; 
// Choose cities and corresponding IDs from OpenWeatherMap
City cities[] = {
  {"Oradea", "671768"},
  {"Vaxjo", "2663536"},
  {"Summerville", "4597919"},
  {"Sidney", "2147714"},
  {"Toronto", "6167865"} // Add more cities if needed
};

// Define time zones for each city
const char* timeZone[] = {
  "EET-2EEST,M3.5.0/3,M10.5.0/4",
  "CET-1CEST,M3.5.0,M10.5.0/3",
  "EST5EDT,M3.2.0,M11.1.0",
  "AEST-10AEDT,M10.1.0,M4.1.0/3",
  "EST5EDT,M3.2.0,M11.1.0"
};

#define BUTTON_PIN 14 // Define the pin for the button
#define startText "Connecting to WiFi" // Start-up text message
#define syncOkTxt "WiFi OK- Time sync" // Message for successful time synchronization
bool sync_OK = false; // Flag indicating whether time synchronization is successful
byte currentCityIndex = 0; // Index of the currently selected city

// Function prototypes
void showStartUpLogo();
void display_Time_and_Weather();
void showConnected();
void show_Message_No_Connection(WiFiManager* myWiFi);
void switchTimeZone();
void buttonFlashPressed();
void SNTP_Sync(struct timeval* tv);

void setup() {
  // Initialize preferences and read the saved city index
  prefs.begin("my-clock", true);
  currentCityIndex = prefs.getInt("counter", 0);
  prefs.end();
  currentCityIndex %= sizeof(timeZone) / sizeof(timeZone[0]); // Ensure index is within bounds

  // Configure button pin as input with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize TFT display
  tft.init();
  tft.setRotation(3);
  sprite.createSprite(320, 170); // Create a sprite for faster rendering

  // Show start-up logo and text
  showStartUpLogo();

  // Set up SNTP callback for time synchronization
  sntp_set_time_sync_notification_cb(SNTP_Sync);

  // Set up WiFi manager to handle WiFi connection
  myWiFi.setAPCallback(show_Message_No_Connection);
  myWiFi.autoConnect("T-Display_S3"); // Connect to WiFi network

  // Show connected message
  showConnected();

  // Configure time zone for the current city
  configTzTime(timeZone[currentCityIndex], "pool.ntp.org");
}

void loop() {
  // Display time and weather information
  display_Time_and_Weather();
  
  // Check if the button is pressed to switch time zone
  if (!digitalRead(BUTTON_PIN)) {
    switchTimeZone();
  }
  if (!digitalRead(0)) buttonFlashPressed();
}


void showStartUpLogo() {
  tft.fillScreen(TFT_BLACK);
  sprite.fillSprite(sprite.color565(100, 100, 100));
  for (int i = 0; i < 12000; i++) {
    byte j = random(100) + 50;
    sprite.drawPixel(random(320), random(170), sprite.color565(j, j, j));
  }
  sprite.setTextColor(tft.color565(48, 48, 48));
  for (byte i = 0; i < 8; i++) {
    sprite.drawRect(131 + (i * 10), 10, 2, 115, sprite.color565(240, 240, 240));
    sprite.fillRoundRect(129 + (i * 10), 18, 6, 99, 2, sprite.color565(240, 240, 240));
    sprite.drawRect(110, 32 + (i * 10), 115, 2, sprite.color565(240, 240, 240));
    sprite.fillRoundRect(118, 30 + (i * 10), 99, 6, 2, sprite.color565(240, 240, 240));
  }
  sprite.fillRoundRect(122, 22, 91, 91, 3, TFT_BLACK);
  sprite.drawRoundRect(122, 22, 91, 91, 3, TFT_DARKGREY);
  sprite.drawCentreString(F("ESP32"), 157, 74, 1);
  sprite.drawCentreString(F("1732S019"), 164, 86, 1);
  sprite.fillCircle(200, 34, 3, sprite.color565(16, 16, 16));
  sprite.setFreeFont(&FreeSans18pt7b);
  sprite.setTextColor(TFT_BLACK);
  sprite.drawCentreString(startText, 161, 132, 1);
  sprite.setTextColor(TFT_WHITE);
  sprite.drawCentreString(startText, 159, 130, 1);
  sprite.setTextColor(sprite.color565(100, 100, 100));
  sprite.drawCentreString(startText, 160, 131, 1);
  sprite.pushSprite(0, 0);
}

void display_Time_and_Weather() {
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?id=" + String(cities[currentCityIndex].id) + "&appid=" + String(apiKey) + "&units=metric";
  Serial.println(url);
  
  http.begin(url);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    String city = doc["name"];
    String country = doc["sys"]["country"];
    float temperature = doc["main"]["temp"];
    int humidity = doc["main"]["humidity"];
    
    getLocalTime(&tInfo);
    if (sync_OK) {
      sprite.fillSprite(TFT_BLACK);
      
      int myColor = (WiFi.status() == WL_CONNECTED) ? TFT_GREEN : TFT_RED;
      sprite.fillCircle(294, 30, 6, myColor);
      for (byte tel = 0; tel < 3; tel++) {
        sprite.drawSmoothArc(294, 32, 30 - tel * 7, 28 - tel * 7, 135, 225, myColor, myColor, true);
      }
      sprite.setFreeFont(&FreeSans12pt7b);
      sprite.setTextColor(TFT_CYAN, TFT_BLACK);
      sprite.setTextSize(1);
      sprite.setCursor(10, 157);
      sprite.printf("%02d", tInfo.tm_hour);
      sprite.fillRect(43, 145, 2, 2, TFT_CYAN);
      sprite.fillRect(43, 153, 2, 2, TFT_CYAN);
      sprite.setCursor(50, 157);
      sprite.printf("%02d", tInfo.tm_min);
      sprite.fillRect(83, 145, 2, 2, TFT_CYAN);
      sprite.fillRect(83, 153, 2, 2, TFT_CYAN);
      sprite.setCursor(90, 157);
      sprite.setTextSize(1);
      sprite.printf("%02d", tInfo.tm_sec);
      
      sprite.setTextColor(TFT_WHITE);
      sprite.setFreeFont(&Orbitron_Light_24);
      sprite.setCursor(10, 35);
      sprite.printf("Temp.: %.1f C\n", temperature);
      sprite.setCursor(10, 65);
      sprite.printf("Hum.: %d%%\n", humidity);
      
      sprite.drawRoundRect(0, 134, 320, 34, 5, TFT_YELLOW);
      sprite.setFreeFont(&FreeSans12pt7b);
      sprite.setCursor(188, 159);
      sprite.setTextColor(TFT_CYAN);
      sprite.printf("%02d-%02d-%04d", tInfo.tm_mday, 1 + tInfo.tm_mon, 1900 + tInfo.tm_year);
      sprite.setTextColor(TFT_ORANGE);
      sprite.drawCentreString(cities[currentCityIndex].name, 160, 108, 1);
      sprite.pushSprite(0, 0);
    }
  } 
  http.end();
}

void showConnected() {
  animat.createSprite(320, 58);
  animat.fillSprite(sprite.color565(100, 100, 100));
  animat.setFreeFont(&FreeSans18pt7b);
  for (int i = 0; i < 4000; i++) {
    byte j = random(100) + 50;
    animat.drawPixel(random(320), random(58), sprite.color565(j, j, j));
  }
  animat.setTextColor(TFT_BLACK);
  animat.drawCentreString(syncOkTxt, 161, 6, 1);
  animat.setTextColor(TFT_WHITE);
  animat.drawCentreString(syncOkTxt, 159, 4, 1);
  animat.setTextColor(sprite.color565(100, 100, 100));
  animat.drawCentreString(syncOkTxt, 160, 5, 1);
  for (int i = 850; i > 650; i--) animat.pushSprite(0, i / 5);
  animat.deleteSprite();
}

void show_Message_No_Connection(WiFiManager* myWiFi) {
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextFont(4);
  tft.setCursor(0, 0, 4);
  tft.print(F(" WiFi: no connection.\n Connect to hotspot 'T-Display_S3'\n and open a browser\n"));
  tft.print(F(" at address 192.168.4.1\n to enter network name \n and password."));
}

void switchTimeZone() {
  animat.createSprite(320, 28);
  animat.setFreeFont(&FreeSans12pt7b);
  animat.fillSprite(TFT_BLACK);
  animat.setTextColor(TFT_ORANGE);
  animat.drawCentreString(cities[currentCityIndex].name, 160, 3, 1);
  for (int tel = 0; tel < 320; tel++) {
    animat.pushSprite(tel, 105);
  }

  currentCityIndex = (currentCityIndex + 1) % (sizeof(timeZone) / sizeof(timeZone[0]));
  configTzTime(timeZone[currentCityIndex], "pool.ntp.org");

  animat.fillSprite(TFT_BLACK);
  animat.drawCentreString(cities[currentCityIndex].name, 160, 3, 1);
  for (int tel = -320; tel < 1; tel++) {
    animat.pushSprite(tel, 105);
  }
  animat.deleteSprite();
}

void buttonFlashPressed() {
  byte savedCityIndex;
  prefs.begin("my-clock");                                 // read from flash memory
  savedCityIndex = prefs.getInt("counter", 0);             // retrieve the last set city index - default to first in the array [0]
  if (savedCityIndex != currentCityIndex) prefs.putInt("counter", currentCityIndex);  // only write the city index to flash memory when it was changed
  prefs.end();                                             // to prevent chip wear from excessive writing
  sprite.deleteSprite();
  sprite.createSprite(320, 50);
  tft.fillScreen(TFT_BLACK);
  sprite.setFreeFont(&FreeSansBold24pt7b);
  sprite.setTextColor(TFT_YELLOW);
  sprite.drawRightString("Deep Sleep", 319, 3, 1);
  for (int tel = 0; tel > -320; tel--) sprite.pushSprite(tel, 60);  // short animation = UI debouncing
  esp_wifi_stop();
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_0, ESP_EXT1_WAKEUP_ALL_LOW);  // flash button set for awakening
  esp_deep_sleep_start();
}

void SNTP_Sync(struct timeval* tv) {
  sync_OK = true;
}
