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
void espMon (unsigned int loopMills);
void espInit (unsigned int loopMills);
String espSendBlocking (String command, unsigned int timeout);

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
const long blinkInterval = 1000;

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
const int repeatClick = 50;

double tempValue = 0;
const int tempInterval = 50;
unsigned long tempMills = 0;

static unsigned long espSent = 0;
static String espCmd = "";
static String espRsp = "";

static espState ESP_STATE = DISCONNECTED;

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
  pinMode(BUTTON_D, INPUT);
  pinMode(BUTTON_C, INPUT);


  //EEPROM
  Wire.begin();

  eepromTest();

  Serial.begin(115200);
}

String oldText =  "";
String newText =  "";

String serialRead = "";
String espDebug = "";

const unsigned int testAddr = 0x00A1;
byte eePromRead = 0x00;
 
//----------------------------------------
//  LOOP
//----------------------------------------

void loop() {
  unsigned long loopMills = millis();
  updateBlink(loopMills);
  updateButtons(loopMills);
  updateDisplay(loopMills);
  updateTemp(loopMills);
  espMon(loopMills);
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
  newText += "Blink STATE: ";
  newText += String(blinkLedState, DEC);
  newText += "\n";
  newText += "BUTTON_A: ";
  newText += String(aState, DEC);
  newText += " CK: ";
  newText += String(aClicks, DEC);
  newText += "\n";
  newText += "BUTTON_B: ";
  newText += String(bState, DEC);
  newText += " CK: ";
  newText += String(bClicks, DEC);
  newText += "\n";
  newText += "BUTTON_C: ";
  newText += String(cState, DEC);
  newText += " CK: ";
  newText += String(cClicks, DEC);
  newText += "\n";
  newText += "BUTTON_D: ";
  newText += String(dState, DEC);
  newText += " CK: ";
  newText += String(dClicks, DEC);

  newText += "\n";
  newText += "TEMP: ";
  newText += String(tempValue, 2);

  newText += "\n";
  newText += "EEPROM: ";
  newText += String(eePromRead, HEX);


  newText += "\n";
  newText += "ESPSTATE: ";
  newText += ESP_STATE;
  newText += " -- (";
  newText += Serial.available();
  newText += ")";
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
    espDebug = espSendBlocking("AT",1000);
  }
  //BUTTON_B
  if (bState && loopMills > bLastClick + repeatClick) {
    bClicks++;
    bLastClick = loopMills;
    espDebug = espSendBlocking("AT+GMR",1000);
  }
  //BUTTON_C
  if (cState && loopMills > cLastClick + repeatClick) {
    cClicks++;
    cLastClick = loopMills;
    espDebug = espSendBlocking("AT+RST",5000);
  }
  //BUTTON_D
  if (dState && loopMills > dLastClick + repeatClick) {
    dClicks++;
    dLastClick = loopMills;
    espDebug = "";
    while (Serial.available()) {
      espDebug += (char)Serial.read();
    }
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

void espMon (unsigned int loopMills) {
  switch (ESP_STATE) {
    case IDLE:
      // espDebug = "idle";
      break;
    case DELAY:
      // espDebug = "delay";
      break;
    case WAITING_LOCAL:
      // espDebug = "localWait";
      break;
    case WAITING_REMOTE:
      // espDebug = "remoteWait";
      break;
    case DISCONNECTED:
      // espDebug = "discon";
      break;
    case FAILED:
      // espDebug = "failed";
      break;
    case LOWPOWER:
      // espDebug = "lowPow";
      break;
  }
}

void espInit (unsigned int loopMills) {
  if (ESP_STATE != DISCONNECTED) {
    espDebug = "INIT_DUPP_RUNNING";
    return;
  }
  Serial.println("AT");

  while (!Serial.available() && millis() - loopMills < 1000) {
    delay(1);
  }
  if (Serial.available()) {
    espDebug = "";
    while (Serial.available()) {
      espDebug += char(Serial.read());
    }
    ESP_STATE = IDLE;
  } else {
    espDebug = "TIMEOUT";
  }
}

String espSendBlocking (String command, unsigned int timeout) {
  if (Serial.available()) {
    return "NOT_READY";
  }

  Serial.println(command);
  unsigned long sentTime = millis();
  String result = "";
  while (!Serial.available() && millis() - sentTime < timeout) {
    delay(10);
  }
  if (Serial.available()) {
    result = "";
    int line = 0;
    unsigned long lastRead = millis();
    while (Serial.available() || millis() - lastRead < timeout) {
      if (Serial.available()) {
        unsigned int lastRead = millis();
        char readChar = Serial.read();
        int readInt = (int)readChar;
        if (readInt >= 32 && readInt <= 126) {
          result += readChar;
        } else {
          result += ">";
          result += readInt;
          result += "<";
        }
      }
    }
  } else {
    ESP_STATE = TIMEOUT;
    result = "LOCAL_ERROR";
    result += "/";
    result += millis();
    result += "/";
    result += sentTime;
  }
  return result;
}

String espCommand (espCMD command) {
  String response = "";
  switch (command) {
    case AT:
      response = espSendBlocking("AT",1000);
      break;
    case AT_GMT:
      break;
  }
  return response;
}
