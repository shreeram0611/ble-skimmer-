/*
  BLUETOOTH SKIMMER / ROGUE DEVICE SCANNER - ESP32
  ---------------------------------------------------
  Passively scans BLE advertisements and tracks how long each device
  has been continuously present nearby and how strong its signal is.
  A device that stays present, close, and unmoving for an extended
  period (default: 10 minutes) is flagged as a possible hidden
  skimmer, as opposed to ordinary phones/earbuds/watches that move
  in and out of range within seconds or minutes.

  This only listens to public BLE advertisement broadcasts - it does
  not connect to or extract data from anyone's device.

  Board: ESP32-S3 DevKit (also works on classic ESP32, adjust pins as needed)
  Arduino IDE setup:
    Tools > Board > Boards Manager > search "esp32" > install
      (by Espressif Systems)
    Tools > Board > select your ESP32-S3 DevKit variant

  Wiring (ESP32-S3): buzzer -> GPIO4, GND. OLED SDA -> GPIO8, OLED
  SCL -> GPIO9, VCC -> 3.3V, GND -> GND. The alert LED uses the
  board's onboard white LED (LED_BUILTIN) - no external LED wiring
  needed.

  FEATURES IN THIS VERSION:
  1) Live web dashboard - the S3 creates its own WiFi hotspot; any
     phone/laptop connects to it and browses to 192.168.4.1 to see
     tracked devices, how long they've been present, and risk scores
     update in real time.
  2) A 0-100 risk score per device, based on REAL ELAPSED TIME present
     (not just scan-cycle count) plus signal strength. A device only
     reaches full persistence score after being continuously present
     for PERSISTENCE_MINUTES (default 10 minutes).
  3) Automatic cleanup - a device that hasn't been seen for
     ABSENCE_TIMEOUT_MS (default 1 minute) is removed from tracking,
     so people who briefly lingered and left don't stay flagged.

  Note: running WiFi and BLE scanning at the same time shares one
  radio internally, so scanning may be very slightly slower than the
  BLE-only version - this is normal and does not affect detection.
*/

#include <WiFi.h>
#include <WebServer.h>

#include "BLEDevice.h"
#include "BLEScan.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BUZZER_PIN 4
#define LED_PIN LED_BUILTIN  // uses the onboard white LED confirmed by your Blink test

// WiFi hotspot the S3 creates for the dashboard - connect your phone/
// laptop to this network, then browse to 192.168.4.1
const char* AP_SSID = "BLE-Skimmer-Scanner";
const char* AP_PASSWORD = "scanner123"; // must be 8+ characters
WebServer server(80);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String alertStatus = "Monitoring...";
int lastDeviceCount = 0;

#define MAX_TRACKED 20
#define SCAN_TIME_SEC 5                    // seconds per scan cycle

// How long a device must be CONTINUOUSLY present before it reaches
// full persistence score. Real skimmers never leave; ordinary people
// browsing a phone nearby almost never stay this long.
#define PERSISTENCE_MINUTES 10UL
#define PERSISTENCE_MS (PERSISTENCE_MINUTES * 60000UL)

// If a device isn't seen for this long, treat it as having left and
// stop tracking it (frees up the slot, and a later reappearance
// starts a fresh timer rather than keeping old history).
#define ABSENCE_TIMEOUT_MS 60000UL         // 1 minute

#define RSSI_NEAR -40   // very close / strong signal
#define RSSI_FAR  -90   // weak / far away signal

// Add your own devices' MAC addresses here so they don't trigger alerts.
// Find your phone's BLE MAC in its Bluetooth settings, or just watch
// Serial Monitor on first run and copy addresses you recognize.
String trustedMacs[] = {
  "AA:BB:CC:DD:EE:FF"  // <-- replace with a real trusted MAC
};
const int trustedCount = 1;

struct TrackedDevice {
  String mac;
  unsigned long firstSeenTime;   // millis() when first spotted
  unsigned long lastSeenTime;    // millis() when last spotted
  int lastRssi;
  int riskScore;                 // 0-100, higher = more likely a hidden skimmer
  bool alerted;
  bool seenThisCycle;            // used internally for cleanup
};

TrackedDevice tracked[MAX_TRACKED];
int trackedCount = 0;

