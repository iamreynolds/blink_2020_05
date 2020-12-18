#include <Arduino.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_ST7735.h>


//TFT display
#define TFT_TCS       16
#define TFT_RST       15
#define TFT_DC        14
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_TCS, TFT_DC, TFT_RST);

#define BLINK_LED     8

int blinkLedState         = LOW;
unsigned long blinkMills  = 0;
const long blinkInterval = 1000;


void setup() {
  //TFT initalization
  tft.initR(INITR_144GREENTAB);
  tft.fillScreen(ST7735_BLACK);
  

  // Blink Pin
  pinMode(BLINK_LED, OUTPUT);
}

void loop() {
  unsigned long loopMills = millis();

  tft.setCursor(0, 0);
  tft.setTextColor(0x07E0);
  tft.setTextWrap(true);
  tft.print("hello world" );


  if (loopMills - blinkMills >= blinkInterval) {
    blinkMills = loopMills;
    if (blinkLedState == LOW) {
      blinkLedState = HIGH;
    } else {
      blinkLedState = LOW;
    }
    digitalWrite(BLINK_LED, blinkLedState);
  }

 }
