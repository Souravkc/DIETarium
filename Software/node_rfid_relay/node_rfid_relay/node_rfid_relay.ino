#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_task_wdt.h"

/* ================= WDT ================= */
#define WDT_TIMEOUT_SEC 5

/* ================= WIFI ================= */
const char* WIFI_SSID = "HUB_NODE";
const char* WIFI_PASS = "12345678";
const char* HUB_IP    = "192.168.4.1";
const uint16_t HUB_PORT = 5000;

WiFiClient client;

/* ================= RFID ================= */
#define SS_PIN    10
#define RST_PIN   14
#define SCK_PIN   12
#define MOSI_PIN  11
#define MISO_PIN  13

MFRC522 rfid(SS_PIN, RST_PIN);

/* ================= RELAY ================= */
#define RELAY_RED     16
#define RELAY_YELLOW  17
#define RELAY_GREEN   21

/* ================= OLED ================= */
#define OLED_SDA 8
#define OLED_SCL 9
Adafruit_SSD1306 display(128, 64, &Wire, -1);

/* ================= CARD STATE ================= */
bool cardActive = false;
String currentUID = "";
unsigned long lastSeen = 0;
#define CARD_REMOVE_TIMEOUT 300  // ms

/* ================= STATUS ================= */
String lastRFID = "-";
String lastCmd  = "-";
bool hubConnected = false;

/* ================= COLOR ================= */
enum RelayColor { NONE, RED, YELLOW, GREEN };

struct RFIDMap {
  const char* uid;
  RelayColor color;
};

RFIDMap rfidMap[] = {
  {"03B4E5E0", RED}, {"735DC7E0", RED}, {"2340D1E0", RED},
  {"63F6D8E0", RED}, {"C30806E0", RED},

  {"C355D7D9", GREEN}, {"23D2D2E0", GREEN}, {"B337D1E0", GREEN},
  {"131CE4E0", GREEN}, {"6335DEE0", GREEN},

  {"334925DA", YELLOW}, {"E3C2E1E0", YELLOW}, {"7334E5E0", YELLOW},
  {"A3A5E3E0", YELLOW}, {"131FE4E0", YELLOW}
};

const int MAP_SIZE = sizeof(rfidMap) / sizeof(rfidMap[0]);

/* ================= OLED UPDATE ================= */
void updateOLED() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.println("NODE 1 - RFID");
  display.print("WiFi: ");
  display.println(WiFi.status()==WL_CONNECTED ? "OK":"WAIT");

  display.print("Hub: ");
  display.println(hubConnected ? "CONNECTED":"WAIT");

  display.print("RFID: ");
  display.println(lastRFID);

  display.display();
}

/* ================= RELAY ================= */
void allRelaysOff() {
  digitalWrite(RELAY_RED, LOW);
  digitalWrite(RELAY_YELLOW, LOW);
  digitalWrite(RELAY_GREEN, LOW);
}

void activateRelay(RelayColor c) {
 // allRelaysOff();
  if (c == RED)     
  {
    digitalWrite(RELAY_RED, HIGH);
        digitalWrite(RELAY_YELLOW, 0);
  digitalWrite(RELAY_GREEN, 0);
  }
  if (c == YELLOW)  
  {
  digitalWrite(RELAY_YELLOW, HIGH);
  digitalWrite(RELAY_RED, 0);
   digitalWrite(RELAY_GREEN, 0);
 }
  if (c == GREEN)   
  {
      digitalWrite(RELAY_RED, 0);
      digitalWrite(RELAY_YELLOW, 0);
  digitalWrite(RELAY_GREEN, HIGH);
  }
}

/* ================= RFID ================= */
RelayColor getRelayForUID(const String& uid) {
  for (int i = 0; i < MAP_SIZE; i++) {
    if (uid == rfidMap[i].uid) return rfidMap[i].color;
  }
  return NONE;
}

void resetRFID() {
  rfid.PCD_Reset();
  rfid.PCD_Init();
}

/* ================= WIFI CONNECT ================= */
void connectToHub() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(WiFi.status() != WL_CONNECTED) {
    updateOLED();
    delay(500);
  }

  while(!client.connect(HUB_IP, HUB_PORT)) {
    hubConnected = false;
    updateOLED();
    delay(1000);
  }

  hubConnected = true;
  updateOLED();
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(9600);

  /* ---- WATCHDOG (ESP32-S3 CORRECT) ---- */
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT_SEC * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  pinMode(RELAY_RED, OUTPUT);
  pinMode(RELAY_YELLOW, OUTPUT);
  pinMode(RELAY_GREEN, OUTPUT);
  allRelaysOff();

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  updateOLED();
  connectToHub();
}

/* ================= LOOP ================= */
void loop() {
  
  /* ---------- HUB CONNECTION ---------- */
  if(!client.connected()) {
    hubConnected = false;
    connectToHub();
  }

  /* ---------- RFID SCAN ---------- */
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

    lastSeen = millis();

    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uid += "0";
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    String msg = "/rfid," + uid;
    Serial.println(uid);
    client.println(msg);
   // delay(1000);
     
    if (!cardActive || uid != currentUID) {
      cardActive = true;
      currentUID = uid;

      RelayColor r = getRelayForUID(uid);
      activateRelay(r);
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  /* ---------- CARD REMOVED ---------- */
  if (cardActive && (millis() - lastSeen > CARD_REMOVE_TIMEOUT)) {
    cardActive = false;
    currentUID = "";
   // allRelaysOff();

    resetRFID();  // critical for continuous operation
  }

  esp_task_wdt_reset();
  delay(5);
}
