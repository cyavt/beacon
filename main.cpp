#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Địa chỉ MAC của các beacon
const char *beacon1 = "aa:bb:cc:dd:ee:01";
const char *beacon2 = "aa:bb:cc:dd:ee:02";
const char *beacon3 = "aa:bb:cc:dd:ee:03";

// Thời gian quét
int scanTime = 5; // giây
BLEScan *pBLEScan;

int rssi1 = -100; // RSSI giả định ban đầu, thấp hơn mức tín hiệu có thể có
int rssi2 = -100;
int rssi3 = -100;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    String address = advertisedDevice.getAddress().toString().c_str();
    int rssi = advertisedDevice.getRSSI();

    if (address.equals(beacon1))
    {
      rssi1 = rssi;
    }
    else if (address.equals(beacon2))
    {
      rssi2 = rssi;
    }
    else if (address.equals(beacon3))
    {
      rssi3 = rssi;
    }
  }
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Scanning...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); // Tạo đối tượng BLEScan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Active scan để có thêm thông tin từ thiết bị
}

void loop()
{
  rssi1 = -100;
  rssi2 = -100;
  rssi3 = -100;

  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);

  Serial.printf("Devices found: %d\n", foundDevices.getCount());
  Serial.printf("RSSI1: %d, RSSI2: %d, RSSI3: %d\n", rssi1, rssi2, rssi3);

  // So sánh RSSI để xác định phòng gần nhất
  if (rssi1 > rssi2 && rssi1 > rssi3)
  {
    Serial.println("Near Room 1");
  }
  else if (rssi2 > rssi1 && rssi2 > rssi3)
  {
    Serial.println("Near Room 2");
  }
  else if (rssi3 > rssi1 && rssi3 > rssi2)
  {
    Serial.println("Near Room 3");
  }
  else
  {
    Serial.println("Unable to determine the closest room");
  }

  pBLEScan->clearResults(); // Clear the results before the next scan to free memory
  delay(5000);             // Đợi 5 giây trước khi quét lại
}
