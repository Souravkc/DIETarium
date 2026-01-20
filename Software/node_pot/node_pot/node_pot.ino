#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_task_wdt.h"

#define WDT_TIMEOUT 5  // seconds

/* ================= NODE INFO ================= */
#define NODE_ID "NODE-POT"

/* ================= WIFI ================= */
const char* WIFI_SSID = "HUB_NODE";
const char* WIFI_PASS = "12345678";
const char* HUB_IP    = "192.168.4.1";
const uint16_t HUB_PORT = 5000;

WiFiClient client;

/* ================= POTENTIOMETERS ================= */
#define POT1_PIN 1
#define POT2_PIN 2
#define POT3_PIN 3

// digital pin 2 has a pB attached to it. Give it a name:
int pB1 = 11;
int pB2= 13;
int pB3= 12;
 
 boolean bS1=1,bS2=1,bS3=1;
  boolean  prevB1=1, prevB2=1, prevB3=1;
#define SAMPLE_COUNT     5
#define SEND_INTERVAL    20   // ms
int CHANGE_THRESHOLD= 25;


int lastPot1 = -1, lastPot2 = -1, lastPot3 = -1;
uint32_t lastSendTime = 0;

/* ================= OLED ================= */
#define OLED_SDA 8
#define OLED_SCL 9
Adafruit_SSD1306 display(128, 64, &Wire, -1);
boolean changed=false;

/* ================= STATUS ================= */
bool hubConnected = false;
String lastSent = "-";
String buttonSent = "-";

/* ================= OLED UPDATE ================= */
void updateOLED(int p1, int p2, int p3,int b1, int b2, int b3) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.println(NODE_ID);

  display.print("WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "OK" : "WAIT");

  display.print("Hub: ");
  display.println(hubConnected ? "CONNECTED" : "WAIT");

  display.print("P1: "); display.println(p1);
  display.print("P2: "); display.println(p2);
  display.print("P3: "); display.println(p3);

  display.print("Last: ");
  display.println(lastSent);

     display.print("B1: ");
  display.print (b1);
  display.print(" B2: ");
  display.print(b2);
  display.print(" B3: ");
  display.println(b3);

  display.display();
}

/* ================= ADC AVERAGE ================= */
int readPotAverage(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sum += analogRead(pin);
  }
  return sum / SAMPLE_COUNT;   // 0–4095
}

/* ================= CONNECT TO HUB ================= */
void connectToHub() {

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    updateOLED(lastPot1, lastPot2, lastPot3,bS1,bS2,bS3);
    delay(300);
  }

  while (!client.connect(HUB_IP, HUB_PORT)) {
    hubConnected = false;
    updateOLED(lastPot1, lastPot2, lastPot3,bS1,bS2,bS3);
    delay(500);
  }

  hubConnected = true;
   updateOLED(lastPot1, lastPot2, lastPot3,bS1,bS2,bS3);
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(9600);
  pinMode(pB1, INPUT_PULLUP);
  pinMode(pB2,INPUT_PULLUP);
  pinMode(pB3, INPUT_PULLUP);
  // Configure watchdog
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // watch both cores
    .trigger_panic = true                            // reset on timeout
  };

  // Initialize watchdog
  esp_task_wdt_init(&wdt_config);

  // Add current task (Arduino loop task)
  esp_task_wdt_add(NULL);


  analogReadResolution(12);       // 0–4095
  analogSetAttenuation(ADC_11db);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  connectToHub();
}

/* ================= LOOP ================= */
void loop() {

  if (!client.connected()) {
    hubConnected = false;
    connectToHub();
  }
   esp_task_wdt_reset();
          int bS1 = digitalRead(pB1);
    int bS2 = digitalRead(pB2);
      int bS3 = digitalRead(pB3);

    int raw1 = readPotAverage(POT1_PIN);
    int raw2 = readPotAverage(POT2_PIN);
    int raw3 = readPotAverage(POT3_PIN);

    int pot1 = map(raw1, 0, 4095, 0, 1023);
    int pot2 = map(raw2, 0, 4095, 0, 1023);
    int pot3 = map(raw3, 0, 4095, 0, 1023);

     if(
      abs(pot1 - lastPot1) > CHANGE_THRESHOLD||
      abs(pot2 - lastPot2) >CHANGE_THRESHOLD ||
      abs(pot3 - lastPot3) >CHANGE_THRESHOLD)
      {
        changed=true;
      }
      else
      {
        changed=false;
      }

    if (changed) {
      lastPot1 = pot1;
      lastPot2 = pot2;
      lastPot3 = pot3;

      lastSent = "/pot, " + String(pot1) + ", " + String(pot2) + ", " + String(pot3);
      client.println(lastSent);
    }

    
    Serial.println(changed);
          if (bS1 != prevB1)
           {        // state changed (0↔1)
        if (client.connected()) {
          String msg = "/pbs/3 " + String(bS1);
          client.println(msg);
        }
        prevB1 = bS1;             // update previous state
      }

       if (bS2 != prevB2) {        // state changed (0↔1)
  if (client.connected())
   {
    String msg = "/pbs/2 " + String(bS2);
    client.println(msg);
  }
  prevB2 = bS2;             // update previous state
}

    if (bS3 != prevB3) {        // state changed (0↔1)
  if (client.connected()) {
    String msg = "/pbs/1 " + String(bS3);
    client.println(msg);
  }
  prevB3 = bS3;             // update previous state
}
    Serial.print(bS1);
    Serial.print(bS2);
      Serial.print(bS3);
        Serial.println();
        updateOLED(pot1, pot2, pot3,bS1,bS2,bS3);
    delay(50);

}
