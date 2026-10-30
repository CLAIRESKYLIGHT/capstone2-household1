// ============================================================
// SMART BARANGAY SYSTEM - Household Device
// ESP32 + LCD + WiFi + NTP + BUZZER + RGB LED
// Adds HMAC verification + replay prevention
// Adds mesh forwarding: duplicate check, hop count (limit 30), forward-once with 0-50 ms jitter
// Announcement persists on screen with timestamp until replaced
// ============================================================
 
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "mbedtls/md.h"
 
// ===== CONFIGURATION =====
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET 28800
 
// Shared secret - MUST match the Gateway's exactly
#define SHARED_SECRET "BarangayOtaviSecretKey2026"
 
// ===== PIN DEFINITIONS =====
#define BUZZER_PIN 12
#define LED_R 25
#define LED_G 26
#define LED_B 27
 
// ===== LCD SETUP =====
LiquidCrystal_I2C lcd(0x27, 16, 2);
char timeBuffer[20];
char dateBuffer[20];
 
// ===== LED STATE TRACKING =====
bool greenBlinkState = false;
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 500;
 
// ===== PERSISTENT ANNOUNCEMENT TRACKING (paged display: whole message, 2 lines per page) =====
String lastMessage = "";
String lastTimestamp = "--:--";
bool hasAnnouncement = false;
 
#define MAX_WRAPPED_LINES 40                  // up to 40 lines x 16 chars = 640 characters
String wrappedLines[MAX_WRAPPED_LINES];
int wrappedCount = 0;
int pageIndex = 0;
bool pageDirty = true;
unsigned long lastPageTime = 0;
const unsigned long PAGE_INTERVAL = 2500;     // ms each page stays on the LCD (raise it for slower reading)
 
// ===== SECURITY: REPLAY PREVENTION =====
unsigned long lastAcceptedSequence = 0;
 
// ===== MESH FORWARDING (matches the ns-3 household simulation) =====
// Each household rebroadcasts the FIRST copy of a new message once, after a random
// 0-50 ms delay, with the hop count increased by 1. Messages at the hop limit are
// shown but not forwarded. No ACK and no retry (broadcast), as in ns-3.
#define HOP_LIMIT 30
#define FORWARD_JITTER_MS 50
 
// ===== FUNCTION: Compute HMAC-SHA256, return first 16 hex chars (8 bytes) =====
String computeHMAC(const String &data) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
 
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)SHARED_SECRET, strlen(SHARED_SECRET));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
 
  String hexResult = "";
  for (int i = 0; i < 8; i++) {
    char buf[3];
    sprintf(buf, "%02x", hmacResult[i]);
    hexResult += buf;
  }
  return hexResult;
}
 
// ===== FUNCTION: Set RGB LED Color =====
void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R, r ? HIGH : LOW);
  digitalWrite(LED_G, g ? HIGH : LOW);
  digitalWrite(LED_B, b ? HIGH : LOW);
}
 
void ledOff()     { setLED(false, false, false); }
void ledRed()     { setLED(true,  false, false); }
void ledGreen()   { setLED(false, true,  false); }
void ledBlue()    { setLED(false, false, true ); }
void ledYellow()  { setLED(true,  true,  false); }
 
// ===== FUNCTION: Update idle blinking green LED =====
void updateIdleLED() {
  if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = millis();
    greenBlinkState = !greenBlinkState;
    if (greenBlinkState) {
      ledGreen();
    } else {
      ledOff();
    }
  }
}
 
// ===== FUNCTION: Play Alert Sounds =====
void playAlert(int type) {
  if (type == 1) {
    for (int i = 0; i < 5; i++) {
      tone(BUZZER_PIN, 1000, 200);
      delay(300);
    }
  } else if (type == 2) {
    for (int i = 0; i < 2; i++) {
      tone(BUZZER_PIN, 600, 300);
      delay(400);
    }
  } else if (type == 3) {
    tone(BUZZER_PIN, 1500, 150);   // one brief beep
    delay(200);
  } else if (type == 4) {
    tone(BUZZER_PIN, 523, 150);
    delay(200);
    tone(BUZZER_PIN, 659, 150);
    delay(200);
    tone(BUZZER_PIN, 784, 300);
  } else {
    tone(BUZZER_PIN, 300, 100);
    delay(100);
  }
}
 
// ===== FUNCTION: Connect to WiFi =====
void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    ledYellow();
    delay(250);
    ledOff();
    delay(250);
    attempts++;
    Serial.print(".");
  }
 
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
 
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("✅ Connected!");
    lcd.setCursor(0, 1);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP().toString().substring(0, 12));
    playAlert(4);
    delay(2000);
  } else {
    ledRed();
  }
}
 
