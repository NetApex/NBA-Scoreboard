#include <WiFi.h>
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <Adafruit_GFX.h>     // Core graphics library
#include "Adafruit_ILI9341.h" // ILI9341 library
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <HTTPClient.h>       // Include HTTPClient library
#include "secrets.h"          // Include secrets.h for API key
#include <FirebaseClient.h>   // Firebase Client library v2.0 (mobizt) - CORRECT INCLUDE

// --- Firebase Project Settings (Replace with your own in secrets.h!) ---
#define FIREBASE_HOST FIREBASE_PROJECT_ID ".firebaseio.com" // Firebase project HOST from secrets.h
#define FIREBASE_AUTH_TOKEN FIREBASE_API_KEY              // Firebase project API key from secrets.h
#define FIREBASE_USER_EMAIL FIREBASSE_USER_EMAIL           // Firebase auth email from secrets.h (if using email/password auth)
#define FIREBASE_USER_PASSWORD FIREBASSE_USER_PASSWORD     // Firebase auth password from secrets.h (if using email/password auth)

// --- Firebase Data Paths ---
#define FIREBASE_GAMES_PATH "nba_games" // Path to your nba_games data in Firebase Realtime DB

// --- Firebase Objects ---
FirebaseDataStream firebaseData; // Corrected to FirebaseDataStream
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

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

// --- Function to Fetch NBA Games from Firebase Realtime DB (mobizt FirebaseClient v2.0 lib) ---
void fetchNBAGamesFromFirebase() {
  Serial.println("Fetching NBA games from Firebase Realtime DB...");

  FirebaseDataStream firebaseData_local; // **Declare FirebaseDataStream object LOCALLY - Corrected to FirebaseDataStream**
  FirebaseJson json; // Declare FirebaseJson object locally for getJSON
  firebaseData_local.clear(); // Clear firebaseData_local object before getJSON - CORRECTED to local object

  if (Firebase.get(firebaseData_local, FIREBASE_GAMES_PATH)) { // **Use Firebase.get() for v2.0 lib - different syntax**
    if (firebaseData_local.dataType() == FirebaseJson::JSON_OBJECT) {
      Serial.println("Successfully fetched NBA games from Firebase Realtime DB!");

      // FirebaseJson *gamesJson = firebaseData_local.jsonObjectPtr(); // No longer needed, use local json object
      FirebaseJsonData jsonData;

      // --- Iterate through the JSON object (assuming game IDs are keys) ---
      json.setJsonString(firebaseData_local.payload()); // **Important: Parse payload for v2.0 lib**
      for (FirebaseJson::Iterator iterator = json.iterator(); iterator.hasNext(); ) { // Iterate over local json object
        iterator.next(&jsonData);
        String gameId = jsonData.key(); // Game ID is the key in the JSON object
        Serial.print("Game ID: ");
        Serial.println(gameId);

        // --- Access game data (example - adjust based on your data structure) ---
        if (jsonData.dataType() == FirebaseJson::JSON_OBJECT) {
          FirebaseJson *gameObject = jsonData.jsonObjectPtr();
          FirebaseJsonData gameDataValue;

          // Example: Get 'id' field within each game object
          if (gameObject->get(gameDataValue, "id")) {
             if (gameDataValue.dataType() == FirebaseJson::JSON_INTEGER) {
                Serial.print("  Game ID (from data): ");
                Serial.println(gameDataValue.intValue());
             }
          }
          // You can access other fields similarly using gameObject->get()
        }
        Serial.println("---");
      }
      json.clear(); // Clear the local json object after use

    } else {
      Serial.println("Error: Incorrect data type received from Firebase (not a JSON Object).");
      Serial.print("Data type: ");
      Serial.println(firebaseData_local.dataTypeStr());
    }
  } else {
    Serial.print("Error fetching NBA games from Firebase Realtime DB: ");
    Serial.println(firebaseData_local.errorReason());
    Serial.print("HTTP Code: "); // Added HTTP code output for debugging v2.0 lib
    Serial.println(firebaseData_local.httpCode()); // Added HTTP code output for debugging v2.0 lib
  }
}


void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(TFT_BL, OUTPUT); // Backlight pin
  digitalWrite(TFT_BL, HIGH); // Turn backlight ON

  tft.begin();              // Initialize ILI9341 display
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

  // --- Initialize Firebase ---
  firebaseConfig.host = FIREBASE_HOST;        // Use FIREBASE_HOST from secrets.h
  firebaseConfig.apiKey = FIREBASE_AUTH_TOKEN; // **apiKey instead of token_status_payload_info for v2.0 lib**
  firebaseAuth.user.email = FIREBASE_USER_EMAIL; // Use FIREBASE_USER_EMAIL from secrets.h
  firebaseAuth.user.password = FIREBASE_USER_PASSWORD; // Use FIREBASE_USER_PASSWORD from secrets.h
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase initialized (mobizt FirebaseClient v2.0)"); // Updated Serial output
  displayMessage("Firebase\nInitialized!");

  // --- Web Server Routes ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<h1>NBA Scoreboard Web Interface</h1><p>Fetching NBA Data from Firebase...</p><div id='nba-data'></div>");
  });

  // --- Start Web Server ---
  server.begin();
  Serial.println("Web server started");
  displayMessage("Web server\nStarted! " + WiFi.localIP().toString()); // Display web server started message

  // --- Fetch NBA Data from Firebase ---
  fetchNBAGamesFromFirebase(); // Call the function to fetch NBA games from Firebase
}

void loop() {
  // Your main code will go here (NBA data, web server, etc.)
  delay(10000);
  Serial.println("Still connected to WiFi and web server running...");
}