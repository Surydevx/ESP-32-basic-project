#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// ===== WiFi Credentials =====
const char* ssid = "PACMAN";
const char* password = "zzzzzzzzz";

// ===== ThingSpeak Credentials =====
const char* thingspeakServer = "https://api.thingspeak.com/update";
const char* thingspeakApiKey = "zzzzzzzzzzzzzzzz";  // Replace with your actual thingspeak write key
unsigned long lastThingspeakUpdate = 0;
const long thingspeakInterval = 15000;  // 15 seconds

// ===== Objects =====
Adafruit_BMP280 bmp;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);

// ===== Variables =====
float temp = 0, pressure_hPa = 0, pressure_atm = 0, altitude = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastSensorRead = 0;
const long lcdUpdateInterval = 1500;
const long sensorReadInterval = 1000;

// ===== Sea level pressure =====
#define SEALEVELPRESSURE_HPA (1013.25)

// ===== ThingSpeak Upload Function =====
void uploadToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Field 1: Temperature (°C)
    // Field 2: Pressure (ATM)
    // Field 3: Altitude (m)
    String url = String(thingspeakServer) + 
                 "?api_key=" + thingspeakApiKey +
                 "&field1=" + String(temp, 1) +           // Temperature in °C
                 "&field2=" + String(pressure_atm, 3) +   // Pressure in ATM
                 "&field3=" + String(altitude, 0);        // Altitude in meters
    
    Serial.println("Uploading to ThingSpeak:");
    Serial.println("  URL: " + url);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      if (httpCode == 200) {
        String response = http.getString();
        Serial.print("✓ ThingSpeak Upload Successful! Entry ID: ");
        Serial.println(response);
        
        // Show success on LCD briefly
        lcd.setCursor(12, 1);
        lcd.print("CL");
        delay(500);
        lcd.setCursor(12, 1);
        lcd.print("  ");
      } else {
        Serial.print("✗ ThingSpeak Upload Failed! HTTP Code: ");
        Serial.println(httpCode);
        
        // Show error on LCD
        lcd.setCursor(12, 1);
        lcd.print("ER");
        delay(500);
        lcd.setCursor(12, 1);
        lcd.print("  ");
      }
    } else {
      Serial.print("✗ ThingSpeak Connection Failed: ");
      Serial.println(http.errorToString(httpCode));
    }
    
    http.end();
  } else {
    Serial.println("✗ WiFi Not Connected - ThingSpeak Upload Skipped");
  }
}

