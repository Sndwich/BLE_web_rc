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

// Define pins
const int conLedPin = 2;

const int brushPWMPin = 23;
const int brushDirPin = 22;
const int rightPWMPin = 19;
const int rightDirPin = 18;
const int leftPWMPin = 17;
const int leftDirPin = 16;

uint8_t brushState = 0;
uint8_t brushSpeed = 0;
uint8_t moveDir    = 0;
uint8_t moveSpeed  = 0;

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
      
      brushState = rxValue[0];
      brushSpeed = rxValue[1];
      moveDir    = rxValue[2];
      moveSpeed  = rxValue[3];

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
  
  // Define pin modes
  pinMode(conLedPin, OUTPUT);
  pinMode(brushPWMPin, OUTPUT);
  pinMode(brushDirPin,OUTPUT);
  pinMode(rightPWMPin, OUTPUT);
  pinMode(rightDirPin, OUTPUT);
  pinMode(leftPWMPin, OUTPUT);
  pinMode(leftDirPin, OUTPUT);
  
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

void MotorKill(){
  // Force motor pins low
  analogWrite(leftPWMPin, 0);
  analogWrite(rightPWMPin, 0);
  analogWrite(brushPWMPin, 0);
  digitalWrite(conLedPin, LOW); // Turn off connection status LED
     
  // Force the global variables to 0
  brushState = 0;
  moveSpeed = 0;
  moveDir = 0;

  systemActive = false;
}

void loop() {
  // Safety Timeout Check
  if (deviceConnected && systemActive) {
    if (millis() - lastCommandTime > TIMEOUT_MS) {
      Serial.println("SAFETY TIMEOUT: Signal lost. Halting all motors.");
      MotorKill();
    }
  }

  // Handle Disconnection
  if (!deviceConnected && oldDeviceConnected) {
    Serial.println("Device disconnected. Halting all motors.");
    MotorKill();
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

  // Handle brush motor
  if (brushState == 1){
    analogWrite(brushPWMPin, brushSpeed); //Generate PWM signal respective to brushSpeed
  } else {
    analogWrite(brushPWMPin, 0); //Set brush motor pin to LOW
  }

  // Handle movement control
  // later add turn on spot mode that runs motors in opposite directions
  if (moveDir == 1){ // ↑
    // Both motors forwards
    digitalWrite(leftDirPin, HIGH);
    digitalWrite(rightDirPin, HIGH);
    analogWrite(leftPWMPin, moveSpeed);
    analogWrite(rightPWMPin, moveSpeed);
  } else if (moveDir == 2){ // ↓
    // Both motors reverse
    digitalWrite(leftDirPin, LOW);
    digitalWrite(rightDirPin, LOW);
    analogWrite(leftPWMPin, moveSpeed);
    analogWrite(rightPWMPin, moveSpeed);
  } else if (moveDir == 3){ // ←
    // R motor only
    digitalWrite(rightDirPin, HIGH);
    analogWrite(rightPWMPin, moveSpeed);
    analogWrite(leftPWMPin, 0); 
  } else if (moveDir == 4){ // →
    // L motor only
    digitalWrite(leftDirPin, HIGH);
    analogWrite(leftPWMPin, moveSpeed);
    analogWrite(rightPWMPin, 0);
  } else {
    // Set both motors to LOW
    analogWrite(leftPWMPin, 0);
    analogWrite(rightPWMPin, 0);
  }
}