BLEScan* pBLEScan;

bool isTrusted(String mac) {
  for (int i = 0; i < trustedCount; i++) {
    if (trustedMacs[i] == mac) return true;
  }
  return false;
}

class ScanCallback: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String mac = String(advertisedDevice.getAddress().toString().c_str());
    int rssi = advertisedDevice.getRSSI();
    if (!isTrusted(mac)) {
      updateTracking(mac, rssi);
    }
  }
};

void updateTracking(String mac, int rssi) {
  unsigned long now = millis();

  for (int i = 0; i < trackedCount; i++) {
    if (tracked[i].mac == mac) {
      tracked[i].lastSeenTime = now;
      tracked[i].lastRssi = rssi;
      tracked[i].seenThisCycle = true;
      checkSuspicious(i);
      return;
    }
  }

  if (trackedCount < MAX_TRACKED) {
    tracked[trackedCount].mac = mac;
    tracked[trackedCount].firstSeenTime = now;
    tracked[trackedCount].lastSeenTime = now;
    tracked[trackedCount].lastRssi = rssi;
    tracked[trackedCount].riskScore = 0;
    tracked[trackedCount].alerted = false;
    tracked[trackedCount].seenThisCycle = true;
    trackedCount++;
  }
}

// Removes devices that haven't been seen in a while (they've left).
// Called once per scan cycle, after the scan completes.
void cleanupStaleDevices() {
  unsigned long now = millis();
  for (int i = 0; i < trackedCount; i++) {
    if (!tracked[i].seenThisCycle && (now - tracked[i].lastSeenTime > ABSENCE_TIMEOUT_MS)) {
      // Remove by shifting the rest of the array left
      for (int j = i; j < trackedCount - 1; j++) {
        tracked[j] = tracked[j + 1];
      }
      trackedCount--;
      i--; // re-check this index, now holds the next device
    }
  }
}

// Computes a 0-100 risk score from two factors:
// - persistence: how close the CONTINUOUS presence duration is to
//   PERSISTENCE_MINUTES (60% weight) - this is the core signal, since
//   a stationary device that never leaves is what we're looking for
// - signal strength: how close (strong) the RSSI is (40% weight)
int computeRiskScore(unsigned long firstSeenTime, unsigned long lastSeenTime, int rssi) {
  unsigned long duration = lastSeenTime - firstSeenTime;

  long persistenceFactor = map((long)duration, 0, (long)PERSISTENCE_MS, 0, 100);
  if (persistenceFactor > 100) persistenceFactor = 100;
  if (persistenceFactor < 0) persistenceFactor = 0;

  long rssiFactor = map(rssi, RSSI_FAR, RSSI_NEAR, 0, 100);
  if (rssiFactor < 0) rssiFactor = 0;
  if (rssiFactor > 100) rssiFactor = 100;

  int score = (int)((persistenceFactor * 60 + rssiFactor * 40) / 100);
  return score;
}

void checkSuspicious(int i) {
  tracked[i].riskScore = computeRiskScore(tracked[i].firstSeenTime, tracked[i].lastSeenTime, tracked[i].lastRssi);

  if (!tracked[i].alerted && tracked[i].riskScore >= 80) {
    Serial.print("ALERT: High risk score - possible hidden skimmer. MAC=");
    Serial.print(tracked[i].mac);
    Serial.print(" RSSI="); Serial.print(tracked[i].lastRssi);
    Serial.print(" minutesPresent="); Serial.print((tracked[i].lastSeenTime - tracked[i].firstSeenTime) / 60000.0);
    Serial.print(" risk="); Serial.println(tracked[i].riskScore);
    tracked[i].alerted = true;
    alertStatus = "ALERT: possible skimmer";
    triggerAlarm();
  } else if (tracked[i].riskScore >= 50 && tracked[i].riskScore < 80) {
    alertStatus = "Elevated risk - watching";
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new ScanCallback(), true);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  Wire.begin(8, 9); // ESP32-S3: SDA=GPIO8, SCL=GPIO9 (explicit, since defaults vary by board)

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed - check wiring/address (0x3C or 0x3D).");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("BLE Skimmer Scanner");
    display.println("Starting...");
    display.display();
  }

  Serial.println("BLE skimmer scanner active.");

  // Start WiFi hotspot for the dashboard
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("WiFi hotspot started. Connect to '");
  Serial.print(AP_SSID);
  Serial.print("' and browse to ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleDashboard);
  server.begin();
}

