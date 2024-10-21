#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <Wire.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

#include <Adafruit_Sensor.h>

#define MQPIN 15   // Chân kết nối cảm biến khí gas MQ
#define LEDPIN 13  // Chân kết nối đèn LED

HardwareSerial SIM7670Serial(2);
String url = "http://iistem.com/data";

const char *beacon1 = "c0:df:81:17:71:62";  // Tầng 1, phòng 113 //becons nhỏ
const char *beacon2 = "d1:65:f0:83:c2:9d";  // Tầng 2, phòng 204 //beacons vừa
const char *beacon3 = "72:64:6c:13:00:8b";  // Tầng 2, phòng 202 //beacons lớn

int scanTime = 5;
int rssi1;
int rssi2;
int rssi3;

struct BLEScanResult {
  String room;
  String floorNumber;
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String address = advertisedDevice.getAddress().toString().c_str();

    if (address.equals(beacon1)) {
      rssi1 = advertisedDevice.getRSSI();
    } else if (address.equals(beacon2)) {
      rssi2 = advertisedDevice.getRSSI();
    } else if (address.equals(beacon3)) {
      rssi3 = advertisedDevice.getRSSI();
    }
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(MQPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);
  SIM7670Serial.begin(115200, SERIAL_8N1, 16, 17);
  Serial.println("Starting...");

  BLEDevice::init("");
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);

  while (!sendATCommand("AT", "OK", 1000)) {
    delay(4000);
    Serial.println("Đang kết nối module SIM...");
  }
  Serial.println("Module SIM đã sẵn sàng, thiết lập thành công !");
  sendATCommand("AT+CGDCONT=1,\"IP\",\"m-wap\"", "OK", 1000);  // Set APN for Mobi
  sendATCommand("AT+CGAUTH=1,1,\"mms\",\"mms\"", "OK", 1000);  // Set APN for Mobi
}

BLEScanResult bleScan() {
  BLEScan *pBLEScan = BLEDevice::getScan();
  BLEScanResults foundDevices = *pBLEScan->start(scanTime, false);  // Dereferencing the pointer
  Serial.printf("Devices found: %d\n", foundDevices.getCount());

  rssi1 = -100;
  rssi2 = -100;
  rssi3 = -100;

  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);
    String address = advertisedDevice.getAddress().toString().c_str();
    Serial.println(address);
    Serial.println(advertisedDevice.getName());

    if (address.equals(beacon1)) {
      rssi1 = advertisedDevice.getRSSI();
    } else if (address.equals(beacon2)) {
      rssi2 = advertisedDevice.getRSSI();
    } else if (address.equals(beacon3)) {
      rssi3 = advertisedDevice.getRSSI();
    }
  }

  BLEScanResult result;
  if (rssi1 > rssi2 && rssi1 > rssi3) {
    result.room = "113";
    result.floorNumber = "1";
  } else if (rssi2 > rssi1 && rssi2 > rssi3) {
    result.room = "203";
    result.floorNumber = "2";
  } else if (rssi3 > rssi1 && rssi3 > rssi2) {
    result.room = "204";
    result.floorNumber = "2";
  }

  return result;
}


void loop() {
  static unsigned long lastScanTime = 0;
  int gasValue;
  int user_status = 0;
  if (millis() - lastScanTime > 1000) {
    lastScanTime = millis();
    if (sendATCommand("AT", "OK", 1000)) {
      gasValue = analogRead(MQPIN);
      if (gasValue > 300) {
        user_status = 2;
        for (int i = 0; i < 20; i++) {
          digitalWrite(LEDPIN, LOW);
          delay(100);
          digitalWrite(LEDPIN, HIGH);
          delay(100);
        }
      } else {
        digitalWrite(LEDPIN, HIGH);
      }
      BLEScanResult scanResult = bleScan();
      Serial.println(scanResult.floorNumber);
      Serial.println(scanResult.room);
      Serial.println(gasValue);

      sendDataToServer(32.45, 83, gasValue, user_status, scanResult.floorNumber, scanResult.room);
    } else {
      Serial.println("Mất kết nối...");
    }
  }
}

void sendDataToServer(float temperature, float heartRate, float gasConcentration, int userStatus, String floorNumber, String room) {
  StaticJsonDocument<200> doc;
  doc["building_id"] = 1;
  doc["floor_number"] = floorNumber;
  doc["room_number"] = room;
  doc["temperature"] = temperature;
  doc["heart_rate"] = heartRate;
  doc["gas_concentration"] = gasConcentration;
  doc["user_status"] = userStatus;
  doc["uuid"] = "UUID-1234";

  String jsonData;
  serializeJson(doc, jsonData);

  sendATCommand("AT+CGATT=1", "OK", 1000);                     // Attach to GPRS service
  sendATCommand("AT+HTTPINIT", "OK", 1000);
  sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 1000);           // Set HTTP parameter URL
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 1000);  // Set HTTP parameter content type
  sendATCommand("AT+HTTPDATA=" + String(jsonData.length()) + ",10000", "DOWNLOAD", 1000);  // Set HTTP data length and timeout
  SIM7670Serial.print(jsonData);                                                           // Send JSON data
  delay(100);

  if (sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,200", 10000)) {  // Check if POST is successful
    Serial.println("Dữ liệu đã được gửi thành công.");
  } else {
    Serial.println("Gửi dữ liệu thất bại.");
  }

  // Read the response
  SIM7670Serial.print("AT+HTTPREAD\r");
  delay(1000);
  while (SIM7670Serial.available()) {
    char c = SIM7670Serial.read();
    Serial.print(c);
  }

  sendATCommand("AT+HTTPTERM", "OK", 1000);  // Terminate HTTP service
}

bool sendATCommand(String cmd, String expectedResponse, int timeout) {
  SIM7670Serial.println(cmd);
  String response = "";
  long timeStart = millis();
  while (millis() - timeStart < timeout) {
    while (SIM7670Serial.available() > 0) {
      char c = SIM7670Serial.read();
      response += c;
    }
    if (response.indexOf(expectedResponse) != -1) {
      Serial.println(response);
      return true;
    }
  }
  Serial.println(response);
  return false;
}
