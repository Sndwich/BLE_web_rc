/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-web-bluetooth/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>

BLEServer* pServer = NULL;
BLECharacteristic* pSensorCharacteristic = NULL;
BLECharacteristic* pLedCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

uint32_t motor_L = 0;

BLECharacteristic* pMotor_L = NULL;
BLECharacteristic* pMotor_R = NULL;
BLECharacteristic* pMotor_B = NULL;
BLECharacteristic* pDriveSpeed = NULL;
BLECharacteristic* pBrushSpeed = NULL;

// Define output pins
const int ledPin = 2;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SENSOR_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define LED_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"

#define MOTOR_L_ACTIVE_UUID "bbcf6bb0-30e6-41af-98d6-b6e8f033d2ec"
#define MOTOR_R_ACTIVE_UUID "bb52a3d0-5f6d-4eaa-a71c-318a38b35a0f"
#define MOTOR_B_ACTIVE_UUID "0690861e-9a4f-4806-824c-5f9394416bdf"
#define DRIVE_SPEED_UUID "04ae2894-cb3d-4669-b883-82e03085e236"
#define BRUSH_SPEED_UUID "bc42ed7b-00b7-4c00-81a3-0446fce0f3ba"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {

    BLEUUID uuid = pCharacteristic->getUUID();
    std::string value = pLedCharacteristic->getValue();

    if (value.length() > 0) {
      int receivedValue = static_cast<int>(value[0]);
      if (uuid.equals(BLEUUID(LED_CHARACTERISTIC_UUID))) {
        Serial.print("Characteristic event, written: ");
        Serial.println(static_cast<int>(value[0])); // Print the integer value
        if (receivedValue == 1) {
          digitalWrite(ledPin, HIGH);
        } else {
          digitalWrite(ledPin, LOW);
        }
      }
      if (uuid.equals(BLEUUID(MOTOR_L_ACTIVE_UUID))) {
        if (receivedValue == 1) {
          Serial.println("Motor L: ON");
        } else {
          Serial.println("Motor L: OFF");
        }
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // Create the BLE Device
  BLEDevice::init("Shroomba");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  CharacteristicCallbacks* myCharacteristicCallbacks = new CharacteristicCallbacks();
  
  // Create a BLE Characteristic
  pSensorCharacteristic = pService->createCharacteristic(
                      SENSOR_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // Create the ON button Characteristic
  pLedCharacteristic = pService->createCharacteristic(
                      LED_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  // Register the callback for the ON button characteristic
  pLedCharacteristic->setCallbacks(myCharacteristicCallbacks);

  // Create the MOTOR_L cahracteristic
  pMotor_L = pService->createCharacteristic(
                      MOTOR_L_ACTIVE_UUID,
                      BLECharacteristic::PROPERTY_WRITE_NR                    
                    );

  // Register the callback for the MOTOR_L characteristic
  pMotor_L->setCallbacks(myCharacteristicCallbacks);
  

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pSensorCharacteristic->addDescriptor(new BLE2902());
  pLedCharacteristic->addDescriptor(new BLE2902());
  pMotor_L->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  // notify changed value
  // if (deviceConnected) {
  //   pSensorCharacteristic->setValue(String(value).c_str());
  //   pSensorCharacteristic->notify();
  //   value++;
  //   Serial.print("New value notified: ");
  //   Serial.println(value);
  //   delay(3000); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
  // }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    Serial.println("Device disconnected.");
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
    Serial.println("Device Connected");
  }
}
