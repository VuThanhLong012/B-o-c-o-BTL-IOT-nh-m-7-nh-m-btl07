#include <DHT.h>

// Định nghĩa chân
#define LIGHT_SENSOR_A0 A0
#define LIGHT_SENSOR_D0 2
#define LIGHT_LED 13    
#define FAN_LED 12      
#define AC_LED 11       
#define DHT_PIN 7
#define DHT_TYPE DHT11

// Định nghĩa mode
#define MODE_MANUAL 0
#define MODE_SENSOR 1

// Định nghĩa các mức độ điều khiển
#define LEVEL_OFF 0
#define LEVEL_LOW 1
#define LEVEL_MED 2
#define LEVEL_HIGH 3
#define LEVEL_MAX 4

// Định nghĩa ngưỡng cảm biến
#define LIGHT_LOW 200
#define LIGHT_MED 500

// Ngưỡng nhiệt độ cho quạt
#define TEMP_FAN_LOW 25
#define TEMP_FAN_MED 28
#define HUMID_LOW 50
#define HUMID_HIGH 70

// Ngưỡng nhiệt độ cho điều hòa
#define TEMP_AC_LOW 25
#define TEMP_AC_MED 28
#define TEMP_AC_HIGH 30

DHT dht(DHT_PIN, DHT_TYPE);

// Biến toàn cục
int currentMode = MODE_MANUAL;
int deviceLevels[3] = {0, 0, 0}; // light, fan, ac

void setup() {
    Serial.begin(9600);
    pinMode(LIGHT_LED, OUTPUT);
    pinMode(FAN_LED, OUTPUT);
    pinMode(AC_LED, OUTPUT);
    pinMode(LIGHT_SENSOR_D0, INPUT);
    dht.begin();
    Serial.println("Arduino started");
}

