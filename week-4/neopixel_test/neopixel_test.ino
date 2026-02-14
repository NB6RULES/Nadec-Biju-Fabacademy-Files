#include <Adafruit_NeoPixel.h>

#define PIN_NEOPIXEL 12  // Data pin
#define PIN_POWER    11  // Power pin (Enable)
#define NUM_PIXELS   1   // XIAO has 1 built-in NeoPixel

// Initialize the library
Adafruit_NeoPixel pixel(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

void setup() {
  pinMode(PIN_POWER, OUTPUT);
  digitalWrite(PIN_POWER, HIGH); // Power on the NeoPixel
  pixel.begin();
  pixel.setBrightness(55); // 0-255
}

void loop() {
  pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Red
  pixel.show();
  delay(100);

  pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Blue
  pixel.show();
  delay(100);
  
}