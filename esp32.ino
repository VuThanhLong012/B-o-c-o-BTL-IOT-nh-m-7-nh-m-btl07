#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// WiFi và Firebase configs
#define WIFI_SSID "DESKTOP-5MJ8D7G 0345"
#define WIFI_PASSWORD "J437,13w"
#define API_KEY "AIzaSyBkK6-57rXAPSyUBZ3Ug-9E6o2XlJyxMes"
#define DATABASE_URL "iot-smart-home-a9896-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "7IM4H4TkdgxKgDaIgrp1yF1hjlez6ySm81bLpLP9"  
#define RX2_PIN 16
#define TX2_PIN 17

// Cấu trúc thiết bị
struct Device {
    String id;         // ID thiết bị (ví dụ: "light1", "fan1")
    String type;       // Loại thiết bị ("L", "F", "A")
    int level;         // Mức độ hiện tại
    bool isConnected;  // Trạng thái kết nối
};

// Mảng động để lưu thiết bị
Device devices[10];    // Tối đa 10 thiết bị
int numDevices = 0;    // Số lượng thiết bị hiện tại

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String currentMode = "manual";
bool isProcessingCommand = false;
bool isFirebaseReady = false;
float temperature, humidity;
int lightValue;

void setup() {
    Serial.begin(9600);
    Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
    
    Serial.println("\nStarting ESP32...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWiFi Connected!");
    
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    if (Firebase.ready()) {
        Serial.println("Connected to Firebase!");
        isFirebaseReady = true;
        loadDevicesFromFirebase();
    }
}

void loadDevicesFromFirebase() {
    // Load thiết bị từ phòng 1
    if (Firebase.getJSON(fbdo, "/rooms/room1/devices")) {
        FirebaseJson &json = fbdo.jsonObject();
        size_t count = json.iteratorBegin();
        numDevices = 0;
        
        // Định nghĩa thứ tự thiết bị cố định
        const char* deviceOrder[] = {"light", "fan", "ac"};
        const char* deviceTypes[] = {"L", "F", "A"};
        
        // Load thiết bị theo thứ tự cố định
        for (int i = 0; i < 3; i++) {
            FirebaseJsonData deviceData;
            json.get(deviceData, deviceOrder[i]);
            
            if (deviceData.success && deviceData.type != "null") {
                FirebaseJson deviceJson;
                deviceJson.setJsonData(deviceData.stringValue);
                
                FirebaseJsonData typeData;
                deviceJson.get(typeData, "type");
                
                if (typeData.success && typeData.stringValue == "control") {
                    devices[numDevices].id = deviceOrder[i];
                    devices[numDevices].type = deviceTypes[i];
                    devices[numDevices].level = 0;
                    devices[numDevices].isConnected = true;
                    numDevices++;
                }
            }
        }
        
        json.iteratorEnd();
        initializeFirebase();
        
        // Debug log
        Serial.println("Loaded devices:");
        for(int i = 0; i < numDevices; i++) {
            Serial.println("Device " + String(i) + ": " + devices[i].id + " -> Type: " + devices[i].type);
        }
    }
}

void initializeFirebase() {
    Firebase.setString(fbdo, "/rooms/room1/controls/mode", "manual");
    Firebase.setInt(fbdo, "/rooms/room1/controls/deviceLevels/light", 0);
    Firebase.setInt(fbdo, "/rooms/room1/controls/deviceLevels/fan", 0);
    Firebase.setInt(fbdo, "/rooms/room1/controls/deviceLevels/ac", 0);
}

void readControlsFromFirebase() {
    if (isProcessingCommand) return;
    
    static unsigned long lastRead = 0;
    if (millis() - lastRead < 1000) return;
    lastRead = millis();

    // Đọc mode
    if (Firebase.getString(fbdo, "/rooms/room1/controls/mode")) {
        String newMode = fbdo.stringData();
        if (newMode != currentMode) {
            currentMode = newMode;
            isProcessingCommand = true;
            
            // Gửi lệnh chuyển mode
            Serial2.println(currentMode == "sensor" ? "MODE_SENSOR" : "MODE_MANUAL");
            
            // Nếu chuyển sang manual mode, reset tất cả thiết bị về 0
            if (currentMode == "manual") {
                delay(100); // Đợi một chút để Arduino xử lý lệnh chuyển mode
                
                // Gửi lệnh reset từng thiết bị
                Serial2.println("L0");
                delay(50);
                Serial2.println("F0");
                delay(50);
                Serial2.println("A0");
                
                // Reset trạng thái trên Firebase
                Firebase.setInt(fbdo, "/rooms/room1/controls/deviceLevels/light", 0);
                Firebase.setInt(fbdo, "/rooms/room1/controls/deviceLevels/fan", 0);
                Firebase.setInt(fbdo, "/rooms/room1/controls/deviceLevels/ac", 0);
                
                // Reset trạng thái local
                for(int i = 0; i < numDevices; i++) {
                    devices[i].level = 0;
                }
            }
            
            isProcessingCommand = false;
        }
    }

    // Xử lý trong mode manual
    if (currentMode == "manual") {
        // Đọc trạng thái từng thiết bị
        const char* deviceIds[] = {"light", "fan", "ac"};
        for(int i = 0; i < numDevices; i++) {
            String path = "/rooms/room1/controls/deviceLevels/" + String(deviceIds[i]);
            if (Firebase.getInt(fbdo, path)) {
                int newLevel = fbdo.intData();
                if (newLevel != devices[i].level) {
                    isProcessingCommand = true;
                    String command = devices[i].type + String(newLevel);
                    Serial.println("Sending command: " + command);
                    Serial2.println(command);
                    devices[i].level = newLevel;
                    delay(50); // Thêm delay nhỏ giữa các lệnh
                    isProcessingCommand = false;
                }
            }
        }
    }
}

