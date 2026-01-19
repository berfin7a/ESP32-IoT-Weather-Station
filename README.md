# 🌦️ IoT Weather Station with ESP32

This repository contains the firmware source code for an **IoT-based Weather Monitoring System** developed as a B.Sc. graduation project. The system monitors environmental parameters in real-time and logs data to both a local SD card and the ThingSpeak cloud platform.

## 🚀 Key Features
* **MCU:** ESP32 DevKit V1
* **Sensors:** BME280 (Temp/Hum/Pressure), MQ-135 (Air Quality)
* **Storage:** Custom **Hot-Swap SD Card Logging** algorithm (No restart required).
* **Display:** SH1106 OLED (Real-time metrics & connection status).
* **Connectivity:** Wi-Fi Station Mode with NTP Time Synchronization.
* **Cloud:** ThingSpeak REST API integration.

## 🔌 Pin Configuration (Wiring)

| Component | ESP32 Pin | Function |
| :--- | :--- | :--- |
| **BME280** | GPIO 21 (SDA), 22 (SCL) | I2C Sensor |
| **OLED Display** | GPIO 21 (SDA), 22 (SCL) | I2C Display |
| **MQ-135** | GPIO 34 (Analog) | Gas Sensor |
| **SD Card** | GPIO 5 (CS), 18 (SCK), 19 (MISO), 23 (MOSI) | SPI Storage |

## 🛠️ Installation & Setup
1.  Install **Visual Studio Code** and the **PlatformIO** extension.
2.  Clone this repository:
    ```bash
    git clone [https://github.com/berfin7a/ESP32-IoT-Weather-Station.git](https://github.com/berfin7a/ESP32-IoT-Weather-Station.git)
    ```
3.  Open the project folder in PlatformIO.
4.  Update `wifi_credentials.h` (or main code) with your Wi-Fi SSID and Password.
5.  Upload the code to the ESP32 board.

## 📊 Data Format
The system logs data to the SD card in CSV format, optimized for regional standards (`;` delimiter):
```csv
Date;Time;Temperature;Humidity;Pressure;Gas_Level
2026-01-20;14:30:00;24.5;45;1013;135
