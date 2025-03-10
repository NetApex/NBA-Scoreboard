#include <WiFi.h>
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <Adafruit_GFX.h>     // Core graphics library
#include "Adafruit_ILI9341.h" // ILI9341 library
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <HTTPClient.h>       // Include HTTPClient library
#include "secrets.h"          // Include secrets.h for AP
#include <ArduinoJson.h>     // Required for JSON parsing


// --- Firebase Data Paths ---
#define FIREBASE_GAMES_PATH "nba_games" // Path to your nba_games data in Firebase Realtime DB

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

bool wifiConnected = false;
unsigned long lastDisplayTime = 0; // Variable to store last display time
unsigned long currentTime = 0;
const long displayInterval = 300000; // 5 minutes

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

String getFirebaseData() { // **Refactored for HTTP GET to Cloud Function**
    if (!wifiConnected) {
        Serial.println("WiFi not connected, cannot get NBA scores.");
        return "WiFi not connected";
    }

    HTTPClient http;
    String serverURL = CLOUD_FUNCTION_URL_GET_SCORES; // Use defined Cloud Function URL
    http.begin(serverURL.c_str());

    String jsonResponse;

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        if (httpResponseCode == HTTP_CODE_OK) {
            jsonResponse = http.getString();
            Serial.print("Response body: \n");
            Serial.println(jsonResponse);
        } else {
            Serial.printf("GET request failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
            jsonResponse = "Error: HTTP GET failed";
        }
    } else {
        Serial.printf("GET request failed, connection error: %s\n", http.errorToString(httpResponseCode).c_str());
        jsonResponse = "Error: HTTP connection failed";
    }

    http.end();
    return jsonResponse;
}

String processNBAScores(String jsonInput) {
    if (jsonInput.startsWith("Error:")) {
        return jsonInput;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, jsonInput);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return "Error: JSON Parse Failed";
    }

    String output = "";
    if (doc.containsKey("games")) {
        JsonArray games = doc["games"].as<JsonArray>();

        if (games.size() == 0) {
            return "No games scheduled.";
        }

        for (JsonObject game : games) {
            String homeTeam = game["homeTeam"] | "N/A";
            String awayTeam = game["awayTeam"] | "N/A";
            int homeScore = game["homeScore"] | 0;
            int awayScore = game["awayScore"] | 0;
            String gameStatus = game["gameStatus"] | "N/A";

            output += awayTeam + " vs " + homeTeam + "\n";
            output += "Score: " + String(awayScore) + "-" + String(homeScore) + "\n";
            output += "Status: " + gameStatus + "\n\n";
        }
    } else {
        return "Error: Invalid JSON format - 'games' key missing";
    }
    return output;
}

void displayScores(String scores) {
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 0);
    tft.setTextSize(2);

    if (scores.startsWith("Error:") || scores.startsWith("WiFi not connected") || scores.startsWith("No games")) {
        tft.print(scores.substring(0, 20)); // Display first part of error
    } else {
        char buffer[256];
        char *line;
        int lineCount = 0;
        int yPos = 0; // Start from top

        line = strtok(const_cast<char*>(scores.c_str()), "\n");
        while (line != nullptr && lineCount < 6) { // Limit to 6 lines for TFT
            strncpy(buffer, line, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            tft.println(buffer); // Use println for TFT display
            line = strtok(nullptr, "\n");
            lineCount++;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(ILI9341_BLACK);

    vspi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(vspi);
    ts.setRotation(1);

    displayMessage("Starting WiFi\nManager...\n");
    displayMessage("Connect to NBA_Scoreboard_AP\nand enter password: netapex123");

    WiFiManager wm;
    // wm.resetSettings(); // For testing - clears saved WiFi credentials

    if (!wm.autoConnect("NBA_Scoreboard_AP", "netapex123")) {
        displayMessage("WiFi Connect\nFailed!");
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
        delay(5000);
    }

    displayMessage("WiFi Connected!\nIP: " + WiFi.localIP().toString());
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // --- Web Server Routes ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<h1>NBA Scoreboard Web Interface</h1><p>Fetching NBA Data from Firebase...</p><div id='nba-data'></div>");
    });

    // --- Start Web Server ---
    server.begin();
    Serial.println("Web server started");
    displayMessage("Web server\nStarted! " + WiFi.localIP().toString()); // Display web server started message
}

void loop() {  // Your main code will go here (NBA data, web server, etc.)
    if (wifiConnected) {
        if (millis() - lastDisplayTime >= displayInterval) {
            lastDisplayTime = millis();
            String nbaScoresJson = getFirebaseData(); // Get data from Cloud Function
            String scoresDisplay = processNBAScores(nbaScoresJson);
            displayScores(scoresDisplay);
        }
    }
    delay(1000); // Check every second
}