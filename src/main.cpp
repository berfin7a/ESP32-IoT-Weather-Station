/*
 * Project: IoT Weather Station - FINAL PRO VERSION
 * Author: Berfin Akinci
 * Platform: ESP32-WROOM-32
 * Date: January 2026
 * Description: 
 * - SD Speed: 1 MHz (Stable)
 * - Auto Time Recovery: YES (Syncs automatically when WiFi returns)
 * - Power Safe: Optimized
 */

#include <Arduino.h>
#include <Wire.h>
#include "SH1106Wire.h" 
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include "ThingSpeak.h"
#include "time.h"         
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- CONFIGURATION ---
#define MQ135_PIN 32      
#define SD_CS_PIN 5       

// --- CALIBRATION ---
const int MIN_DISPLAY_VALUE = 135; 
const int SENSITIVITY_GAIN = 4;    

// --- WIFI & THINGSPEAK CREDENTIALS ---
const char* ssid = "YOUR_WIFI_NAME";          
const char* password = "YOUR_WIFI_PASSWORD";        

unsigned long myChannelNumber = 3212753;
const char * myWriteAPIKey = "7IEZ489MAT2UQIZW";

// --- TIME SETTINGS (NTP) ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 10800; // UTC+3 (Turkey Time)
const int   daylightOffset_sec = 0;

// --- OBJECT INITIALIZATION ---
SH1106Wire display(0x3C, 21, 22);
Adafruit_BME280 bme;
WiFiClient client;

// --- TIMER VARIABLES (Non-Blocking) ---
unsigned long prevTimeDisplay = 0;
unsigned long prevTimeSD = 0;
unsigned long prevTimeCloud = 0;
unsigned long prevTimeSDCheck = 0; 

// --- GLOBAL VARIABLES ---
float smoothedValue = 135.0; 
bool isCardMounted = false; 

// --- FUNCTION: GET TIMESTAMP ---
String getFullDateTime() {
  struct tm timeinfo;
  time_t now;
  
  time(&now);
  localtime_r(&now, &timeinfo);

  char timeStringBuff[50];
  // Excel compatible format: DD.MM.YYYY HH:MM:SS
  strftime(timeStringBuff, sizeof(timeStringBuff), "%d.%m.%Y %H:%M:%S", &timeinfo);
  
  return String(timeStringBuff);
}

// --- FUNCTION: CHECK/WRITE CSV HEADERS ---
void checkHeaders() {
    // 1MHz LOW SPEED for Stability
    File file = SD.open("/weather_data.csv", FILE_READ);
    bool isFileEmpty = !file || file.size() == 0;
    if(file) file.close();

    if(isFileEmpty) {
       File writeFile = SD.open("/weather_data.csv", FILE_WRITE);
       if(writeFile) {
         writeFile.println("Date_Time;Temperature_C;Humidity_Pct;Pressure_hPa;Air_Quality_Raw");
         writeFile.close();
         Serial.println("Headers initialized.");
       }
    }
}

// --- FUNCTION: ROBUST SD LOGGING ---
void logToSD(String dataMessage) {
  // Hot-Swap Logic
  if (!isCardMounted) {
    SD.end(); 
    // CRITICAL FIX: Speed lowered to 1MHz for stability
    if(SD.begin(SD_CS_PIN, SPI, 1000000)) { 
       isCardMounted = true;
       checkHeaders(); 
    } else {
       return; 
    }
  }
  
  File file = SD.open("/weather_data.csv", FILE_APPEND); 
  
  if(!file) {
    isCardMounted = false; 
    SD.end(); 
    return;
  }
  
  if(file.println(dataMessage)) {
    Serial.println("LOG: " + dataMessage);
    isCardMounted = true; 
  } else {
    file.close();
    isCardMounted = false; 
    SD.end(); 
  }
  file.close();
}

void setup() {
  // 1. Hardware Configuration
  analogSetAttenuation(ADC_11db); 
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);
  Wire.begin();

  // 2. Initialize Display
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  
  // 3. Initialize Sensors
  if (!bme.begin(0x76, &Wire)) {
     Serial.println("BME280 Sensor Error!");
  }

  // 4. Initialize SD Card (1 MHz Low Speed)
  if(!SD.begin(SD_CS_PIN, SPI, 1000000)) {
    isCardMounted = false;
    Serial.println("SD Card Failed or Missing!");
  } else {
    isCardMounted = true;
    checkHeaders();
  }
  
  // 5. Connect WiFi
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Connecting WiFi:");
  display.drawString(0, 15, ssid); 
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int tryCount = 0;
  while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
    delay(500); 
    tryCount++;
    Serial.print(".");
    display.drawString(tryCount * 5, 30, "."); 
    display.display();
  }
  
  display.clear();
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    display.drawString(0, 0, "WiFi Connected!");
    display.drawString(0, 15, "IP: " + WiFi.localIP().toString());
    
    // 6. Time Sync (NTP)
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    display.drawString(0, 35, "Time Sync OK!");
  } else {
    Serial.println("\nWiFi Failed!");
    display.drawString(0, 0, "WiFi Failed!");
    display.drawString(0, 15, "Mode: Offline Log");
  }
  
  // 7. Safety Net: Manual Fallback Date
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo, 0) || timeinfo.tm_year < 100) {
      Serial.println("Time Recovery Failed. Setting Manual Demo Date.");
      
      // Manual Set: January 19, 2026, 12:00:00
      struct tm tm_manual;
      tm_manual.tm_year = 2026 - 1900; 
      tm_manual.tm_mon  = 0;           
      tm_manual.tm_mday = 19;          
      tm_manual.tm_hour = 12;          
      tm_manual.tm_min  = 0;           
      tm_manual.tm_sec  = 0;           
      tm_manual.tm_isdst = 0;

      time_t t = mktime(&tm_manual);
      struct timeval now = { .tv_sec = t };
      settimeofday(&now, NULL);
  }

  display.display();
  delay(2000); 
  
  ThingSpeak.begin(client);
}

