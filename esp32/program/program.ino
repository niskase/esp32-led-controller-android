#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Settings
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

const int TILT_PIN = 3;
const int BUZZER_PIN = 4;
const int RELAY_PIN = 5;
#define I2C_SDA 6 // I2C SDA PIN
#define I2C_SCL 7 // I2C SCL PIN
const int LED_PIN = 8; // ESP32-C3 built-in LED

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences prefs;
int secretPin;
bool isAuthenticated = false;
bool isLedOn = false;
bool isRelayOn = false;
bool isAlarmOn = false;
bool alarmTriggered = false;
int lastTiltState;
unsigned long lastBeepTime = 0;
bool buzzerState = false;
bool buzzerWasActive = false; 

unsigned long motionDetectedTime = 0;
bool waitingForConfirmation = false;

// Connection icon
void drawConnectionIcon() {
  if (isAuthenticated) {
    display.fillRect(118, 8, 2, 2, SSD1306_WHITE);
    display.fillRect(121, 6, 2, 4, SSD1306_WHITE);
    display.fillRect(124, 4, 2, 6, SSD1306_WHITE);
  } else {
    display.drawLine(118, 4, 126, 10, SSD1306_WHITE);
    display.drawLine(126, 4, 118, 10, SSD1306_WHITE);
  }
}

// Update screen
void updateDisplay(String title, String msg, int textSize = 2) {
  display.clearDisplay();
  drawConnectionIcon();
  controlPanel();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(title);
  display.setTextSize(textSize);
  display.setCursor(0, 15);
  display.println(msg);
  display.display();
}

// Server callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Device disconnected");
      isAuthenticated = false; 
      pServer->getAdvertising()->start();
      updateDisplay("Status", "Disconnected", 1);
    }
};

void controlPanel() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 40);
  isLedOn ? display.println("LED ON |") : display.println("LED OFF |");
  display.setCursor(50, 40);
  isRelayOn ? display.println("RELAY ON") : display.println("RELAY OFF");
  display.setCursor(0, 50);
  isAlarmOn ? display.println("ALARM ON") : display.println("ALARM OFF");
}

void playSuccessSound(String sound) {
  if (sound == "alarmon") {
    tone(BUZZER_PIN, 1000); delay(100);
    tone(BUZZER_PIN, 1500); delay(100);
    tone(BUZZER_PIN, 2000); delay(100);
    noTone(BUZZER_PIN);  
  } else if (sound == "alarmoff") {
    tone(BUZZER_PIN, 2000); delay(100);
    tone(BUZZER_PIN, 1500); delay(100);
    tone(BUZZER_PIN, 1000); delay(100);
    noTone(BUZZER_PIN);
  }
}

// Control panel callbacks
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();
      if (value.length() >= 6) {
        updateDisplay("Checking PIN-code...", value, 2);
        delay(1000);
        if (value.toInt() == secretPin) {
          isAuthenticated = true;
          updateDisplay("Checking PIN-code...", "OK!", 2);
          delay(1000);
          updateDisplay("Connected", "", 1);
        }
      } else if (isAuthenticated && value.length() == 1) {
        uint8_t cmd = value[0];
        if (cmd == 0) { digitalWrite(LED_PIN, HIGH); updateDisplay("LED", "OFF"); isLedOn = false; }
        else if (cmd == 1) { digitalWrite(LED_PIN, LOW); updateDisplay("LED", "ON"); isLedOn = true; }
        else if (cmd == 2) { digitalWrite(RELAY_PIN, LOW); updateDisplay("RELAY", "OFF"); isRelayOn = false; }
        else if (cmd == 3) { digitalWrite(RELAY_PIN, HIGH); updateDisplay("RELAY", "ON"); isRelayOn = true; }
        else if (cmd == 4) {
          playSuccessSound("alarmoff");
          updateDisplay("ALARM", "OFF");
          isAlarmOn = false; 
          if (alarmTriggered) {
            alarmTriggered = false;
            lastTiltState = digitalRead(TILT_PIN);
            updateDisplay("ALARM", "DISARMED");
          }
          
        }
        else if (cmd == 5) {
          playSuccessSound("alarmon");
          isAlarmOn = true;
          lastTiltState = digitalRead(TILT_PIN);
          updateDisplay("ALARM", "ON");  
        }
      }
      delay(1000);
      updateDisplay("Connected", "", 1);
    }
};

void setup() {
  
  Serial.begin(115200);

  // Define I2C pins and init 
  Wire.begin(6, 7); 

  // Init screen (0x3C is default)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed!"));
    // If failed, blink blue LED
    for(int i=0; i<5; i++) {
      digitalWrite(LED_PIN, LOW); delay(100); digitalWrite(LED_PIN, HIGH); delay(100);
    }
  }

  updateDisplay("Welcome", "Please wait...", 1);
  delay(500);
  
  // Load secret PIN code
  prefs.begin("secret", false);
  secretPin = prefs.getInt("saved_pin", 0);

  if (secretPin == 0) {
    randomSeed(analogRead(0)); // Get some noise from ADC pin 
    secretPin = random(10000, 99999);  
    Serial.println("--- SECRET CODE GENERATED ---");
    Serial.print("Code:");
    Serial.println(secretPin);
    Serial.println("----------------------------");
    prefs.putInt("saved_pin", secretPin); // SAVE
  } else {
    Serial.print("Use saved code from memory: ");
    Serial.println(secretPin);
  }

  prefs.end();

  // Show PIN code on screen
  updateDisplay("Connection code:", String(secretPin), 3);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TILT_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, HIGH); // When LED_PIN is high, LED is off
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  lastTiltState = digitalRead(TILT_PIN);
  
  BLEDevice::init("ESP32C3-LED-Control");
  
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void loop() {

  if (isAlarmOn && !alarmTriggered) {
    int currentTiltState = digitalRead(TILT_PIN);

    if (currentTiltState != lastTiltState && !waitingForConfirmation) {
      motionDetectedTime = millis();
      waitingForConfirmation = true;
    }
    
    if (waitingForConfirmation) {
      if (digitalRead(TILT_PIN) != lastTiltState) {
        if (millis() - motionDetectedTime > 150) {
          alarmTriggered = true;
          updateDisplay("ALARM!", "Motion detected", 1);
          waitingForConfirmation = false;
        }
      } else {
        waitingForConfirmation = false;
      }
    }
  }

  if (isAlarmOn && alarmTriggered) {
    unsigned long currentMillis = millis();

    buzzerWasActive = true;
    
    if ((currentMillis / 300) % 2 == 0) {
      tone(BUZZER_PIN, 3000);
      digitalWrite(RELAY_PIN, LOW);
    } else {
      tone(BUZZER_PIN, 2000);
      digitalWrite(RELAY_PIN, HIGH);
    }
  } else {
    if (buzzerWasActive) {
      noTone(BUZZER_PIN);
      buzzerWasActive = false;
    }
    
  }

  delay(10);
}