void loop() {
    // Đọc và kiểm tra cảm biến ánh sáng
    int lightDigital = digitalRead(LIGHT_SENSOR_D0);
    int lightAnalog = analogRead(LIGHT_SENSOR_A0);
    int lightValue;
    
    // Kiểm tra kết nối cảm biến ánh sáng
    static int lastValidValue = 0;
    static unsigned long lastChange = 0;
    
    if (abs(lightAnalog - lastValidValue) > 100) {  // Nếu giá trị thay đổi đột ngột
        if (millis() - lastChange < 100) {  // Và thay đổi quá nhanh
            lightValue = -999;  // Coi như cảm biến bị ngắt
        } else {
            lastValidValue = lightAnalog;
            lastChange = millis();
            lightValue = lightAnalog;
        }
    } else {
        lastValidValue = lightAnalog;
        lightValue = lightAnalog;
    }
    
    // // Nếu chân digital báo lỗi, coi như cảm biến bị ngắt
    // if (lightDigital == LOW) {
    //     lightValue = -999;
    // }
    
    // Đọc và kiểm tra giá trị từ DHT
    float temp = dht.readTemperature();
    float humid = dht.readHumidity();
    
    // Kiểm tra giá trị hợp lệ từ DHT
    String tempStr, humidStr;
    if (isnan(temp)) {
        temp = -999;
        tempStr = "-999";
    } else {
        tempStr = String(temp);
    }
    
    if (isnan(humid)) {
        humid = -999;
        humidStr = "-999";
    } else {
        humidStr = String(humid);
    }
    
    // Debug thông tin cảm biến
    Serial.print("Sensors - Light: ");
    Serial.print(lightValue == -999 ? "DISCONNECTED" : String(lightValue));
    Serial.print(" | Temp: ");
    Serial.print(tempStr);
    Serial.print(" | Humid: ");
    Serial.println(humidStr);
    
    // Xử lý lệnh từ Serial
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      
      Serial.print("Received command: '");
      Serial.print(command);
      Serial.println("'");
      
      // Xử lý chuyển mode
      if (command.equals("MODE_MANUAL")) {
          currentMode = MODE_MANUAL;
          Serial.println("Switched to MANUAL mode");
      }
      else if (command.equals("MODE_SENSOR")) {
          currentMode = MODE_SENSOR;
          Serial.println("Switched to SENSOR mode");
      }
      // Xử lý điều khiển thiết bị trong mode manual
      else if (currentMode == MODE_MANUAL) {
          if (command.startsWith("L")) {
              int level = command.substring(1).toInt();
              switch(level) {
                  case LEVEL_OFF: analogWrite(LIGHT_LED, 0); break;    // 0%
                  case LEVEL_LOW: analogWrite(LIGHT_LED, 64); break;   // 25%
                  case LEVEL_MED: analogWrite(LIGHT_LED, 128); break;  // 50%
                  case LEVEL_HIGH: analogWrite(LIGHT_LED, 192); break; // 75%
                  case LEVEL_MAX: analogWrite(LIGHT_LED, 255); break;  // 100%
              }
              deviceLevels[0] = level; // Sửa: Gán trực tiếp giá trị level
              Serial.println("Light set to level: " + String(level));
          }
          else if (command.startsWith("F")) {
            int level = command.substring(1).toInt();
            switch(level) {
                case LEVEL_OFF: analogWrite(FAN_LED, 0); break;
                case LEVEL_LOW: analogWrite(FAN_LED, 64); break;
                case LEVEL_MED: analogWrite(FAN_LED, 128); break;
                case LEVEL_HIGH: analogWrite(FAN_LED, 192); break;
                case LEVEL_MAX: analogWrite(FAN_LED, 255); break;
            }
            deviceLevels[1] = level; // Sửa: Gán trực tiếp giá trị level
            Serial.println("Fan set to level: " + String(level));
        }
        else if (command.startsWith("A")) {
            int level = command.substring(1).toInt();
            switch(level) {
                case LEVEL_OFF: analogWrite(AC_LED, 0); break;
                case LEVEL_LOW: analogWrite(AC_LED, 64); break;
                case LEVEL_MED: analogWrite(AC_LED, 128); break;
                case LEVEL_HIGH: analogWrite(AC_LED, 192); break;
                case LEVEL_MAX: analogWrite(AC_LED, 255); break;
            }
            deviceLevels[2] = level; // Sửa: Gán trực tiếp giá trị level
            Serial.println("AC set to level: " + String(level));
        }
      }
    }

    // Xử lý mode sensor
    if (currentMode == MODE_SENSOR) {
      // Điều khiển đèn dựa trên ánh sáng
      if (lightValue > 900) {         // Rất tối
          analogWrite(LIGHT_LED, 255); // 100%
          deviceLevels[0] = 4;
      } 
      else if (lightValue > 700) {    // Khá tối
          analogWrite(LIGHT_LED, 192); // 75%
          deviceLevels[0] = 3;
      }
      else if (lightValue > 500) {    // Hơi tối
          analogWrite(LIGHT_LED, 128); // 50%
          deviceLevels[0] = 2;
      }
      else if (lightValue > 300) {    // Ít tối
          analogWrite(LIGHT_LED, 64);  // 25%
          deviceLevels[0] = 1;
      }
      else {                          // Đủ sáng
          analogWrite(LIGHT_LED, 0);   // 0%
          deviceLevels[0] = 0;
      }
      
      // Điều khiển quạt dựa trên nhiệt độ và độ ẩm
      if (temp > 32 || humid > 80) {
          analogWrite(FAN_LED, 255);  // 100%
          deviceLevels[1] = 4;
      }
      else if (temp > 30 || humid > 70) {
          analogWrite(FAN_LED, 192);  // 75%
          deviceLevels[1] = 3;
      }
      else if (temp > 28 || humid > 60) {
          analogWrite(FAN_LED, 128);  // 50%
          deviceLevels[1] = 2;
      }
      else if (temp > 25 || humid > 50) {
          analogWrite(FAN_LED, 64);   // 25%
          deviceLevels[1] = 1;
      }
      else {
          analogWrite(FAN_LED, 0);    // 0%
          deviceLevels[1] = 0;
      }
      
      // Điều khiển điều hòa dựa trên nhiệt độ
      if (temp > 32) {
          analogWrite(AC_LED, 255);   // 100%
          deviceLevels[2] = 4;
      }
      else if (temp > 30) {
          analogWrite(AC_LED, 192);   // 75%
          deviceLevels[2] = 3;
      }
      else if (temp > 28) {
          analogWrite(AC_LED, 128);   // 50%
          deviceLevels[2] = 2;
      }
      else if (temp > 25) {
          analogWrite(AC_LED, 64);    // 25%
          deviceLevels[2] = 1;
      }
      else {
          analogWrite(AC_LED, 0);     // 0%
          deviceLevels[2] = 0;
      }
    }
    
    // Gửi dữ liệu sang ESP32
    String data = "DATA:";
    data += "T:" + tempStr + ",";
    data += "H:" + humidStr + ",";
    data += "L:" + String(lightValue) + ",";
    data += "S:";
    data += String(deviceLevels[0]) + ",";
    data += String(deviceLevels[1]) + ",";
    data += String(deviceLevels[2]);
    Serial.println(data);
    
    delay(2000);
}