// ===== FUNCTION: Get Time from NTP =====
void getTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Updating Time");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
 
  configTime(UTC_OFFSET, 0, NTP_SERVER);
 
  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    attempts++;
    Serial.print(".");
  }
 
  if (attempts < 10) {
    Serial.println("\n✅ Time Synced!");
    playAlert(4);
  }
}
 
// ===== FUNCTION: Display Time on LCD (shown only when no announcement is pending) =====
void displayTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    lcd.setCursor(0, 1);
    lcd.print("Time Error      ");
    return;
  }

  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y", &timeinfo);

  lcd.setCursor(0, 0);
  lcd.print(timeBuffer);
  lcd.print("        ");     // clear the rest of the top line (16 columns)

  lcd.setCursor(0, 1);
  lcd.print(dateBuffer);
  lcd.print("      ");       // clear the rest of the bottom line
}
 
// ===== FUNCTION: Word-wrap a message into lines of at most 16 characters =====
int wrapText(const String &msg, String out[], int maxLines) {
  int n = 0;
  String cur = "";
  int len = msg.length();
  int i = 0;
  while (i < len && n < maxLines) {
    int j = i;
    while (j < len && msg[j] != ' ') j++;
    String word = msg.substring(i, j);
    i = j;
    while (i < len && msg[i] == ' ') i++;
    if (word.length() == 0) continue;
    while (word.length() > 16) {                       // a single word longer than a line
      if (cur.length() > 0) {
        out[n++] = cur;
        cur = "";
        if (n >= maxLines) return n;
      }
      out[n++] = word.substring(0, 16);
      word = word.substring(16);
      if (n >= maxLines) return n;
    }
    if (cur.length() == 0) {
      cur = word;
    } else if (cur.length() + 1 + word.length() <= 16) {
      cur += " ";
      cur += word;
    } else {
      out[n++] = cur;
      cur = word;
    }
  }
  if (cur.length() > 0 && n < maxLines) out[n++] = cur;
  return n;
}
 
String pad16(String text) {
  if ((int)text.length() > 16) text = text.substring(0, 16);
  while ((int)text.length() < 16) text += ' ';
  return text;
}
 
// ===== FUNCTION: Persistent announcement display (cycles through the pages until replaced) =====
void updatePersistentDisplay() {
  int textPages = (wrappedCount + 1) / 2;
  int totalPages = textPages + 1;                      // last page shows the time received
 
  if (millis() - lastPageTime >= PAGE_INTERVAL) {
    lastPageTime = millis();
    pageIndex = (pageIndex + 1) % totalPages;
    pageDirty = true;
  }
  if (!pageDirty) return;                              // redraw only when the page changes (no flicker)
  pageDirty = false;
 
  String line0, line1;
  if (pageIndex < textPages) {
    line0 = wrappedLines[pageIndex * 2];
    line1 = (pageIndex * 2 + 1 < wrappedCount) ? wrappedLines[pageIndex * 2 + 1] : "";
  } else {
    line0 = "Received " + lastTimestamp;
    line1 = "Barangay Otavi";
  }
  lcd.setCursor(0, 0);
  lcd.print(pad16(line0));
  lcd.setCursor(0, 1);
  lcd.print(pad16(line1));
}
 
// ===== FUNCTION: Handle a new VERIFIED incoming announcement =====
void displayAnnouncement(String message, int alertType) {
  if (alertType == 1) {
    for (int i = 0; i < 6; i++) {
      ledRed();
      delay(150);
      ledOff();
      delay(150);
    }
  } else {
    ledBlue();
  }
 
  playAlert(alertType);
  Serial.println("BEEP_DONE");   // tells the watcher the beep has finished, so the voice can start
 
  struct tm timeinfo;
  String timeStr = "--:--";
  if (getLocalTime(&timeinfo)) {
    char buf[10];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    timeStr = String(buf);
  }
 
  lcd.clear();
  lcd.setCursor(0, 0);
  if (alertType == 1) {
    lcd.print("EMERGENCY!");
  } else if (alertType == 2) {
    lcd.print("WARNING!");
  } else {
    lcd.print("ANNOUNCEMENT");
  }
  lcd.setCursor(0, 1);
  int cut = (int)message.length() > 16 ? 16 : (int)message.length();
  lcd.print(message.substring(0, cut));
  delay(600);
 
  Serial.print("RECEIVED:");
  Serial.println(message);
 
  lastMessage = message;
  lastTimestamp = timeStr;
  hasAnnouncement = true;
  wrappedCount = wrapText(message, wrappedLines, MAX_WRAPPED_LINES);
  pageIndex = 0;
  pageDirty = true;
  lastPageTime = millis();
}
 
