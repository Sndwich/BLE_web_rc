#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>

BLEServer* pServer = NULL;
BLECharacteristic* pControlCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Hardware Safety Variables
unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 500;
bool systemActive = false; // Tracks if motors are currently running to prevent serial spam

#define SERVICE_UUID "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CONTROL_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"

const int conLedPin = 2;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class ControlCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() == 4) {
      // Update the timeout timer
      lastCommandTime = millis();
      
      uint8_t brushState = rxValue[0];
      uint8_t brushSpeed = rxValue[1];
      uint8_t moveDir    = rxValue[2];
      uint8_t moveSpeed  = rxValue[3];

      // Mark system as active if anything is moving/running
      systemActive = (brushState > 0 || moveDir > 0);

      Serial.printf("Brush: %d | BrushSpd: %3d | Dir: %d | MoveSpd: %3d\n", 
                    brushState, brushSpeed, moveDir, moveSpeed);
    }
  }
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("Shroomba");
  pinMode(conLedPin, OUTPUT);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pControlCharacteristic = pService->createCharacteristic(
                                        CONTROL_CHARACTERISTIC_UUID,
                                        BLECharacteristic::PROPERTY_WRITE | 
                                        BLECharacteristic::PROPERTY_WRITE_NR
                                      );

  pControlCharacteristic->setCallbacks(new ControlCallbacks());
  pControlCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection...");
}

void loop() {
  // Safety Timeout Check
  if (deviceConnected && systemActive) {
    if (millis() - lastCommandTime > TIMEOUT_MS) {
      Serial.println("SAFETY TIMEOUT: Signal lost. Halting all motors.");
      // Add logic here to force motor pins low
      // e.g., digitalWrite(motorPin, LOW);
      digitalWrite(conLedPin, LOW); // Turn off connection status LED
      
      systemActive = false;
    }
  }

  // Handle Disconnection
  if (!deviceConnected && oldDeviceConnected) {
    Serial.println("Device disconnected. Halting all motors.");
    // Add logic here to force motor pins low
    systemActive = false;
    digitalWrite(conLedPin, LOW); // Turn off connection status LED
    
    delay(500); 
    pServer->startAdvertising(); 
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }
  
  // Handle Connection
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    lastCommandTime = millis(); // Reset timer on fresh connection
    Serial.println("Device Connected");
    digitalWrite(conLedPin, HIGH); // Turn ON connection status LED
  }
}