void handleDashboard() {
  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='3'>";
  html += "<title>BLE Skimmer Scanner</title>";
  html += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:20px;}";
  html += "h1{color:#4fc3a1;} table{width:100%;border-collapse:collapse;margin-top:10px;}";
  html += "th,td{padding:8px;border-bottom:1px solid #333;text-align:left;}";
  html += ".high{color:#ff6b6b;font-weight:bold;} .mid{color:#ffb74d;} .low{color:#8bc34a;}</style>";
  html += "</head><body>";
  html += "<h1>BLE Skimmer Scanner</h1>";
  html += "<p>Status: " + alertStatus + "</p>";
  html += "<p>Devices seen this cycle: " + String(lastDeviceCount) + " | Tracked: " + String(trackedCount) + "</p>";
  html += "<table><tr><th>MAC Address</th><th>RSSI</th><th>Present for</th><th>Risk Score</th></tr>";

  for (int i = 0; i < trackedCount; i++) {
    String riskClass = tracked[i].riskScore >= 80 ? "high" : (tracked[i].riskScore >= 50 ? "mid" : "low");
    unsigned long durationSec = (tracked[i].lastSeenTime - tracked[i].firstSeenTime) / 1000;
    String durationStr = String(durationSec / 60) + "m " + String(durationSec % 60) + "s";
    html += "<tr><td>" + tracked[i].mac + "</td><td>" + String(tracked[i].lastRssi) + " dBm</td>";
    html += "<td>" + durationStr + "</td>";
    html += "<td class='" + riskClass + "'>" + String(tracked[i].riskScore) + "</td></tr>";
  }

  html += "</table><p style='color:#888;font-size:12px;margin-top:20px;'>";
  html += "Auto-refreshes every 3 seconds. Full alert requires ~";
  html += String(PERSISTENCE_MINUTES) + " minutes of continuous presence at close range.</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void updateDisplay(int deviceCount) {
  int topScore = 0;
  for (int i = 0; i < trackedCount; i++) {
    if (tracked[i].riskScore > topScore) topScore = tracked[i].riskScore;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("BLE Skimmer Scanner");
  display.print("Devices seen: ");
  display.println(deviceCount);
  display.print("Tracked: ");
  display.println(trackedCount);
  display.print("Top risk: ");
  display.print(topScore);
  display.println("/100");
  display.setTextSize(1);
  display.println(alertStatus);
  display.display();
}

void loop() {
  server.handleClient();

  // TEST MODE: type 's' + Enter in Serial Monitor to simulate a
  // device that has ALREADY been present for the full persistence
  // window, so you can see a full alert without waiting 10 real
  // minutes.
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's') {
      Serial.println("SIMULATION: injecting fake long-dwelling device...");
      if (trackedCount < MAX_TRACKED) {
        unsigned long now = millis();
        tracked[trackedCount].mac = "SIMULATED:DEVICE";
        tracked[trackedCount].firstSeenTime = now - PERSISTENCE_MS; // backdate it
        tracked[trackedCount].lastSeenTime = now;
        tracked[trackedCount].lastRssi = -50;
        tracked[trackedCount].riskScore = 0;
        tracked[trackedCount].alerted = false;
        tracked[trackedCount].seenThisCycle = true;
        checkSuspicious(trackedCount);
        trackedCount++;
      }
    }
  }

  // Mark everyone as "not seen yet" before this cycle's scan
  for (int i = 0; i < trackedCount; i++) {
    tracked[i].seenThisCycle = false;
  }

  BLEScanResults* foundDevices = pBLEScan->start(SCAN_TIME_SEC, false);
  lastDeviceCount = foundDevices->getCount();
  Serial.print("Devices this cycle: ");
  Serial.println(lastDeviceCount);

  cleanupStaleDevices();
  updateDisplay(lastDeviceCount);
  pBLEScan->clearResults();
  delay(500);
}

void triggerAlarm() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
}
