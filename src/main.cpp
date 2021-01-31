#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_ST7735.h>

//------------------------------------------------------
// ATmega328p pin connections
//------------------------------------------------------
// 1             Reset   | + | EEPROM_IC2 SCL  19/A5  28
// 2   0         WIFI RX |   | EEPROM_IC2 SDA  18/A4  27
// 3   1         WIFI TX |   | TEMP            17/A3  26
// 4   2/INT0   BUTTON_A |   | TFT_TCS         16/A2  25
// 5   3/INT1   BUTTON_B |   | TFT_RST         15/A1  24
// 6   4        BUTTON_C |   | TFT_DC          14/A0  23
// 7                 VCC |   | GND                    22
// 8                 GND |   | N/C                    21
// 9               XTAL1 |   | VCC                    20
// 10              XTAL2 |   | SCK             13     19
// 11  5        BUTTON_D |   | MISO            12     18
// 12  6                 |   | MOSI            11     17
// 13  7                 |   |                 10     16
// 14  8        blinkLED |   |                  9     15
//-------------------------------------------------------


//ESP
enum espState {
  IDLE,
  DELAY,
  WAITING_LOCAL,
  WAITING_REMOTE,
  DISCONNECTED,
  FAILED,
  LOWPOWER,
  TIMEOUT
};

enum espCMD {
  AT,
  AT_GMT
};

//----------------------------------------
// FUNCTIONS
//----------------------------------------
void updateDisplay(unsigned long loopMills);
void updateBlink (unsigned long loopMills);
void updateButtons(unsigned long loopMills);
void updateTemp(unsigned long loopMills);

void writeEEPROM(unsigned int address, byte data);
byte readEEPROM(unsigned int address);
void eepromTest ();

// ESP V2
boolean espMustBlock (unsigned long loopMills);
boolean espReadyToWrite (unsigned long loopMills);
boolean espWrite (unsigned long loopMills, String message);
boolean espFailureFlush (unsigned long loopMills);
boolean espFlushRead (unsigned long loopMills);
boolean espReadReady (unsigned long loopMills);
String espGetRead (unsigned long loopMills);


//----------------------------------------
// PIN DEFINITIONS
//----------------------------------------

//TFT display
#define TFT_TCS 16
#define TFT_RST 15
#define TFT_DC  14

//BUTTONS 
#define BUTTON_A 2
#define BUTTON_B 3
#define BUTTON_C 4
#define BUTTON_D 5

//LED blink LED
#define BLINK_LED 8

// Sesnsors
#define TEMP  A3

//EEPROM adress
#define eepromAddr 0x50  // 1010(A2)(A1)(A0) >> 1010000

int blinkLedState         = LOW;
unsigned long blinkMills  = 0;
const long blinkInterval = 50;

static int aState = LOW;
static int bState = LOW;
static int dState = LOW;
static int cState = LOW;

static unsigned int aClicks = 0;
static unsigned int bClicks = 0;
static unsigned int dClicks = 0;
static unsigned int cClicks = 0;
 
static unsigned long aLastClick = 0;
static unsigned long bLastClick = 0;
static unsigned long cLastClick = 0;
static unsigned long dLastClick = 0;
const int repeatClick = 200;

double tempValue = 0;
const int tempInterval = 50;
unsigned long tempMills = 0;

//ESP V2
static String espLastCommand = "";         //Last cmd sent
static unsigned long espReadTimeout = 0;   // When to bail on reading an incoming message
static unsigned long espTimeoutExpire = 0; // When a retry is allowed after a failed message
static String espReadBuffer = "";          //Partial message from Serial


//----------------------------------------
//  LIB Init
//----------------------------------------
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_TCS, TFT_DC, TFT_RST);

//----------------------------------------
//  SETUP
//----------------------------------------
void setup() {
  //TFT initalization
  tft.initR(INITR_144GREENTAB);
  tft.fillScreen(ST77XX_BLACK);

  // Blink Pin
  pinMode(BLINK_LED, OUTPUT);

  //Button
  pinMode(BUTTON_A, INPUT);
  pinMode(BUTTON_B, INPUT);
  pinMode(BUTTON_C, INPUT);
  pinMode(BUTTON_D, INPUT);


  //EEPROM
  Wire.begin();

  // eepromTest();

  Serial.begin(115200);
}

