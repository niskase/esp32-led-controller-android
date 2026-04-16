#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

float lastValidTemp = 0.0;
float lastValidHumi = 0.0;
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);
  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }

  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdate >= 3000) {
    lastUpdate = currentMillis;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      lastValidTemp = t;
      lastValidHumi = h;
      Serial.println("Data updated");
    } else {
      Serial.println("Data error");
    }

    updateDisplay();
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("ENVIRONMENT");
  display.drawLine(0, 10, 60, 10, SSD1306_WHITE);

  // Temperature
  display.setCursor(0, 18);
  display.print("Temperature:");
  display.setTextSize(2);
  display.setCursor(0, 28);
  display.print(lastValidTemp, 1);
  display.print(" C");

  // Humidity
  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print("Humidity: ");
  display.print(lastValidHumi, 0);
  display.print(" %");

  display.display();
}
