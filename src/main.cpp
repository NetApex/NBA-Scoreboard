#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ESPAsyncWebServer.h> //https://github.com/me-no-dev/ESPAsyncWebServer
#include <Adafruit_GFX.h>     // Core graphics library
#include "Adafruit_ILI9341.h" // ILI9341 library
#include <SPI.h>
#include <XPT2046_Touchscreen.h>


// --- Display Configuration ---
#define TFT_BL 21
#define TFT_CS 15
#define TFT_DC 2
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_RST -1

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass hspi(HSPI);
SPIClass vspi = SPIClass(VSPI);

XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&hspi, TFT_DC, TFT_CS, TFT_RST);

// --- Web Server ---
AsyncWebServer server(80); // Create AsyncWebServer object on port 80

// --- Function to display text on the screen (ILI9341 version) ---
void displayMessage(const String& message) {
  tft.fillScreen(ILI9341_BLACK); // Clear the screen with ILI9341_BLACK
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE); // Use ILI9341_WHITE
  tft.setTextSize(1);
  tft.println(message);
  Serial.println(message);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(TFT_BL, OUTPUT); // Backlight pin
  digitalWrite(TFT_BL, HIGH); // Turn backlight ON

  tft.begin();             // Initialize ILI9341 display
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK); // Clear screen with ILI9341_BLACK

  vspi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); // Init VSPI for touch
  ts.begin(vspi);
  ts.setRotation(1);

  displayMessage("Starting WiFi\nManager...\n"); // Display startup message
  displayMessage("Connect to NBA_Scoreboard_AP\nand enter password: netapex123"); // Display WiFi AP message

  WiFiManager wm;
  // wm.resetSettings(); // For testing - clears saved WiFi credentials

  if (!wm.autoConnect("NBA_Scoreboard_AP", "netapex123")) {
    displayMessage("WiFi Connect\nFailed!"); // Display WiFi fail message
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  displayMessage("WiFi Connected!\nIP: " + WiFi.localIP().toString()); // Display WiFi success
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

   // --- Web Server Routes ---
   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<h1>NBA Scoreboard Web Interface</h1><p>Hello from your ESP32 CYD!</p>");
  });

  // --- Start Web Server ---
  server.begin();
  Serial.println("Web server started");
  displayMessage("Web server\nStarted!"  + WiFi.localIP().toString()); // Display web server started message
}

void loop() {
  // Your main code will go here (NBA data, web server, etc.)
  delay(10000);
  Serial.println("Still connected to WiFi and web server running...");
}