// ===== FUNCTION: Parse a packet, verify, then process and forward =====
// New format: "sequence|hop|hmac|message"
// Old format: "sequence|hmac|message" (treated as hop 0, so the existing Gateway still works)
// The HMAC covers "sequence|message" only, because the hop count changes at every forward.
void processIncomingPacket(String packet) {
  int firstPipe = packet.indexOf('|');
  int secondPipe = packet.indexOf('|', firstPipe + 1);
 
  if (firstPipe == -1 || secondPipe == -1) {
    Serial.println("REJECTED: malformed packet (missing fields)");
    return;
  }
 
  String seqStr = packet.substring(0, firstPipe);
  String field2 = packet.substring(firstPipe + 1, secondPipe);
  String receivedHmac;
  String message;
  int hop = 0;
 
  // The HMAC is 16 characters; a hop count is 1-3 digits, so the two formats cannot be confused.
  bool hasHop = (field2.length() >= 1 && field2.length() <= 3);
  for (unsigned int i = 0; hasHop && i < field2.length(); i++) {
    if (!isDigit(field2[i])) hasHop = false;
  }
 
  if (hasHop) {
    int thirdPipe = packet.indexOf('|', secondPipe + 1);
    if (thirdPipe == -1) {
      Serial.println("REJECTED: malformed packet (missing fields)");
      return;
    }
    hop = field2.toInt();
    receivedHmac = packet.substring(secondPipe + 1, thirdPipe);
    message = packet.substring(thirdPipe + 1);
  } else {
    receivedHmac = field2;
    message = packet.substring(secondPipe + 1);
  }
 
  unsigned long sequenceNumber = seqStr.toInt();
 
  // ===== Duplicate check (normal in a mesh: the same message arrives from several neighbours) =====
  if (sequenceNumber == lastAcceptedSequence) {
    Serial.print("DUPLICATE: seq ");
    Serial.print(sequenceNumber);
    Serial.println(" already handled (not shown, not forwarded)");
    return;
  }
 
  // ===== Replay Prevention Check =====
  if (sequenceNumber < lastAcceptedSequence) {
    Serial.print("REJECTED: replay detected (seq ");
    Serial.print(sequenceNumber);
    Serial.print(" < last accepted ");
    Serial.print(lastAcceptedSequence);
    Serial.println(")");
    return;
  }
 
  // ===== Message Integrity / Authentication Check =====
  String signedPayload = seqStr + "|" + message;
  String expectedHmac = computeHMAC(signedPayload);
 
  if (receivedHmac != expectedHmac) {
    Serial.println("REJECTED: message integrity check failed (HMAC mismatch)");
    Serial.print("   Expected: ");
    Serial.println(expectedHmac);
    Serial.print("   Received: ");
    Serial.println(receivedHmac);
    return;
  }
 
  // ===== Passed both checks =====
  Serial.println("ACCEPTED: message authenticated and verified");
  lastAcceptedSequence = sequenceNumber;
 
  // ===== Forward once, BEFORE the slow local display, with random jitter =====
  if (hop < HOP_LIMIT) {
    delay(random(0, FORWARD_JITTER_MS + 1));
    Serial.print("FORWARD:");
    Serial.print(seqStr);
    Serial.print("|");
    Serial.print(hop + 1);
    Serial.print("|");
    Serial.print(receivedHmac);
    Serial.print("|");
    Serial.println(message);
  } else {
    Serial.print("HOPLIMIT: hop ");
    Serial.print(hop);
    Serial.println(" reached, shown but not forwarded");
  }
 
  displayAnnouncement(message, 3);
}
 
// ===== FUNCTION: Check for incoming announcement from Gateway (via Serial) =====
void handleIncomingAnnouncement() {
  if (Serial.available()) {
    String packet = Serial.readStringUntil('\n');
    packet.trim();
    if (packet.length() > 0) {
      processIncomingPacket(packet);
    }
  }
}
 
// ===== SETUP =====
void setup() {
  Serial.begin(115200);
 
  Serial.println("\n========================================");
  Serial.println("  SMART BARANGAY ANNOUNCEMENT SYSTEM");
  Serial.println("  ROLE: HOUSEHOLD DEVICE (with HMAC auth)");
  Serial.println("========================================\n");
 
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  ledOff();
 
  lcd.init();
  lcd.backlight();
  lcd.clear();
 
  lcd.setCursor(0, 0);
  lcd.print("Barangay Otavi");
  lcd.setCursor(0, 1);
  lcd.print("Smart Device");
  playAlert(4);
  delay(2000);
 
  connectWiFi();
 
  if (WiFi.status() == WL_CONNECTED) {
    getTime();
  }
 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("✅ System Ready");
  lcd.setCursor(0, 1);
  lcd.print("Barangay Otavi");
  playAlert(3);
  delay(2000);
 
  Serial.println("✅ System Ready!");
  Serial.println("   Waiting for authenticated announcements...");
}
 
// ===== LOOP =====
void loop() {
  if (hasAnnouncement) {
    updatePersistentDisplay();
  } else {
    displayTime();
  }
  updateIdleLED();
  handleIncomingAnnouncement();
  delay(100);
}