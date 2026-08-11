// ============================================================
// SMART BARANGAY SYSTEM - Household Device (Phase 2)
// ESP32 + LCD + WiFi + NTP + BUZZER
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ===== CONFIGURATION =====
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET 0

// ===== PIN DEFINITIONS =====
#define BUZZER_PIN 12

// ===== LCD SETUP =====
LiquidCrystal_I2C lcd(0x27, 16, 2);
char timeBuffer[20];
char dateBuffer[20];

// ===== FUNCTION: Play Alert Sounds =====
void playAlert(int type) {
  if (type == 1) { // EMERGENCY ALERT - Fast beeps
    for (int i = 0; i < 5; i++) {
      tone(BUZZER_PIN, 1000, 200);
      delay(300);
    }
  } else if (type == 2) { // WARNING ALERT - Two beeps
    for (int i = 0; i < 2; i++) {
      tone(BUZZER_PIN, 600, 300);
      delay(400);
    }
  } else if (type == 3) { // NORMAL ALERT - Single beep
    tone(BUZZER_PIN, 400, 200);
    delay(200);
  } else if (type == 4) { // SUCCESS ALERT - Happy tune
    tone(BUZZER_PIN, 523, 150);
    delay(200);
    tone(BUZZER_PIN, 659, 150);
    delay(200);
    tone(BUZZER_PIN, 784, 300);
  } else { // DEFAULT - Short beep
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
    delay(500);
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
    playAlert(4); // Success tune
    delay(2000);
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
    playAlert(4); // Success tune
  }
}

// ===== FUNCTION: Display Time on LCD =====
void displayTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    lcd.setCursor(0, 1);
    lcd.print("Time Error");
    return;
  }
  
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y", &timeinfo);
  
  lcd.setCursor(0, 0);
  lcd.print(timeBuffer);
  
  lcd.setCursor(0, 1);
  lcd.print(dateBuffer);
  lcd.print("   ");
}

// ===== FUNCTION: Display Announcement with Sound =====
void displayAnnouncement(String message, int alertType) {
  // Play the alert sound first
  playAlert(alertType);
  
  // Display the announcement
  lcd.clear();
  lcd.setCursor(0, 0);
  
  // Show alert type
  if (alertType == 1) {
    lcd.print("🚨 EMERGENCY!");
  } else if (alertType == 2) {
    lcd.print("⚠️ WARNING!");
  } else {
    lcd.print("📢 ANNOUNCEMENT");
  }
  
  lcd.setCursor(0, 1);
  
  // Display message (with scrolling if long)
  if (message.length() > 16) {
    lcd.print(message.substring(0, 16));
    delay(1500);
    lcd.setCursor(0, 1);
    lcd.print(message.substring(16, message.length()));
    delay(1500);
  } else {
    lcd.print(message);
    delay(3000);
  }
  
  lcd.clear();
}

// ===== FUNCTION: Test Announcements =====
void testAnnouncement(int type) {
  if (type == 1) {
    displayAnnouncement("TYPHOON ALERT: No sailing today!", 1);
  } else if (type == 2) {
    displayAnnouncement("HEAVY RAIN WARNING: Stay indoors", 2);
  } else {
    displayAnnouncement("Barangay Meeting at 5PM today", 3);
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("  SMART BARANGAY ANNOUNCEMENT SYSTEM");
  Serial.println("  Household Device - Phase 2 (with Buzzer)");
  Serial.println("========================================\n");
  
  // Initialize buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Show startup screen
  lcd.setCursor(0, 0);
  lcd.print("Barangay Otavi");
  lcd.setCursor(0, 1);
  lcd.print("Smart Device");
  playAlert(4); // Startup tune
  delay(2000);
  
  // Connect to WiFi
  connectWiFi();
  
  // Get time if WiFi connected
  if (WiFi.status() == WL_CONNECTED) {
    getTime();
  }
  
  // Show ready status
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("✅ System Ready");
  lcd.setCursor(0, 1);
  lcd.print("Barangay Otavi");
  playAlert(3);
  delay(2000);
  
  Serial.println("✅ System Ready!");
  Serial.println("   Testing alerts...");
  
  // Show test announcements
  testAnnouncement(1);
  testAnnouncement(2);
  testAnnouncement(3);
}

// ===== LOOP =====
void loop() {
  displayTime();
  
  // Show a test announcement every 15 seconds
  static unsigned long lastAnnouncement = 0;
  static int counter = 0;
  
  if (millis() - lastAnnouncement > 15000) {
    lastAnnouncement = millis();
    counter++;
    
    if (counter % 3 == 1) {
      displayAnnouncement("TYPHOON ALERT: No sailing", 1);
    } else if (counter % 3 == 2) {
      displayAnnouncement("HEAVY RAIN WARNING", 2);
    } else {
      displayAnnouncement("Barangay Meeting 5PM", 3);
    }
  }
  
  delay(1000);
}