// ===== JSON API using ArduinoJson =====
void handleData() {
  StaticJsonDocument<200> doc;
  doc["temp"] = temp;
  doc["pressure"] = pressure_atm;      // Send pressure in ATM
  doc["altitude"] = altitude;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ===== Web Page =====
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='30'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background: #1e1e2f; color: white; text-align: center; margin: 0; padding: 20px; }";
  html += ".card { background: #2c2c3e; padding: 20px; margin: 20px auto; width: 90%; max-width: 400px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.2); }";
  html += "h1 { color: #00d4ff; font-size: 24px; }";
  html += "p { font-size: 18px; margin: 15px 0; }";
  html += ".value { font-size: 24px; font-weight: bold; color: #00ffaa; }";
  html += ".unit { font-size: 14px; color: #aaa; }";
  html += ".status { color: #00d4ff; font-size: 12px; margin-top: 20px; }";
  html += ".thingspeak-link { margin-top: 20px; }";
  html += ".thingspeak-link a { color: #00d4ff; text-decoration: none; }";
  html += "@media (max-width: 600px) { .card { width: 95%; } p { font-size: 16px; } .value { font-size: 20px; } }";
  html += "</style>";
  html += "</head><body>";
  html += "<h1>🌦 ESP32 Weather Station</h1>";
  html += "<div class='card'>";
  html += "<p>🌡 Temperature: <span class='value' id='temp'>--</span> <span class='unit'>°C</span></p>";
  html += "<p>🌬 Pressure: <span class='value' id='pressure'>--</span> <span class='unit'>atm</span></p>";
  html += "<p>⛰ Altitude: <span class='value' id='altitude'>--</span> <span class='unit'>m</span></p>";
  html += "<div class='status'>🔄 Updating every 2 seconds</div>";
  html += "<div class='thingspeak-link'>📈 <a href='https://thingspeak.com/channels/zzzzzzz' target='_blank'>View on ThingSpeak</a></div>";
  html += "</div>";
  html += "<script>";
  html += "function updateData() {";
  html += "fetch('/data')";
  html += ".then(res => { if(!res.ok) throw new Error('Network error'); return res.json(); })";
  html += ".then(data => {";
  html += "document.getElementById('temp').innerText = data.temp.toFixed(1);";
  html += "document.getElementById('pressure').innerText = data.pressure.toFixed(3);";
  html += "document.getElementById('altitude').innerText = data.altitude.toFixed(0);";
  html += "})";
  html += ".catch(err => { console.error('Error:', err); });";
  html += "}";
  html += "setInterval(updateData, 2000);";
  html += "updateData();";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html; charset=utf-8", html);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n=== ESP32 Weather Station Starting ===");
  Serial.println("ThingSpeak Fields:");
  Serial.println("  Field 1: Temperature (°C)");
  Serial.println("  Field 2: Pressure (ATM)");
  Serial.println("  Field 3: Altitude (m)");
  
  // I2C with specific pins
  Wire.begin(21, 22);
  Serial.println("I2C initialized on pins SDA=21, SCL=22");
  
  // BMP280 with address detection
  Serial.print("Initializing BMP280... ");
  bool bmpFound = false;
  
  if (!bmp.begin(0x76)) {
    Serial.println("not found at 0x76, trying 0x77...");
    if (!bmp.begin(0x77)) {
      Serial.println("BMP280 not found at any address!");
      Serial.println("Please check wiring: VCC→3.3V, GND→GND, SDA→21, SCL→22");
      while (1) {
        delay(1000);
        Serial.println("Waiting for BMP280 sensor...");
      }
    } else {
      bmpFound = true;
      Serial.println("found at 0x77!");
    }
  } else {
    bmpFound = true;
    Serial.println("found at 0x76!");
  }
  
  if (bmpFound) {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
    Serial.println("BMP280 configured successfully!");
  }
  
  // LCD initialization
  Serial.print("Initializing LCD... ");
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Weather Station");
  lcd.setCursor(0, 1);
  lcd.print("Booting...");
  Serial.println("LCD initialized!");
  
  // WiFi connection
  Serial.print("Connecting to WiFi");
  lcd.clear();
  lcd.print("WiFi Connecting");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts % 10 == 0) {
      lcd.setCursor(0, 1);
      lcd.print("Attempt ");
      lcd.print(attempts/2);
    }
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi Connected!");
    Serial.print("  IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    lcd.clear();
    lcd.print("WiFi OK!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(3000);
  } else {
    Serial.println("✗ WiFi Connection Failed!");
    lcd.clear();
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Check SSID/PWD");
    delay(5000);
  }
  
  // Web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Web server started!");
  Serial.print("  Access at: http://");
  Serial.println(WiFi.localIP());
  
  // ThingSpeak initialization message
  Serial.println("ThingSpeak configured!");
  Serial.print("  Update interval: ");
  Serial.print(thingspeakInterval / 1000);
  Serial.println(" seconds");
  
  lcd.clear();
  lcd.print("Ready!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(2000);
  lcd.clear();
}

void loop() {
  // Handle web server requests
  server.handleClient();
  
  // Read sensors periodically
  if (millis() - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = millis();
    
    // Read sensor data
    temp = bmp.readTemperature();
    pressure_hPa = bmp.readPressure() / 100.0F;
    pressure_atm = pressure_hPa / 1013.25;  // Convert hPa to ATM
    altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);
    
    // Validate readings
    if (isnan(temp)) temp = 0;
    if (isnan(pressure_hPa)) {
      pressure_hPa = 0;
      pressure_atm = 0;
    }
    if (isnan(altitude)) altitude = 0;
    
    // Serial output
    Serial.print("📊 Sensor Data | Temp: ");
    Serial.print(temp, 1);
    Serial.print("°C | Pressure: ");
    Serial.print(pressure_hPa, 1);
    Serial.print(" hPa (");
    Serial.print(pressure_atm, 3);
    Serial.print(" atm) | Altitude: ");
    Serial.print(altitude, 0);
    Serial.println(" m");
  }
  
  // Upload to ThingSpeak periodically
  if (millis() - lastThingspeakUpdate >= thingspeakInterval) {
    lastThingspeakUpdate = millis();
    uploadToThingSpeak();
  }
  
  // Update LCD without blocking
  if (millis() - lastLCDUpdate >= lcdUpdateInterval) {
    lastLCDUpdate = millis();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C");
    
    lcd.setCursor(8, 0);
    lcd.print("P:");
    lcd.print(pressure_atm, 3);
    
    lcd.setCursor(0, 1);
    lcd.print("Alt:");
    lcd.print(altitude, 0);
    lcd.print("m");
    
    if (WiFi.status() == WL_CONNECTED) {
      lcd.setCursor(14, 1);
      lcd.print("W");
    } else {
      lcd.setCursor(14, 1);
      lcd.print("!");
    }
  }
}
//Done!, hehe:)