String oldText =  "";
String newText =  "";

String espDebug = "";

const unsigned int testAddr = 0x00A1;
byte eePromRead = 0x00;
 
//----------------------------------------
//  LOOP
//----------------------------------------

void loop() {
  unsigned long loopMills = millis();
  if (espMustBlock(loopMills)) {
    // ESP takes priority
    espFlushRead(loopMills);
  } else {
    updateBlink(loopMills);
    updateDisplay(loopMills);
    updateTemp(loopMills);
    // espMon(loopMills);
    espFlushRead(loopMills);
    updateButtons(loopMills);
  }
}

void updateBlink (unsigned long loopMills) {
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

void updateDisplay(unsigned long loopMills) {
  newText = "";
  // newText += "Blink STATE: ";
  // newText += String(blinkLedState, DEC);
  // newText += "\n";
  // newText += "BUTTON_A: ";
  // newText += String(aState, DEC);
  // newText += " CK: ";
  // newText += String(aClicks, DEC);
  // newText += "\n";
  // newText += "BUTTON_B: ";
  // newText += String(bState, DEC);
  // newText += " CK: ";
  // newText += String(bClicks, DEC);
  // newText += "\n";
  // newText += "BUTTON_C: ";
  // newText += String(cState, DEC);
  // newText += " CK: ";
  // newText += String(cClicks, DEC);
  // newText += "\n";
  // newText += "BUTTON_D: ";
  // newText += String(dState, DEC);
  // newText += " CK: ";
  // newText += String(dClicks, DEC);

  // newText += "\n";
  // newText += "TEMP: ";
  // newText += String(tempValue, 2);

  newText += "\n";
  newText += "EEPROM: ";
  newText += String(eePromRead, HEX);

  newText += "\n";
  newText += "ESP: ";
  newText += espDebug;

  if (newText != oldText) {
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_BLACK);
    tft.setTextWrap(true);
    tft.print(oldText);

    oldText = newText;
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextWrap(true);
    tft.print(newText);
  }
}

//----------------------
// BUTTONS
//----------------------
void updateButtons(unsigned long loopMills) {
  aState = digitalRead(BUTTON_A);
  bState = digitalRead(BUTTON_B);
  dState = digitalRead(BUTTON_D);
  cState = digitalRead(BUTTON_C);

  //BUTTON_A
  if (aState && loopMills > aLastClick + repeatClick) {
    aClicks++;
    aLastClick = loopMills;
    if (espWrite(loopMills,"AT")) {
      espDebug = "Wrote AT";
    } else {
      espDebug = "Write blocked\n";
      espDebug += "lc =";
      espDebug += espLastCommand;
      espDebug += "\n";
      espDebug += "rt =";
      espDebug += espReadTimeout;
      espDebug += "\n";
      espDebug += "te =";
      espDebug += espTimeoutExpire;
      espDebug += "\n";
      espDebug += "rb =";
      espDebug += espReadBuffer;
    }
  }
  //BUTTON_B
  if (bState && loopMills > bLastClick + repeatClick) {
    bClicks++;
    bLastClick = loopMills;
    if (espWrite(loopMills,"AT+GMR")) {
      espDebug = "Wrote AT+GMR";
    } else {
      espDebug = "Write blocked\n";
      espDebug += "lc =";
      espDebug += espLastCommand;
      espDebug += "\n";
      espDebug += "rt =";
      espDebug += espReadTimeout;
      espDebug += "\n";
      espDebug += "te =";
      espDebug += espTimeoutExpire;
      espDebug += "\n";
      espDebug += "rb =";
      espDebug += espReadBuffer;
    }
  }
  //BUTTON_C
  if (cState && loopMills > cLastClick + repeatClick) {
    cClicks++;
    cLastClick = loopMills;
    espDebug = espGetRead(loopMills);
  }
  //BUTTON_D
  if (dState && loopMills > dLastClick + repeatClick) {
    dClicks++;
    dLastClick = loopMills;

    espDebug = "";
    espDebug += "lc =";
    espDebug += espLastCommand;
    espDebug += "\n";
    espDebug += "rt =";
    espDebug += espReadTimeout;
    espDebug += "\n";
    espDebug += "te =";
    espDebug += espTimeoutExpire;
    espDebug += "\n";
    espDebug += "rb =";
    espDebug += espReadBuffer;
  }
}

