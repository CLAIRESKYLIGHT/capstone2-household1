// ============================================================
// SMART BARANGAY SYSTEM - Household Device (Phase 1)
// ESP32 + LCD + WiFi + NTP Time
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

// ===== LCD SETUP =====
LiquidCrystal_I2C lcd(0x27, 16, 2);
char timeBuffer[20];
char dateBuffer[20];

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
  lcd.print("   "); // Clear extra characters
}

// ===== FUNCTION: Display Announcement =====
void displayAnnouncement(String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("📢 ANNOUNCEMENT");
  lcd.setCursor(0, 1);
  
  if (message.length() > 16) {
    // Show first 16 chars then scroll
    lcd.print(message.substring(0, 16));
    delay(2000);
    lcd.setCursor(0, 1);
    lcd.print(message.substring(16, message.length()));
    delay(2000);
  } else {
    lcd.print(message);
    delay(3000);
  }
  
  lcd.clear();
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("  SMART BARANGAY ANNOUNCEMENT SYSTEM");
  Serial.println("  Household Device - Phase 1");
  Serial.println("========================================\n");
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Show startup screen
  lcd.setCursor(0, 0);
  lcd.print("Barangay Otavi");
  lcd.setCursor(0, 1);
  lcd.print("Smart Device");
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
  delay(2000);
  
  Serial.println("✅ System Ready!");
}

// ===== LOOP =====
void loop() {
  displayTime();
  
  // Example: Display a test announcement every 10 seconds
  static unsigned long lastAnnouncement = 0;
  if (millis() - lastAnnouncement > 10000) {
    lastAnnouncement = millis();
    displayAnnouncement("Welcome to Barangay Otavi Smart System");
  }
  
  delay(1000);
}