void sendToFirebase() {
    // Gửi dữ liệu cảm biến
    if (temperature != -999) {
        Firebase.setFloat(fbdo, "/rooms/room1/sensors/temperature", temperature);
    } else {
        // Thay đổi từ "NULL" thành -999
        Firebase.setFloat(fbdo, "/rooms/room1/sensors/temperature", -999);
    }
    
    if (humidity != -999) {
        Firebase.setFloat(fbdo, "/rooms/room1/sensors/humidity", humidity);
    } else {
        // Thay đổi từ "NULL" thành -999
        Firebase.setFloat(fbdo, "/rooms/room1/sensors/humidity", -999);
    }
    
    if (lightValue != -999) {
        Firebase.setInt(fbdo, "/rooms/room1/sensors/light", lightValue);
    } else {
        // Thay đổi từ "NULL" thành -999
        Firebase.setInt(fbdo, "/rooms/room1/sensors/light", -999);
    }
    
    // Gửi trạng thái thiết bị trong mode sensor
    if (currentMode == "sensor") {
        const char* deviceIds[] = {"light", "fan", "ac"};
        for(int i = 0; i < numDevices; i++) {
            String path = "/rooms/room1/controls/deviceLevels/" + String(deviceIds[i]);
            if (devices[i].isConnected) {
                Firebase.setInt(fbdo, path, devices[i].level);
            } else {
                Firebase.setInt(fbdo, path, -999);  // Thay đổi từ "NULL" thành -999
            }
        }
    }
}

bool parseData(String data) {
    int tIndex = data.indexOf("T:");
    int hIndex = data.indexOf(",H:");
    int lIndex = data.indexOf(",L:");
    int sIndex = data.indexOf(",S:");
    
    if (tIndex == -1 || hIndex == -1 || lIndex == -1 || sIndex == -1) {
        return false;
    }

    // Parse sensor values
    String tempStr = data.substring(tIndex + 2, hIndex);
    String humidStr = data.substring(hIndex + 3, lIndex);
    String lightStr = data.substring(lIndex + 3, sIndex);
    
    temperature = tempStr == "NULL" ? -999 : tempStr.toFloat();
    humidity = humidStr == "NULL" ? -999 : humidStr.toFloat();
    lightValue = lightStr == "NULL" ? -999 : lightStr.toInt();
    
    // Parse device states
    String states = data.substring(sIndex + 3);
    int startPos = 0;
    int deviceIndex = 0;
    
    while (startPos < states.length() && deviceIndex < numDevices) {
        int commaPos = states.indexOf(',', startPos);
        if (commaPos == -1) commaPos = states.length();
        
        String levelStr = states.substring(startPos, commaPos);
        if (levelStr == "NULL") {
            devices[deviceIndex].isConnected = false;
            devices[deviceIndex].level = -1;
        } else {
            devices[deviceIndex].isConnected = true;
            devices[deviceIndex].level = levelStr.toInt();
        }
        
        startPos = commaPos + 1;
        deviceIndex++;
    }
    
    return true;
}

void loop() {
    if (!isFirebaseReady) {
        Serial.println("Firebase not ready. Retrying...");
        delay(1000);
        return;
    }

    if (Serial2.available()) {
        String data = Serial2.readStringUntil('\n');
        Serial.println("Received from Arduino: " + data);
        
        if (data.startsWith("DATA:")) {
            if (parseData(data)) {
                Serial.println("\n--- Parsed Data ---");
                Serial.println("Temperature: " + (temperature != -999 ? String(temperature) : "NULL"));
                Serial.println("Humidity: " + (humidity != -999 ? String(humidity) : "NULL"));
                Serial.println("Light: " + (lightValue != -999 ? String(lightValue) : "NULL"));
                
                Serial.println("Device States:");
                for(int i = 0; i < numDevices; i++) {
                    Serial.println(devices[i].id + ": " + 
                                 (devices[i].isConnected ? String(devices[i].level) : "NULL"));
                }
                
                sendToFirebase();
            }
        }
    }

    readControlsFromFirebase();
    delay(1000);
}