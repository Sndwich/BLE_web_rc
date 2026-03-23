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
const int rightPWMPin = 5;
const int rightDirPin = 18;
const int leftPWMPin = 16;
const int leftDirPin = 17;

// --- LEDC PWM Config ---
const int freq = 5000;
const int pwmResolution = 8;

// Hardcode indpendent timer channels
const int rightPWMChannel = 2;
const int leftPWMChannel = 3;
const int brushPWMChannel = 4;

uint8_t brushState = 0;
uint8_t brushSpeed = 0;
uint8_t moveDir    = 0;
uint8_t moveSpeed  = 0;

uint8_t oldMoveDir = 0;

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
  pinMode(brushDirPin,OUTPUT);
  pinMode(rightDirPin, OUTPUT);
  pinMode(leftDirPin, OUTPUT);

  // Config LEDC PWM Timers
  ledcSetup(rightPWMChannel, freq, pwmResolution);
  ledcSetup(leftPWMChannel, freq, pwmResolution);
  ledcSetup(brushPWMChannel, freq, pwmResolution);

  // Attatch pins to the configured channels
  ledcAttachPin(rightPWMPin, rightPWMChannel);
  ledcAttachPin(leftPWMPin, leftPWMChannel);
  ledcAttachPin(brushPWMPin, brushPWMChannel);

  // Start motors off
  ledcWrite(rightPWMChannel, 0);
  ledcWrite(leftPWMChannel, 0);
  ledcWrite(brushPWMChannel, 0);
  
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
  ledcWrite(leftPWMChannel, 0);
  ledcWrite(rightPWMChannel, 0);
  ledcWrite(brushPWMChannel, 0);
  digitalWrite(conLedPin, LOW); // Turn off connection status LED
     
  // Force the global variables to 0
  brushState = 0;
  moveSpeed = 0;
  moveDir = 0;
  oldMoveDir =0;

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
    ledcWrite(brushPWMChannel, brushSpeed); //Generate PWM signal respective to brushSpeed
  } else {
    ledcWrite(brushPWMChannel, 0); //Set brush motor pin to LOW
  }

  if (moveDir != oldMoveDir){
    // Handle movement control
    // later add turn on spot mode that runs motors in opposite directions
    if (moveDir == 1){ // ↑
      // Both motors forwards
      digitalWrite(leftDirPin, HIGH);
      digitalWrite(rightDirPin, HIGH);
      ledcWrite(leftPWMChannel, moveSpeed);
      ledcWrite(rightPWMChannel, moveSpeed);
      Serial.println("FORWARDS");
    } else if (moveDir == 2){ // ↓
      // Both motors reverse
      digitalWrite(leftDirPin, LOW);
      digitalWrite(rightDirPin, LOW);
      ledcWrite(leftPWMChannel, moveSpeed);
      ledcWrite(rightPWMChannel, moveSpeed);
      Serial.println("BACKWARDS");
    } else if (moveDir == 3){ // ←
      // R motor only
      digitalWrite(rightDirPin, HIGH);
      ledcWrite(leftPWMChannel, 0);
      ledcWrite(rightPWMChannel, moveSpeed);
      Serial.println("LEFT"); 
    } else if (moveDir == 4){ // →
      // L motor only
      digitalWrite(leftDirPin, HIGH);
      ledcWrite(leftPWMChannel, moveSpeed);
      ledcWrite(rightPWMChannel, 0);
      Serial.println("RIGHT");
    } else {
      // Set both motors to LOW
      ledcWrite(leftPWMChannel, 0);
      ledcWrite(rightPWMChannel, 0);
      Serial.println("STOP");
    }
  }
  oldMoveDir = moveDir;
}
