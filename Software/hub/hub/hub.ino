#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include "esp_task_wdt.h"

#define WDT_TIMEOUT 5  // seconds

/* ================= OLED ================= */
#define OLED_SDA 8
#define OLED_SCL 9
Adafruit_SSD1306 display(128,64,&Wire,-1);

/* ================= WIFI ================= */
const char* AP_SSID = "HUB_NODE";
const char* AP_PASS = "12345678";
WiFiServer server(5000);

/* ================= CLIENT TRACKING ================= */
#define MAX_NODES 5
WiFiClient nodes[MAX_NODES];

/* ================= DATA ================= */
String node1Data = "";
String node2Data = "";
String cpuCmd = "";
String lastNode = "-";

/* ================= COUNT ACTIVE NODES ================= */
int countActiveNodes() {
  int count = 0;
  for(int i=0;i<MAX_NODES;i++) {
    if(nodes[i] && nodes[i].connected()) {
      count++;
    } else {
      nodes[i].stop(); // cleanup dead socket
    }
  }
  return count;
}

/* ================= OLED ================= */
void oledUpdate() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.println("HUB NODE");
  display.print("Nodes: ");
  display.println(countActiveNodes());
  display.print("Last: ");
  display.println(lastNode);

  display.print("N1: ");
  display.println(node1Data);

  display.print("N2: ");
  display.println(node2Data);

  display.print("CPU: ");
  display.println(cpuCmd);

  display.display();
}

/* ================= ACCEPT NEW NODES ================= */
void acceptNewNode() {
  WiFiClient newClient = server.available();
  if(!newClient) return;

  for(int i=0;i<MAX_NODES;i++) {
    if(!nodes[i] || !nodes[i].connected()) {
      nodes[i] = newClient;
      return;
    }
  }
  newClient.stop(); // no slot
}

/* ================= RECEIVE FROM NODES ================= */
void receiveFromNodes() {
  for(int i=0;i<MAX_NODES;i++) {
    if(nodes[i] && nodes[i].connected() && nodes[i].available()) {

      String msg = nodes[i].readStringUntil('\n');
      msg.trim();
      if(!msg.length()) return;

      if(msg.startsWith("/rfid")) {
        node1Data = msg;
        lastNode = "NODE1";
        Serial.println(msg);  // send to CPU
      }

      if(msg.startsWith("/pot")) {
        node2Data = msg;
        lastNode = "NODE2";
        Serial.println(msg);  // send to CPU
      }

      if(msg.startsWith("/pbs")) {
        node2Data = msg;
        lastNode = "NODE2";
        Serial.println(msg);  // send to CPU
      }
    }
  }
}

/* ================= RECEIVE FROM CPU ================= */
void receiveFromCPU() {
  if(!Serial.available()) return;

  cpuCmd = Serial.readStringUntil('\n');
  cpuCmd.trim();
}

/* ================= SEND TO NODE1 ONLY ================= */
void sendToNode1() {
  if(!cpuCmd.length()) return;

  for(int i=0;i<MAX_NODES;i++) {
    if(nodes[i] && nodes[i].connected()) { 
      nodes[i].println(cpuCmd);   // send color
      cpuCmd = "";
      return;
    }
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(9600);
    esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // watch both cores
    .trigger_panic = true                            // reset on timeout
  };

  // Initialize watchdog
  esp_task_wdt_init(&wdt_config);

  // Add current task (Arduino loop task)
  esp_task_wdt_add(NULL);


  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);

  WiFi.softAP(AP_SSID, AP_PASS);
  server.begin();
}

/* ================= LOOP ================= */
void loop() {
  acceptNewNode();
  receiveFromNodes();
  receiveFromCPU();
  sendToNode1();
  oledUpdate();
  esp_task_wdt_reset();
  delay(100);
}
