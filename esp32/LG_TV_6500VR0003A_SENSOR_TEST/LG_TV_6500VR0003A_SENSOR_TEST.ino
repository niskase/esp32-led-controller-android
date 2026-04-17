#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SENSOR_PIN 2  // Using 6500VR0003A sensor from LG TV
#define RELAY_PIN 10  // To avoid power peaks, use capacitors between VDD and GND
bool relayState = false;

const int numReadings = 50;     // How many measurements
int readings[numReadings];      // Measurement buffer
int readIndex = 0;              // INdex of the current measurement
long total = 0;                 // Sum for calculations
int history[SCREEN_WIDTH];      // Chart for the screen's curve 

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Wire.begin(8, 9);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }

  // Init average buffer and history screen with nulls
  for (int i = 0; i < numReadings; i++) readings[i] = 0;
  for (int i = 0; i < SCREEN_WIDTH; i++) history[i] = SCREEN_HEIGHT - 1;
}

void loop() {
  // Floating mean
  total = total - readings[readIndex];       // Subtract oldest measurement from sum
  readings[readIndex] = analogRead(SENSOR_PIN); // Read new measurement
  total = total + readings[readIndex];       // Add new measurement for sum
  readIndex = readIndex + 1;                 // Move to the next index

  if (readIndex >= numReadings) readIndex = 0; // Reutrn to the beginning if buffer is full
  
  int averageRaw = total / numReadings;      // Calculate mean

  // Scaling and history
  int y = map(averageRaw, 1500, 2300, SCREEN_HEIGHT - 1, 0);
  y = constrain(y, 0, SCREEN_HEIGHT - 1);

  for(int i = 0; i < SCREEN_WIDTH - 1; i++) {
    history[i] = history[i+1];
  }
  history[SCREEN_WIDTH - 1] = y;

  // Percent calculations for a relay
  int percentage = map(averageRaw, 1500, 2300, 0, 100);
  percentage = constrain(percentage, 0, 100);

  // Relay logics
  if (percentage <= 15 && !relayState) {
    digitalWrite(RELAY_PIN, HIGH);
    relayState = true;
  } 
  else if (percentage >= 15 && relayState) {
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;
  }

  // Screen drawings
  display.clearDisplay();
  
  // Curve
  for(int i = 1; i < SCREEN_WIDTH; i++) {
    display.drawLine(i-1, history[i-1], i, history[i], SSD1306_WHITE);
  }

  // Text
  display.setCursor(0,0);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.print(percentage);
  display.print("%  RELAY:");
  display.print(relayState ? "ON" : "OFF");

  display.display();
  
  delay(10); // Small delay that a loop runs about 100 times per second
}