void loop() {
  unsigned long currentTime = millis();

  // --- TASK 1: HOT-SWAP SD CARD CHECK (Every 2 Seconds) ---
  if (!isCardMounted && (currentTime - prevTimeSDCheck >= 2000)) {
       prevTimeSDCheck = currentTime;
       SD.end(); 
       if (SD.begin(SD_CS_PIN, SPI, 1000000)) {
           isCardMounted = true;
           checkHeaders(); 
           Serial.println("SD Card Re-inserted and Mounted!");
       }
  }
  
  // --- TASK 2: SENSOR READING & DISPLAY UPDATE (Every 250ms) ---
  if (currentTime - prevTimeDisplay >= 250) { 
    prevTimeDisplay = currentTime;

    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0F;
    
    // Smooth Air Quality Readings
    long totalMQ = 0;
    for(int i=0; i<5; i++) { 
      totalMQ += analogRead(MQ135_PIN); delay(2);
    }
    int raw_reading = totalMQ / 5; 
    int calculatedValue = MIN_DISPLAY_VALUE + (raw_reading * SENSITIVITY_GAIN);
    smoothedValue = (smoothedValue * 0.8) + (calculatedValue * 0.2);
    int showVal = (int)smoothedValue;

    String statusTag = "";
    if (showVal < 160) statusTag = "[GOOD]";
    else if (showVal < 250) statusTag = "[FAIR]";
    else statusTag = "[POOR]";

    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "Weather St.");
    
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    String wifiIcon = (WiFi.status() == WL_CONNECTED) ? "[W]" : "[X]"; 
    String sdIcon = (isCardMounted) ? "[SD]" : "[NO]";
    display.drawString(100, 0, wifiIcon); 
    display.drawString(128, 0, sdIcon);   
    display.drawLine(0, 12, 128, 12);

    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 15, "Temperature : " + String(t, 1) + " C");
    display.drawString(0, 27, "Humidity      : %" + String((int)h));
    display.drawString(0, 39, "Pressure     : " + String((int)p) + " hPa");
    display.drawString(0, 51, "Air Quality   : " + String(showVal) + " " + statusTag);

    display.display();
  }

  // --- TASK 3: LOCAL DATA LOGGING (Every 1 Minute) ---
  if (currentTime - prevTimeSD >= 60000) { 
    prevTimeSD = currentTime;
    
    String timeStamp = getFullDateTime();
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0F;
    int air = (int)smoothedValue;
    
    // EXCEL FIX: Convert Dot to Comma
    String sTemp = String(t); sTemp.replace(".", ",");
    String sHum = String(h);  sHum.replace(".", ",");
    String sPres = String(p); sPres.replace(".", ",");
    
    String logLine = timeStamp + ";" + sTemp + ";" + sHum + ";" + sPres + ";" + String(air);
    logToSD(logLine);
  }

  // --- TASK 4: CLOUD TELEMETRY & AUTO-TIME RECOVERY (Every 20 Seconds) ---
  if (currentTime - prevTimeCloud >= 20000) {
    prevTimeCloud = currentTime;
    
    if(WiFi.status() == WL_CONNECTED) {
       
       // --- SMART TIME RECOVERY ---
       // Check if time is still "Manual/Wrong" (Year 2026 Manual set or before)
       // If WiFi is back, force an NTP update.
       struct tm timeinfo;
       if(getLocalTime(&timeinfo, 0)) {
           // If we are still in the "Manual Start" hour and minutes (roughly), try to sync
           // This is a gentle check, consumes no power.
           if(timeinfo.tm_year < (2025-1900) || (timeinfo.tm_hour == 12 && timeinfo.tm_min < 5)) {
              configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
           }
       }

       ThingSpeak.setField(1, bme.readTemperature());    
       ThingSpeak.setField(2, bme.readHumidity());    
       ThingSpeak.setField(3, bme.readPressure() / 100.0F);    
       ThingSpeak.setField(4, (int)smoothedValue);  
       ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
       Serial.println("Data sent to ThingSpeak");
    } else {
       Serial.println("WiFi Disconnected! Skipping upload.");
    }
  }
}