void updateTemp(unsigned long loopMills) {
  if (loopMills > tempMills + tempInterval) {
    tempValue = (double)analogRead(TEMP)/1024;
    tempValue = tempValue * 3.3;
    tempValue = tempValue - 0.5;
    tempValue = tempValue * 100;
    tempMills = loopMills;
  }
}

void writeEEPROM(unsigned int address, byte data) {
  Wire.beginTransmission(eepromAddr);
  Wire.write((int)(address >> 8));   // MSB
  Wire.write((int)(address & 0xFF)); // LSB
  Wire.write(data);
  Wire.endTransmission();
  delay(5);
}

byte readEEPROM(unsigned int address) {
  byte rdata = 0xFF;
  Wire.beginTransmission(eepromAddr);
  Wire.write((int)(address >> 8));   // MSB
  Wire.write((int)(address & 0xFF)); // LSB
  Wire.endTransmission();

  Wire.requestFrom(eepromAddr,1);

  if (Wire.available()) rdata = Wire.read();
  return rdata;
}

void eepromTest () {
  writeEEPROM(testAddr,0xEE);
  eePromRead = readEEPROM(testAddr);
}

boolean espMustBlock (unsigned long loopMills) {
  if (espReadTimeout > loopMills) {
    return true;
  } else {
    return false;
  }
}

boolean espReadyToWrite (unsigned long loopMills) {
  if (espReadTimeout > loopMills) {
    //Still waiting to read
    return false;
  }
  if (espTimeoutExpire > loopMills) {
    //Still waiting for timout delay to expire
    return false;
  }

  if (espReadTimeout != 0 && espReadTimeout < loopMills) {
    //Failed read, need to clear.
    espFailureFlush(loopMills);
    return espReadyToWrite (loopMills);
  }

  if (espTimeoutExpire < loopMills) {
    // Clear out anything in the read buffer
    espFailureFlush(loopMills);
    espTimeoutExpire = 0;

    // All clear to try again
    return true;
  }
}

boolean espWrite (unsigned long loopMills, String message) {
  if (espReadyToWrite(loopMills)) {
    espLastCommand = message;
    Serial.println(message);
    espReadTimeout = loopMills + 2000;
    espTimeoutExpire = 0;
    return true;
  }
  return false;
}

boolean espFailureFlush (unsigned long loopMills) {
  espLastCommand = "";
  espReadTimeout = 0;
  espTimeoutExpire = loopMills + 1000;
  espReadBuffer = "";
  while (Serial.available()) {
    Serial.read();
  }
  return true;
}


boolean espFlushRead (unsigned long loopMills) {
  // TODO - count the lines
  if (espLastCommand != "") {
    if (Serial.available() > 50) {
      espReadBuffer += "!";
    }
    while (Serial.available()) {
      char readChar = Serial.read();
      int readInt = (int)readChar;
      if (readInt >= 32 && readInt <= 126) {
        espReadBuffer += readChar;
      } else {
        espReadBuffer += "{";
        espReadBuffer += readInt;
        espReadBuffer += "}";
      }
    }
  }
}

boolean espReadReady (unsigned long loopMills) {
  if (espLastCommand != "" && espReadTimeout < loopMills) {
    return true;
  }
  return false;
}

String espGetRead (unsigned long loopMills) {
  if (!espReadReady(loopMills)) {
    if (espLastCommand == "") {
      return "ERROR: No Command sent";
    } else {
      String result = "ERROR: read ready in ";
      result += espReadTimeout - loopMills;
      result += "seconds";
      return result;
    }
  }
  espTimeoutExpire = loopMills + 500;
  espLastCommand = "";
  return espReadBuffer;
}
