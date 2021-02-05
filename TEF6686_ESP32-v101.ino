/*
*************************************************
* Advanced Tuner software for NXP TEF668x ESP32 *
*  (c) 2021 Sjef Verhoeven pe5pvb@het-bar.net   *   
*   Based on Catena / NXP semiconductors API    *
*                   v 1.01                      *
*                                               *
*                                               *
* v1.00 - Release                               *
* v1.01 - Add interrupt on both Rotary pins     *
*         Add default settings                  *
*         Add Standby function                  *
*         Forced reboot after XDR-GTK disconnect*
*                                               *
*************************************************
*/
#include <Arduino.h>
#include "TEF6686.h"
#include "constants.h"
#include <EEPROM.h>
#include <Wire.h>
#include <SPI.h>
#include <analogWrite.h>      // https://github.com/ERROPiX/ESP32_AnalogWrite
#include <TFT_eSPI.h>         // https://github.com/Bodmer/TFT_eSPI
#include <RotaryEncoder.h>    // https://github.com/enjoyneering/RotaryEncoder

#define TFT_GREYOUT     0x38E7
#define ROTARY_PIN_A    34
#define ROTARY_PIN_B    36
#define ROTARY_BUTTON   39
#define PIN_POT         35
#define PWRBUTTON       4
#define BWBUTTON        25
#define MODEBUTTON      26
#define CONTRASTPIN     2

/* Use this settings in TFT_eSPI User_Setup.h
  #define TFT_CS          5
  #define TFT_DC          17
  #define TFT_RST         16
  #define SPI_FREQUENCY   55000000
*/

RotaryEncoder encoder(ROTARY_PIN_A, ROTARY_PIN_B, ROTARY_BUTTON);
int16_t position = 0;

void ICACHE_RAM_ATTR encoderISR(){encoder.readAB();}
void ICACHE_RAM_ATTR encoderButtonISR(){encoder.readPushButton();}

TFT_eSPI tft = TFT_eSPI(240, 320);

bool power = true;
bool BWreset;
bool menu;
bool menuopen = false;
bool RDSstatusold = false;
bool SQ;
bool Stereostatusold = false;
bool StereoToggle = true;
bool store;
bool tunemode = false;
bool USBstatus = false;
bool XDRMute;
byte BWset;
byte ContrastSet;
byte freqoldcount;
byte SStatusoldcount;
char buff[16];
char programServicePrevious[9];
char programTypePrevious[17];
char radioIdPrevious[4];
char radioTextPrevious[65];
int AGC;
int BWOld;
int ConverterSet;
int DeEmphasis;
int ForceMono;
int freqold;
int HighCutLevel;
int HighCutOffset;
int HighEdgeSet;
int LevelOffset;
int LowEdgeSet;
int menuoption = 30;
int MStatusold;
int OStatusold;
int peakholdold;
int peakholdtimer;
int scanner_filter;
int Sqstatusold;
int Squelch;
int SStatusold;
int StereoLevel;
int Stereostatus;
int VolSet;
int volume;
int XDRBWset;
int XDRBWsetold;
int16_t BW;
int16_t MStatus;
int16_t OStatus;
int16_t SStatus;
int16_t USN;
int16_t WAM;
String ContrastString;
String ConverterString;
String HighCutLevelString;
String HighCutOffsetString;
String HighEdgeString;
String LevelOffsetString;
String LowEdgeString;
String PIold;
String PSold;
String PTYold;
String RTold;
String StereoLevelString;
String VolString;
uint16_t rdsB;
uint16_t rdsC;
uint16_t rdsD;
uint16_t rdsErr;
uint8_t buff_pos = 0;
uint8_t RDSstatus;
unsigned int change;
unsigned int freq_scan;
unsigned int frequency;
unsigned int frequencyold;
unsigned int scanner_end;
unsigned int scanner_start;
unsigned int scanner_step;
unsigned long peakholdmillis;

TEF6686 radio;
RdsInfo rdsInfo;

void setup() {
  EEPROM.begin(40);
  if (EEPROM.readByte(39) != 1) {
    EEPROM.writeUInt(0, 10000);
    EEPROM.writeInt(2, 0);
    EEPROM.writeInt(6, 0);
    EEPROM.writeInt(10, 87);
    EEPROM.writeInt(14, 108);
    EEPROM.writeInt(18, 50);
    EEPROM.writeInt(22, 0);
    EEPROM.writeInt(26, 0);
    EEPROM.writeInt(30, 70);
    EEPROM.writeInt(34, 0);
    EEPROM.writeByte(39, 1);
  }
  frequency = EEPROM.readUInt(0);
  VolSet = EEPROM.readInt(2);
  ConverterSet = EEPROM.readInt(6);
  LowEdgeSet = EEPROM.readInt(10);
  HighEdgeSet = EEPROM.readInt(14);
  ContrastSet = EEPROM.readInt(18);
  LevelOffset = EEPROM.readInt(22);
  StereoLevel = EEPROM.readInt(26);
  HighCutLevel = EEPROM.readInt(30);
  HighCutOffset = EEPROM.readInt(34);

  encoder.begin();   
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A),  encoderISR,       CHANGE);
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B),  encoderISR,       CHANGE);
  attachInterrupt(digitalPinToInterrupt(ROTARY_BUTTON), encoderButtonISR, FALLING);
  pinMode(MODEBUTTON, INPUT);
  pinMode(BWBUTTON, INPUT);
  pinMode (CONTRASTPIN, OUTPUT);
  btStop();
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  radio.init();
  radio.setFrequency(frequency, LowEdgeSet, HighEdgeSet);
  radio.setVolume(VolSet);
  radio.setOffset(LevelOffset);
  radio.setStereoLevel(StereoLevel);
  radio.setHighCutLevel(HighCutLevel);
  radio.setHighCutOffset(HighCutOffset);
  radio.clearRDS();
  radio.setUnMute();
  radio.power(0);
  delay(100);
  BuildDisplay();
  analogWrite(CONTRASTPIN, ContrastSet*2+27);
}

void loop() {
  if (digitalRead(PWRBUTTON) == LOW && USBstatus == false) {PWRButtonPress();}
  if (power == true) {
    radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);
    if (menu == false) {
      ShowModLevel();
      readRds();
      showPI();
      showPTY();
      showPS();
      showRadioText();
      showPS();
      ShowFreq(0);
      ShowSignalLevel();
      ShowStereoStatus();
      ShowOffset();
      ShowBW();
    } 

    XDRGTKRoutine();
    doSquelch();

    if (encoder.getPushButton() == true) {ButtonPress();}

    if (menu == true && menuopen == true && menuoption == 110) {
      if (SStatus < 0) {SStatus = 0;}
      if (SStatus > SStatusold || SStatus < SStatusold)
      {
        String count = String(SStatus/10, DEC);
        if (count.length() != SStatusoldcount)
        {
          tft.setTextColor(TFT_BLACK, TFT_BLACK);
          if (SStatusoldcount <= 1) {tft.setCursor (120, 140);}
          if (SStatusoldcount == 2) {tft.setCursor (93, 140);}
          if (SStatusoldcount >= 3) {tft.setCursor (66, 140);}
          tft.setTextFont(6);
          tft.print(SStatusold/10);
          tft.setTextFont(4);
          tft.print(".");
          if (SStatusold < 0) 
          {
            String negative = String (SStatusold%10, DEC);
            if (SStatusold%10 == 0) {tft.print("0");} else {tft.print(negative[1]);}
          } else {
            tft.print(SStatusold%10);
          }
        }
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        if (count.length() == 1) {tft.setCursor (120, 140);}
        if (count.length() == 2) {tft.setCursor (93, 140);}
        if (count.length() == 3) {tft.setCursor (66, 140);}
        tft.setTextFont(6);
        tft.print(SStatus/10);
        tft.setTextFont(4);
        tft.print(".");
        if (SStatus < 0) 
        {
          String negative = String (SStatus%10, DEC);
          if (SStatus%10 == 0) {tft.print("0");} else {tft.print(negative[1]);}
        } else {
          tft.print(SStatus%10);
        }
        SStatusold = SStatus;
        SStatusoldcount = count.length();
      }
    }
  
    if (position < encoder.getPosition()) {KeyUp();}
  
    if (position > encoder.getPosition()) {KeyDown();}
  
    if (digitalRead(MODEBUTTON) == LOW) {ModeButtonPress();}
    if (digitalRead(BWBUTTON) == LOW) {BWset++;doBW();while (digitalRead(BWBUTTON) == LOW) {delay(10);}}
  
    if (store == true) {change++;}
    if (change > 100 && store == true) {
      EEPROM.writeUInt(0, frequency);
      EEPROM.commit();
      store = false;
    }
  }
}

void PWRButtonPress() {
  if (power == false) {
    ESP.restart();
  } else {
    power = false;
    analogWrite(CONTRASTPIN, 0);
    radio.power(1);
  }
  while (digitalRead(PWRBUTTON) == LOW) {delay(50);}
}

void ModeButtonPress() {
  if (menu == false) {
    unsigned long counterold = millis();
    unsigned long counter = 0;
    while (digitalRead(MODEBUTTON) == LOW) {
      counter = millis();
    }
    if (counter - counterold <= 1000) {
      doTuneMode();
    } else {
      if (StereoToggle == true) {
        tft.drawCircle(81,15,10,TFT_BLACK);
        tft.drawCircle(81,15,9,TFT_BLACK);
        tft.drawCircle(91,15,10,TFT_BLACK);
        tft.drawCircle(91,15,9,TFT_BLACK);
        tft.drawCircle(86,15,10,TFT_SKYBLUE);
        tft.drawCircle(86,15,9,TFT_SKYBLUE);
        radio.setMono(2);
        StereoToggle = false;
      } else {
        tft.drawCircle(86,15,10,TFT_BLACK);
        tft.drawCircle(86,15,9,TFT_BLACK);
        radio.setMono(0);
        Stereostatusold = false;
        StereoToggle = true;
      }
    }
  } else {
      OStatusold = 1000;
      Stereostatusold = false;
      SStatusold = 0;
      BWOld = 0;
      radio.clearRDS();
      RDSstatus = 0;
      BuildDisplay();
      menu = false;
      menuopen = false;
      EEPROM.writeInt(2, VolSet);
      EEPROM.writeInt(6, ConverterSet);
      EEPROM.writeInt(10, LowEdgeSet);
      EEPROM.writeInt(14, HighEdgeSet);
      EEPROM.writeInt(18, ContrastSet);
      EEPROM.writeInt(22, LevelOffset);
      EEPROM.writeInt(26, StereoLevel);
      EEPROM.writeInt(30, HighCutLevel);
      EEPROM.writeInt(34, HighCutOffset);
      EEPROM.commit();
    }
  while (digitalRead(MODEBUTTON) == LOW) {delay(50);}
}

void ButtonPress() {
  if (USBstatus == true) {
    ShowFreq(5);
    tft.setTextFont(4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor (70, 60);
    tft.print("NOT POSSIBLE");
    delay(1000);
    tft.setTextFont(4);
    tft.setTextColor(TFT_BLACK, TFT_BLACK);
    tft.setCursor (70, 60);
    tft.print("NOT POSSIBLE");
    ShowFreq(0);
  } else {
      if (menu == false) {
      BuildMenu();
      menu = true;
    } else {
      if (menuopen == false) {
        menuopen = true;
          tft.drawRoundRect(40,40,220,160,5,TFT_WHITE);
          tft.fillRoundRect(42,42,216,156,5,TFT_BLACK);
        switch (menuoption) {
          case 30:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Volume:", 150, 70, 4);
          tft.drawString("dB", 170, 110, 4);
          
          if (VolSet > 0) {VolString = (String)"+" + VolSet, DEC;} else {VolString = String(VolSet, DEC);}
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(VolString, 165, 110, 4);
          break;

          case 50:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Converter:", 150, 70, 4);
          tft.drawString("MHz", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          
          ConverterString = String(ConverterSet, DEC);
          tft.drawRightString(ConverterString, 165, 110, 4);
          break;

          case 70:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Low bandedge:", 150, 70, 4);
          tft.drawString("MHz", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          LowEdgeString = String(LowEdgeSet+ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          break;

          case 90:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("High bandedge:", 150, 70, 4);
          tft.drawString("MHz", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          HighEdgeString = String(HighEdgeSet+ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          break;

          case 110:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("RF Level offset:", 150, 70, 4);
          tft.drawString("dB", 170, 110, 4);
          
          if (LevelOffset > 0) {LevelOffsetString = (String)"+" + LevelOffset, DEC;} else {LevelOffsetString = String(LevelOffset, DEC);}
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          SStatusold = 200;
          break;

          case 130:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Stereo threshold:", 150, 70, 4);
          if (StereoLevel !=0) {tft.drawString("dBuV", 170, 110, 4);}
          tft.setTextColor(TFT_YELLOW);
          
          if (StereoLevel !=0) {StereoLevelString = String(StereoLevel, DEC);} else {StereoLevelString = String("Off");}
          tft.drawRightString(StereoLevelString, 165, 110, 4);
          break;

          case 150:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("High Cut corner:", 150, 70, 4);
          if (HighCutLevel !=0) {tft.drawString("kHz", 170, 110, 4);}
          tft.setTextColor(TFT_YELLOW);
          
          if (HighCutLevel !=0) {HighCutLevelString = String(HighCutLevel*100, DEC);} else {HighCutLevelString = String("Off");}
          tft.drawRightString(HighCutLevelString, 165, 110, 4);
          break;

          case 170:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Highcut threshold:", 150, 70, 4);
          if (HighCutOffset !=0) {tft.drawString("dBuV", 170, 110, 4);}
          tft.setTextColor(TFT_YELLOW);
          
          if (HighCutOffset !=0) {HighCutOffsetString = String(HighCutOffset, DEC);} else {HighCutOffsetString = String("Off");}
          tft.drawRightString(HighCutOffsetString, 165, 110, 4);
          break;

          case 190:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Contrast:", 150, 70, 4);
          tft.drawString("%", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          break;
        }
      } else {
        menuopen = false;
        BuildMenu();
      }
    }
  }
  while (digitalRead(ROTARY_BUTTON) == LOW) {delay(50);}
}

void KeyUp() {
    if (menu == false) {
      if (tunemode == true) {
        ShowFreq(1);
        radio.setMute();
        frequency = radio.seekUp(LowEdgeSet, HighEdgeSet, Squelch);
        ShowFreq(3);
        ShowFreq(0);
        radio.setUnMute();
      } else {
        frequency = radio.tuneUp(LowEdgeSet, HighEdgeSet);
        }
      if (USBstatus == true) {
        Serial.print("T");
        Serial.println(frequency*10);          
      }
      radio.clearRDS();
      change = 0;
      store = true;
    } else {
      if (menuopen == false) {
        tft.drawRoundRect(10,menuoption,300,18,5,TFT_BLACK);
        menuoption += 20;
        if (menuoption > 190) {menuoption = 30;}
        tft.drawRoundRect(10,menuoption,300,18,5,TFT_WHITE);
      } else {
        switch (menuoption) {
          case 30:
          tft.setTextColor(TFT_BLACK);
          if (VolSet > 0) {VolString = (String)"+" + VolSet, DEC;} else {VolString = String(VolSet, DEC);}
          tft.drawRightString(VolString, 165, 110, 4);
          VolSet++;
          if (VolSet > 10) {VolSet = 10;}
          tft.setTextColor(TFT_YELLOW);
          if (VolSet > 0) {VolString = (String)"+" + VolSet, DEC;} else {VolString = String(VolSet, DEC);}
          tft.drawRightString(VolString, 165, 110, 4);
          radio.setVolume(VolSet);
          break;

          case 50:
          tft.setTextColor(TFT_BLACK);
          ConverterString = String(ConverterSet, DEC);
          tft.drawRightString(ConverterString, 165, 110, 4);
          ConverterSet++;
          if (ConverterSet > 2400 || ConverterSet <= 200) {
            if (ConverterSet == 1) {
              ConverterSet = 200;
            } else {
              ConverterSet = 0;
            }
          }
          tft.setTextColor(TFT_YELLOW);
          ConverterString = String(ConverterSet, DEC);
          tft.drawRightString(ConverterString, 165, 110, 4);
          break;

          case 70:
          tft.setTextColor(TFT_BLACK);
          LowEdgeString = String(LowEdgeSet+ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          LowEdgeSet ++;
          if (LowEdgeSet > 107) {LowEdgeSet = 65;}
          tft.setTextColor(TFT_YELLOW);
          LowEdgeString = String(LowEdgeSet+ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          break;

          case 90:
          tft.setTextColor(TFT_BLACK);
          HighEdgeString = String(HighEdgeSet+ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          HighEdgeSet ++;
          if (HighEdgeSet > 110) {HighEdgeSet = 66;}
          tft.setTextColor(TFT_YELLOW);
          HighEdgeString = String(HighEdgeSet+ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          break;        

          case 110:
          tft.setTextColor(TFT_BLACK);
          if (LevelOffset > 0) {LevelOffsetString = (String)"+" + LevelOffset, DEC;} else {LevelOffsetString = String(LevelOffset, DEC);}
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          LevelOffset++;
          if (LevelOffset > 15) {LevelOffset = -48;}
          tft.setTextColor(TFT_YELLOW);
          if (LevelOffset > 0) {LevelOffsetString = (String)"+" + LevelOffset, DEC;} else {LevelOffsetString = String(LevelOffset, DEC);}
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          radio.setOffset(LevelOffset);
          break;

          case 130:
          tft.setTextColor(TFT_BLACK);
          StereoLevelString = String(StereoLevel, DEC);
          tft.drawRightString(StereoLevelString, 165, 110, 4);
          StereoLevel ++;
          if (StereoLevel > 60 || StereoLevel <= 30) {
            if (StereoLevel == 1) {
              StereoLevel = 30;
            } else {
              StereoLevel = 0;
            }
          }
          if (StereoLevel !=0) {StereoLevelString = String(StereoLevel, DEC);} else {StereoLevelString = String("Off");}
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (StereoLevel !=0) {tft.drawString("dBuV", 170, 110, 4);}
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(StereoLevelString, 165, 110, 4);
          radio.setStereoLevel(StereoLevel);
          break;              

          case 150:
          tft.setTextColor(TFT_BLACK);
          HighCutLevelString = String(HighCutLevel*100, DEC);
          tft.drawRightString(HighCutLevelString, 165, 110, 4);
          HighCutLevel ++;
          if (HighCutLevel > 70) {HighCutLevel = 15;}
          HighCutLevelString = String(HighCutLevel*100, DEC);
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(HighCutLevelString, 165, 110, 4);
          radio.setHighCutLevel(HighCutLevel);
          break;              

          case 170:
          tft.setTextColor(TFT_BLACK);
          HighCutOffsetString = String(HighCutOffset, DEC);
          tft.drawRightString(HighCutOffsetString, 165, 110, 4);
          HighCutOffset ++;
          if (HighCutOffset > 60 || HighCutOffset <= 20) {
            if (HighCutOffset == 1) {
              HighCutOffset = 20;
            } else {
              HighCutOffset = 0;
            }
          }
          if (HighCutOffset !=0) {HighCutOffsetString = String(HighCutOffset, DEC);} else {HighCutOffsetString = String("Off");}
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (HighCutOffset !=0) {tft.drawString("dBuV", 170, 110, 4);}
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(HighCutOffsetString, 165, 110, 4);
          radio.setHighCutOffset(HighCutOffset);
          break;      

          case 190:
          tft.setTextColor(TFT_BLACK);
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          ContrastSet ++;
          if (ContrastSet > 100) {ContrastSet = 1;}
          tft.setTextColor(TFT_YELLOW);
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          analogWrite(CONTRASTPIN, ContrastSet*2+27);
          break;        
        }
      }
    }
    while (digitalRead(ROTARY_PIN_A) == LOW || digitalRead(ROTARY_PIN_B) == LOW) {delay(5);}
    position = encoder.getPosition();
}

void KeyDown() {
    if (menu == false) {
      if (tunemode == true) {
        ShowFreq(2);
        radio.setMute();
        frequency = radio.seekDown(LowEdgeSet, HighEdgeSet, Squelch);
        ShowFreq(4);
        ShowFreq(0);
        radio.setUnMute();
      } else {
        frequency = radio.tuneDown(LowEdgeSet, HighEdgeSet);
      }
      if (USBstatus == true) {
        Serial.print("T");
        Serial.println(frequency*10);
      }
      radio.clearRDS();
      change = 0;
      store = true;
    } else {
      if (menuopen == false) {
        tft.drawRoundRect(10,menuoption,300,18,5,TFT_BLACK);
        menuoption -= 20;
        if (menuoption < 30) {menuoption = 190;}
        tft.drawRoundRect(10,menuoption,300,18,5,TFT_WHITE);
      } else {
        switch (menuoption) {
          case 30:
          
          tft.setTextColor(TFT_BLACK);
          if (VolSet > 0) {VolString = (String)"+" + VolSet, DEC;} else {VolString = String(VolSet, DEC);}
          tft.drawRightString(VolString, 165, 110, 4);
          VolSet--;
          if (VolSet < -10) {VolSet = -10;}
          tft.setTextColor(TFT_YELLOW);
          if (VolSet > 0) {VolString = (String)"+" + VolSet, DEC;} else {VolString = String(VolSet, DEC);}
          tft.drawRightString(VolString, 165, 110, 4);
          radio.setVolume(VolSet);
          break;

          case 50:
          tft.setTextColor(TFT_BLACK);
          ConverterString = String(ConverterSet, DEC);
          tft.drawRightString(ConverterString, 165, 110, 4);
          ConverterSet--;
          if (ConverterSet < 200) {
            if (ConverterSet < 0) {
              ConverterSet = 2400;
            } else {
              ConverterSet = 0;
            }
          }
          tft.setTextColor(TFT_YELLOW);
          ConverterString = String(ConverterSet, DEC);
          tft.drawRightString(ConverterString, 165, 110, 4);
          break;

          case 70:
          tft.setTextColor(TFT_BLACK);
          LowEdgeString = String(LowEdgeSet+ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          LowEdgeSet --;
          if (LowEdgeSet < 65) {LowEdgeSet = 107;}
          tft.setTextColor(TFT_YELLOW);
          LowEdgeString = String(LowEdgeSet+ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          break;

          case 90:
          tft.setTextColor(TFT_BLACK);
          HighEdgeString = String(HighEdgeSet+ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          HighEdgeSet --;
          if (HighEdgeSet < 66) {HighEdgeSet = 110;}
          tft.setTextColor(TFT_YELLOW);
          HighEdgeString = String(HighEdgeSet+ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          break;

          case 110:
          tft.setTextColor(TFT_BLACK);
          if (LevelOffset > 0) {LevelOffsetString = (String)"+" + LevelOffset, DEC;} else {LevelOffsetString = String(LevelOffset, DEC);}
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          LevelOffset--;
          if (LevelOffset < -48) {LevelOffset = 15;}
          tft.setTextColor(TFT_YELLOW);
          if (LevelOffset > 0) {LevelOffsetString = (String)"+" + LevelOffset, DEC;} else {LevelOffsetString = String(LevelOffset, DEC);}
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          radio.setOffset(LevelOffset);
          break;

          case 130:
          tft.setTextColor(TFT_BLACK);
          StereoLevelString = String(StereoLevel, DEC);
          tft.drawRightString(StereoLevelString, 165, 110, 4);
          StereoLevel --;
          if (StereoLevel < 30) {
            if (StereoLevel < 0) {
              StereoLevel = 60;
            } else {
              StereoLevel = 0;
            }
          }
          
          if (StereoLevel !=0) {StereoLevelString = String(StereoLevel, DEC);} else {StereoLevelString = String("Off");}
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (StereoLevel !=0) {tft.drawString("dBuV", 170, 110, 4);}
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(StereoLevelString, 165, 110, 4);
          radio.setStereoLevel(StereoLevel);
          break;    

          case 150:
          tft.setTextColor(TFT_BLACK);
          HighCutLevelString = String(HighCutLevel*100, DEC);
          tft.drawRightString(HighCutLevelString, 165, 110, 4);
          HighCutLevel --;
          if (HighCutLevel < 15) {HighCutLevel = 70;}
          HighCutLevelString = String(HighCutLevel*100, DEC);
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(HighCutLevelString, 165, 110, 4);
          radio.setHighCutLevel(HighCutLevel);
          break;    

          case 170:
          tft.setTextColor(TFT_BLACK);
          HighCutOffsetString = String(HighCutOffset, DEC);
          tft.drawRightString(HighCutOffsetString, 165, 110, 4);
          HighCutOffset --;
          if (HighCutOffset < 20) {
            if (HighCutOffset < 0) {
              HighCutOffset = 60;
            } else {
              HighCutOffset = 0;
            }
          }
          
          if (HighCutOffset !=0) {HighCutOffsetString = String(HighCutOffset, DEC);} else {HighCutOffsetString = String("Off");}
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (HighCutOffset !=0) {tft.drawString("dBuV", 170, 110, 4);}
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(HighCutOffsetString, 165, 110, 4);
          radio.setHighCutOffset(HighCutOffset);
          break;    

          case 190:
          tft.setTextColor(TFT_BLACK);
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          ContrastSet --;
          if (ContrastSet < 1) {ContrastSet = 100;}
          tft.setTextColor(TFT_YELLOW);
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          analogWrite(CONTRASTPIN, ContrastSet*2+27);
          break;        
        }  
      }
    }
    while (digitalRead(ROTARY_PIN_A) == LOW || digitalRead(ROTARY_PIN_B) == LOW) {delay(5);}
    position = encoder.getPosition();
}



void readRds() {
  radio.getRDS(&rdsInfo);
  RDSstatus = radio.readRDS(rdsB, rdsC, rdsD, rdsErr);
  ShowRDSLogo(RDSstatus);

  if (RDSstatus == 0) {
   tft.setTextColor(TFT_BLACK);
   tft.drawString(PIold, 244, 192, 4);
   tft.drawString(PSold, 38, 192, 4);
   tft.drawString(RTold, 6, 222, 2);
   tft.drawString(PTYold, 38, 168, 2);
   strcpy(programServicePrevious, " ");
   strcpy(radioIdPrevious, " ");
  }

  if (RDSstatus == 1 && USBstatus == true) {
    Serial.print("P");
    Serial.print(rdsInfo.programId);
    Serial.print("\nR");
    serial_hex(rdsB >> 8);
    serial_hex(rdsB);
    serial_hex(rdsC >> 8);
    serial_hex(rdsC);
    serial_hex(rdsD >> 8);
    serial_hex(rdsD);
    serial_hex(rdsErr >> 8);
    Serial.print("\n");
  }
}

void showPI() {
  if ((RDSstatus == 1) && !strcmp(rdsInfo.programId, radioIdPrevious, 4)) {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(PIold, 244, 192, 4);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(rdsInfo.programId, 244, 192, 4);
    PIold = rdsInfo.programId;
    strcpy(radioIdPrevious, rdsInfo.programId);
  }
}

void showPTY() {
  if ((RDSstatus == 1) && !strcmp(rdsInfo.programType, programTypePrevious, 16)) {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(PTYold, 38, 168, 2);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(rdsInfo.programType, 38, 168, 2);
    PTYold = rdsInfo.programType;
    strcpy(programTypePrevious, rdsInfo.programType);
  }
}

void showPS() {
  if ((RDSstatus == 1) && (strlen(rdsInfo.programService) == 8) && !strcmp(rdsInfo.programService, programServicePrevious, 8)) {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(PSold, 38, 192, 4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(rdsInfo.programService, 38, 192, 4);
    PSold = rdsInfo.programService;
    strcpy(programServicePrevious, rdsInfo.programService);
  }
}


void showRadioText() {
  if ((RDSstatus == 1) && !strcmp(rdsInfo.radioText, radioTextPrevious, 65)) {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(RTold, 6, 222, 2);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(rdsInfo.radioText, 6, 222, 2);
    RTold = rdsInfo.radioText;
    strcpy(radioTextPrevious, rdsInfo.radioText);
  }
}

bool strcmp(char* str1, char* str2, int length) {
  for (int i = 0; i < length; i++) {
    if (str1[i] != str2[i]) {
      return false;
    }
  }
  return true;
}

void BuildMenu() {
    tft.fillScreen(TFT_BLACK);  
    tft.drawRect(0, 0,320,240,TFT_BLUE);
    tft.drawLine(0,218,320,218,TFT_BLUE);
    tft.setTextColor(TFT_SKYBLUE);
    tft.drawCentreString("PRESS MODE TO EXIT AND STORE", 160, 222, 2);
    tft.drawCentreString("MENU",160,6,4);
    tft.drawRoundRect(10,menuoption,300,18,5,TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.drawRightString("dB", 305, 30, 2);
    tft.drawRightString("MHz", 305, 50, 2);
    tft.drawRightString("MHz", 305, 70, 2);
    tft.drawRightString("MHz", 305, 90, 2);
    tft.drawRightString("dB", 305,110,2);
    if (StereoLevel != 0) {tft.drawRightString("dBuV", 305, 130, 2);}
    if (HighCutLevel != 0) {tft.drawRightString("kHz", 305, 150, 2);}
    if (HighCutOffset != 0) {tft.drawRightString("dBuV", 305, 170, 2);}

    tft.drawRightString("%", 305, 190, 2);
    tft.drawString("Set volume",20,30,2);
    tft.drawString("Set converter offset",20,50,2);
    tft.drawString("Set low bandedge", 20, 70, 2);
    tft.drawString("Set high bandedge", 20, 90, 2);
    tft.drawString("Set level offset",20,110,2);
    tft.drawString("Set Stereo sep. threshold", 20,130,2);
    tft.drawString("Set high cut corner frequency", 20, 150,2);
    tft.drawString("Set High cut threshold",20,170,2);
    tft.drawString("Set Display brightness",20,190,2);
    
    if (VolSet > 0) {VolString = (String)"+" + VolSet, DEC;} else {VolString = String(VolSet, DEC);}
    ConverterString = String(ConverterSet, DEC);
    LowEdgeString = String(LowEdgeSet+ConverterSet, DEC);
    HighEdgeString = String(HighEdgeSet+ConverterSet, DEC);
    ContrastString = String(ContrastSet, DEC);
    
    if (StereoLevel !=0) {StereoLevelString = String(StereoLevel, DEC);} else {StereoLevelString = String("Off");}
    if (HighCutLevel !=0) {HighCutLevelString = String(HighCutLevel*100, DEC);} else {HighCutLevelString = String("Off");}
    if (LevelOffset > 0) {LevelOffsetString = (String)"+" + LevelOffset, DEC;} else {LevelOffsetString = String(LevelOffset, DEC);}
    if (HighCutOffset !=0) {HighCutOffsetString = String(HighCutOffset, DEC);} else {HighCutOffsetString = String("Off");}
    tft.setTextColor(TFT_YELLOW);
    tft.drawRightString(VolString, 270, 30, 2);
    tft.drawRightString(ConverterString, 270, 50, 2);
    tft.drawRightString(LowEdgeString, 270, 70, 2);
    tft.drawRightString(HighEdgeString, 270, 90, 2);
    tft.drawRightString(LevelOffsetString, 270, 110, 2);
    tft.drawRightString(StereoLevelString, 270, 130, 2);
    tft.drawRightString(HighCutLevelString, 270, 150, 2);
    tft.drawRightString(HighCutOffsetString, 270, 170, 2);
    tft.drawRightString(ContrastString, 270, 190, 2);
}

void BuildDisplay()
{
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0,320,240,TFT_BLUE);
    tft.drawLine(0,30,320,30,TFT_BLUE);
    tft.drawLine(0,100,320,100,TFT_BLUE);
    tft.drawLine(64,30,64,0,TFT_BLUE);
    tft.drawLine(210,100,210,218,TFT_BLUE);
    tft.drawLine(268,30,268,0,TFT_BLUE);
    tft.drawLine(0,165,210,165,TFT_BLUE);
    tft.drawLine(0,187,320,187,TFT_BLUE);
    tft.drawLine(0,218,320,218,TFT_BLUE);
    tft.drawLine(108,30,108,0,TFT_BLUE);
    tft.drawLine(174,30,174,0,TFT_BLUE);
    tft.drawLine(20,120,200,120,TFT_DARKGREY);
    tft.drawLine(20,150,200,150,TFT_DARKGREY);

    tft.setTextColor(TFT_WHITE);
    tft.drawString("S", 8, 108, 2);
    tft.drawString("M", 8, 138, 2);
    tft.drawString("PI:", 216, 195, 2);
    tft.drawString("PS:", 6, 195, 2);
    tft.drawString("PTY:", 6, 168, 2);
    tft.drawString("%", 196, 153, 1);
    tft.drawString("1", 20, 123, 1);
    tft.drawString("3", 50, 123, 1);
    tft.drawString("5", 80, 123, 1);
    tft.drawString("7", 110, 123, 1);
    tft.drawString("9", 140, 123, 1);
    tft.drawString("+20", 158, 123, 1);
    tft.drawString("+40", 182, 123, 1);
    tft.drawString("20", 20, 153, 1);
    tft.drawString("40", 50, 153, 1);
    tft.drawString("60", 80, 153, 1);
    tft.drawString("80", 110,153, 1);
    tft.drawString("100", 134,153, 1);
    tft.drawString("120", 164,153, 1);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("kHz", 225, 6, 4);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("MHz", 256, 64, 4);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("dBuV", 280, 155, 2);
    tft.drawPixel(295,166,TFT_WHITE);
    tft.drawPixel(295,167,TFT_WHITE);
    tft.drawPixel(295,168,TFT_WHITE);
    tft.drawPixel(295,169,TFT_WHITE);
    tft.drawPixel(295,170,TFT_WHITE);
    ShowFreq(0);
    updateTuneMode();
    updateBW();
    ShowUSBstatus();
}

void ShowFreq(int mode)
{
  unsigned int freq = radio.getFrequency() + ConverterSet*100;
  String count = String(freq/100, DEC);

  if (count.length() != freqoldcount || mode !=0)
  {
  tft.setTextColor(TFT_BLACK, TFT_BLACK);
  if (freqoldcount <= 2) {tft.setCursor (98, 45);}
  if (freqoldcount == 3) {tft.setCursor (71, 45);}
  if (freqoldcount == 4) {tft.setCursor (44, 45);}
  tft.setTextFont(7);
  tft.print(freqold/100);
  tft.print(".");
  if (freqold%100 < 10) {tft.print("0");}
  tft.print(freqold%100);
  }
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if (count.length() <= 2) {tft.setCursor (98, 45);}
  if (count.length() == 3) {tft.setCursor (71, 45);}
  if (count.length() == 4) {tft.setCursor (44, 45);}
  if (mode == 0) {
    tft.setTextFont(7);
    tft.print(freq/100);
    tft.print(".");
    if (freq%100 < 10) {tft.print("0");}
    tft.print(freq%100);
    freqold = freq;
    freqoldcount = count.length();
  } else if (mode == 1) {
    tft.setTextFont(4);
    tft.setCursor (98, 60);
    tft.print("SEEK UP");
  } else if (mode == 2) {
    tft.setTextFont(4);
    tft.setCursor (90, 60);
    tft.print("SEEK DOWN");
  } else if (mode == 3) {
    tft.setTextFont(4);
    tft.setTextColor(TFT_BLACK, TFT_BLACK);
    tft.setCursor (98, 60);
    tft.print("SEEK UP");
  } else if (mode == 4) {
    tft.setTextFont(4);
    tft.setTextColor(TFT_BLACK, TFT_BLACK);
    tft.setCursor (90, 60);
    tft.print("SEEK DOWN");
  } else if (mode == 5) {
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      if (freqoldcount <= 2) {tft.setCursor (98, 45);}
      if (freqoldcount == 3) {tft.setCursor (71, 45);}
      if (freqoldcount == 4) {tft.setCursor (44, 45);}
      tft.setTextFont(7);
      tft.print(freqold/100);
      tft.print(".");
      if (freqold%100 < 10) {tft.print("0");}
      tft.print(freqold%100);
  }
}

void ShowSignalLevel()
{
  if (SStatus < 0) {SStatus = 0;}
  if (SStatus > (SStatusold +20) || SStatus < (SStatusold -20))
  {
    String count = String(SStatus/10, DEC);

    if (count.length() != SStatusoldcount)
    {
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      if (SStatusoldcount <= 1) {tft.setCursor (267, 110);}
      if (SStatusoldcount == 2) {tft.setCursor (240, 110);}
      if (SStatusoldcount >= 3) {tft.setCursor (213, 110);}
      tft.setTextFont(6);
      tft.print(SStatusold/10);
      tft.setTextFont(4);
      tft.print(".");
      tft.print(SStatusold%10);
    }
  
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    if (count.length() == 1) {tft.setCursor (267, 110);}
    if (count.length() == 2) {tft.setCursor (240, 110);}
    if (count.length() == 3) {tft.setCursor (213, 110);}
    tft.setTextFont(6);
    tft.print(SStatus/10);
    tft.setTextFont(4);
    tft.print(".");
    tft.print(SStatus%10);
        
    if (SStatus/10 > 0) {tft.fillRect(20, 113,12,4,TFT_GREEN);} else {tft.fillRect(20, 113,12,4,TFT_GREYOUT);} 
    if (SStatus/10 > 6) {tft.fillRect(34, 113,12,4,TFT_GREEN);} else {tft.fillRect(34, 113,12,4,TFT_GREYOUT);} 
    if (SStatus/10 > 12) {tft.fillRect(48, 113,12,4,TFT_GREEN);} else {tft.fillRect(48, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 18) {tft.fillRect(62, 113,12,4,TFT_GREEN);} else {tft.fillRect(62, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 26) {tft.fillRect(76, 113,12,4,TFT_GREEN);} else {tft.fillRect(76, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 32) {tft.fillRect(90, 113,12,4,TFT_GREEN);} else {tft.fillRect(90, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 38) {tft.fillRect(104, 113,12,4,TFT_GREEN);} else {tft.fillRect(104, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 44) {tft.fillRect(118, 113,12,4,TFT_GREEN);} else {tft.fillRect(118, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 50) {tft.fillRect(132, 113,12,4,TFT_GREEN);} else {tft.fillRect(132, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 56) {tft.fillRect(146, 113,12,4,TFT_RED);} else {tft.fillRect(146, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 62) {tft.fillRect(160, 113,12,4,TFT_RED);} else {tft.fillRect(160, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 68) {tft.fillRect(174, 113,12,4,TFT_RED);} else {tft.fillRect(174, 113,12,4,TFT_GREYOUT);}
    if (SStatus/10 > 74) {tft.fillRect(188, 113,12,4,TFT_RED);} else {tft.fillRect(188, 113,12,4,TFT_GREYOUT);}
    SStatusold = SStatus;
    SStatusoldcount = count.length();
  }
}

void ShowRDSLogo(bool RDSstatus)
{
  if (RDSstatus != RDSstatusold)
  {
    if (RDSstatus == true)
    {
      tft.drawBitmap(110,5,RDSLogo,67,22,TFT_SKYBLUE);
    } else {
      tft.drawBitmap(110,5,RDSLogo,67,22,TFT_GREYOUT);
    }
    RDSstatusold = RDSstatus;
  }
}

void ShowStereoStatus()
{
  if (StereoToggle == true) 
  {
    Stereostatus = radio.getStereoStatus();
    if (Stereostatus != Stereostatusold)
    {
      if (Stereostatus == true)
    {
      tft.drawCircle(81,15,10,TFT_RED);
      tft.drawCircle(81,15,9,TFT_RED);
      tft.drawCircle(91,15,10,TFT_RED);
      tft.drawCircle(91,15,9,TFT_RED);
    } else {
      tft.drawCircle(81,15,10,TFT_GREYOUT);
      tft.drawCircle(81,15,9,TFT_GREYOUT);
      tft.drawCircle(91,15,10,TFT_GREYOUT);
      tft.drawCircle(91,15,9,TFT_GREYOUT);
    }
    Stereostatusold = Stereostatus;
    }
  }
}

void ShowOffset()
{
  if (OStatus != OStatusold) {
    if (OStatus < -500)
    {
      tft.fillTriangle(6,8,6,22,14,14,TFT_GREYOUT);
      tft.fillTriangle(18,8,18,22,26,14,TFT_GREYOUT);
      tft.fillCircle(32,15,3,TFT_GREYOUT);
      tft.fillTriangle(38,14,46,8,46,22,TFT_GREYOUT);
      tft.fillTriangle(50,14,58,8,58,22,TFT_RED);
    }
    if (OStatus < -250 && OStatus > -500)
    {
      tft.fillTriangle(6,8,6,22,14,14,TFT_GREYOUT);
      tft.fillTriangle(18,8,18,22,26,14,TFT_GREYOUT);
      tft.fillCircle(32,15,3,TFT_GREYOUT);
      tft.fillTriangle(38,14,46,8,46,22,TFT_RED);
      tft.fillTriangle(50,14,58,8,58,22,TFT_GREYOUT);
    }
    if (OStatus > -250 && OStatus < 250)
    {
      tft.fillTriangle(6,8,6,22,14,14,TFT_GREYOUT);
      tft.fillTriangle(18,8,18,22,26,14,TFT_GREYOUT);
      tft.fillCircle(32,15,3,TFT_GREEN);
      tft.fillTriangle(38,14,46,8,46,22,TFT_GREYOUT);
      tft.fillTriangle(50,14,58,8,58,22,TFT_GREYOUT);
    }
    if (OStatus > 250 && OStatus < 500)
    {
      tft.fillTriangle(6,8,6,22,14,14,TFT_GREYOUT);
      tft.fillTriangle(18,8,18,22,26,14,TFT_RED);
      tft.fillCircle(32,15,3,TFT_GREYOUT);
      tft.fillTriangle(38,14,46,8,46,22,TFT_GREYOUT);
      tft.fillTriangle(50,14,58,8,58,22,TFT_GREYOUT);
    }
    if (OStatus > 500)
    {
      tft.fillTriangle(6,8,6,22,14,14,TFT_RED);
      tft.fillTriangle(18,8,18,22,26,14,TFT_GREYOUT);
      tft.fillCircle(32,15,3,TFT_GREYOUT);
      tft.fillTriangle(38,14,46,8,46,22,TFT_GREYOUT);
      tft.fillTriangle(50,14,58,8,58,22,TFT_GREYOUT);
    }
    OStatusold = OStatus;
  }
}

void ShowBW()
{
  if (BW != BWOld || BWreset == true)
  {
    String BWString = String (BW, DEC);
    String BWOldString = String (BWOld, DEC);
    tft.setTextColor(TFT_BLACK);
    tft.drawRightString(BWOldString,218,6,4);
    if (BWset == 0) {
      tft.setTextColor(TFT_SKYBLUE);
    } else {
      tft.setTextColor(TFT_YELLOW);
    }
    tft.drawRightString(BWString,218,6,4);
    BWOld = BW;
    BWreset = false;
  }
}

void ShowModLevel()
{
  int hold;
  if (SQ == false) {} else {MStatus = 0; MStatusold = 1;}
  
  if (MStatus != MStatusold)
  {
    if (MStatus > 0) {tft.fillRect(20, 143,12,4,TFT_GREEN);hold=20;} else {tft.fillRect(20, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 10) {tft.fillRect(34, 143,12,4,TFT_GREEN);hold=34;} else {tft.fillRect(34, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 20) {tft.fillRect(48, 143,12,4,TFT_GREEN);hold=48;} else {tft.fillRect(48, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 30) {tft.fillRect(62, 143,12,4,TFT_GREEN);hold=62;} else {tft.fillRect(62, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 40) {tft.fillRect(76, 143,12,4,TFT_GREEN);hold=76;} else {tft.fillRect(76, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 50) {tft.fillRect(90, 143,12,4,TFT_GREEN);hold=90;} else {tft.fillRect(90, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 60) {tft.fillRect(104, 143,12,4,TFT_GREEN);hold=104;} else {tft.fillRect(104, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 70) {tft.fillRect(118, 143,12,4,TFT_GREEN);hold=118;} else {tft.fillRect(118, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 80) {tft.fillRect(132, 143,12,4,TFT_YELLOW);hold=132;} else {tft.fillRect(132, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 90) {tft.fillRect(146, 143,12,4,TFT_RED);hold=146;} else {tft.fillRect(146, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 100) {tft.fillRect(160, 143,12,4,TFT_RED);hold=160;} else {tft.fillRect(160, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 110) {tft.fillRect(174, 143,12,4,TFT_RED);hold=174;} else {tft.fillRect(174, 143,12,4,TFT_GREYOUT);}
    if (MStatus > 120) {tft.fillRect(188, 143,12,4,TFT_RED);hold=188;} else {tft.fillRect(188, 143,12,4,TFT_GREYOUT);}
    
    if (peakholdold < hold) {peakholdold = hold;}
    if (peakholdold < 132) {
      tft.fillRect(peakholdold, 143, 12, 4, TFT_GREEN);
    } else if (peakholdold == 132) {
      tft.fillRect(peakholdold, 143, 12, 4, TFT_YELLOW);
    } else {
      tft.fillRect(peakholdold, 143, 12, 4, TFT_RED);
    }

    if (peakholdmillis > peakholdtimer + 3000) {
      peakholdtimer += 3000;
      peakholdold = hold;
    }
    
    peakholdmillis = millis();
    MStatusold = MStatus;
   }
}


void doSquelch() {
  ShowSquelch();
  if (USBstatus == false) {
    Squelch = analogRead(PIN_POT)/4;
  }
  if (XDRMute == false) {
    if (Squelch != -1) {
      if (Squelch < SStatus || Squelch < 10) {
        radio.setUnMute();
        SQ = false;
      } else {
        radio.setMute();
        SQ = true;
      }
    } else {
      if (Stereostatus == true) {
        radio.setUnMute();
        SQ = false;
      } else {
        radio.setMute();
        SQ = true;
      }
    }
  }
}

void ShowSquelch() {
  if (menu == false) {
    if (SQ == false) {
        tft.drawRoundRect(3,79,40,20,5,TFT_GREYOUT);
        tft.setTextColor(TFT_GREYOUT);
        tft.drawCentreString("MUTE",24,81,2);
    } else {
      tft.drawRoundRect(3,79,40,20,5,TFT_WHITE);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("MUTE",24,81,2);
    }
  }
}


void updateBW() {
      if (BWset == 0) {
      tft.drawRoundRect(249,35,68,20,5,TFT_WHITE);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("AUTO BW",283,37,2);
      radio.setFMABandw();
    } else {
      tft.drawRoundRect(249,35,68,20,5,TFT_GREYOUT);
      tft.setTextColor(TFT_GREYOUT);
      tft.drawCentreString("AUTO BW",283,37,2);
    }
}

void doBW() {
    if (BWset > 16) {BWset = 0;}
    updateBW();
    BWreset = true;
    
    if (BWset == 1) {radio.setFMBandw(56);}
    if (BWset == 2) {radio.setFMBandw(64);}
    if (BWset == 3) {radio.setFMBandw(72);}
    if (BWset == 4) {radio.setFMBandw(84);}
    if (BWset == 5) {radio.setFMBandw(97);}
    if (BWset == 6) {radio.setFMBandw(114);}
    if (BWset == 7) {radio.setFMBandw(133);}
    if (BWset == 8) {radio.setFMBandw(151);}
    if (BWset == 9) {radio.setFMBandw(168);}
    if (BWset == 10) {radio.setFMBandw(184);}
    if (BWset == 11) {radio.setFMBandw(200);}
    if (BWset == 12) {radio.setFMBandw(217);}
    if (BWset == 13) {radio.setFMBandw(236);}
    if (BWset == 14) {radio.setFMBandw(254);}
    if (BWset == 15) {radio.setFMBandw(287);}
    if (BWset == 16) {radio.setFMBandw(311);}
}

void doTuneMode() {
  if (tunemode == true) {
    tunemode = false;
  } else {
    tunemode = true;
  }
  updateTuneMode();
}

void updateTuneMode() {
  if (tunemode == true) {
    tft.drawRoundRect(3,57,40,20,5,TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("AUTO",24,59,2);

    tft.drawRoundRect(3,35,40,20,5,TFT_GREYOUT);
    tft.setTextColor(TFT_GREYOUT);
    tft.drawCentreString("MAN",24,37,2);
  } else {
    tft.drawRoundRect(3,57,40,20,5,TFT_GREYOUT);
    tft.setTextColor(TFT_GREYOUT);
    tft.drawCentreString("AUTO",24,59,2);

    tft.drawRoundRect(3,35,40,20,5,TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("MAN",24,37,2);
  }
}
void ShowUSBstatus() {
  if (USBstatus == true) 
  {
    tft.drawBitmap(272,6,USBLogo,43,21,TFT_SKYBLUE);
  } else {
    tft.drawBitmap(272,6,USBLogo,43,21,TFT_GREYOUT);
  }
}

void XDRGTKRoutine() {
  if (Serial.available() > 0)
  {
    buff[buff_pos] = Serial.read();
    if (buff[buff_pos] != '\n' && buff_pos != 16 - 1) 
      buff_pos++;
     else {
      buff[buff_pos] = 0;
      buff_pos = 0;
      switch (buff[0])
      {
        case 'x':
          Serial.println("OK");
          Serial.print("T");
          Serial.print(frequency*10);
          Serial.print("A0\n");
          Serial.print("D0\n");
          Serial.print("G00\n");
          USBstatus = true;
          ShowUSBstatus();
          if (menu == true) {ModeButtonPress();}
        break;

        case 'A':
          AGC = atol(buff + 1);
          Serial.print("A");
          Serial.print(AGC);
          Serial.print("\n");
          radio.setAGC(AGC);
        break;

        case 'N':
        ModeButtonPress();
        break;
        
        case 'D':
          DeEmphasis = atol(buff + 1);
          Serial.print("D");
          Serial.print(DeEmphasis);
          Serial.print("\n");
          radio.setDeemphasis(DeEmphasis);
        break;

        case 'F':
          XDRBWset = atol(buff + 1);
          Serial.print("F");
          if (XDRBWset < 0) {
            XDRBWsetold = XDRBWset;
            BWset = 0;
          } else if (XDRBWset == 0) {
            BWset = 1;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 26) {
            BWset = 2;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 1) {
            BWset = 3;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 28) {
            BWset = 4;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 29) {
            BWset = 5;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 3) {
            BWset = 6;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 4) {
            BWset = 7;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 5) {
            BWset = 8;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 7) {
            BWset = 9;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 8) {
            BWset = 10;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 9) {
            BWset = 11;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 10) {
            BWset = 12;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 11) {
            BWset = 13;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 12) {
            BWset = 14;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 13) {
            BWset = 15;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 15) {
            BWset = 16;
            XDRBWsetold = XDRBWset;
          } else {
            XDRBWset = XDRBWsetold;
          }
          doBW();
          Serial.print(XDRBWset);
          Serial.print("\n");
        break;

        case 'G':
          LevelOffset =  atol(buff + 1);
          Serial.print("G");
          if (LevelOffset == 0) {
            radio.setOffset(0);
            Serial.print("00\n");
          }
          if (LevelOffset == 10) {
            radio.setOffset(6);
            Serial.print("10\n");
          }
          if (LevelOffset == 1) {
            radio.setOffset(6);
            Serial.print("01\n");
          }
          if (LevelOffset == 11) {
            radio.setOffset(12);
            Serial.print("11\n");
          }
          break;

        case 'T':
          frequency = atoi(buff +1);
          radio.setFrequency(frequency/10,65,110);
          Serial.print("T");
          Serial.print(frequency);
          Serial.print("\n");
          radio.clearRDS();
        break;

        case 'Q':
          Squelch = atoi(buff + 1);
          if (Squelch == -1) {
            Serial.print("Q-1\n");
          } else {
            Squelch *=10;
            Serial.print("Q\n");
            Serial.print(Squelch/10);
          }
        break;

        case 'S':
          if (buff[1] == 'a')
          {
            scanner_start = (atol(buff + 2) + 5) / 10;
          } else if (buff[1] == 'b')
          {
            scanner_end = (atol(buff + 2) + 5) / 10;
          } else if (buff[1] == 'c')
          {
            scanner_step = (atol(buff + 2) + 5) / 10;
          } else if (buff[1] == 'f')
          {
            scanner_filter = atol(buff + 2);
          } else if (scanner_start > 0 && scanner_end > 0 && scanner_step > 0 && scanner_filter >= 0)
          {
            frequencyold = radio.getFrequency();
            radio.setFrequency(scanner_start,65,110);
            Serial.print('U');
            if (scanner_filter < 0) {
            BWset = 0;
          } else if (scanner_filter == 0) {
            BWset = 1;
          } else if (scanner_filter == 26) {
            BWset = 2;
          } else if (scanner_filter == 1) {
            BWset = 3;
          } else if (scanner_filter == 28) {
            BWset = 4;
          } else if (scanner_filter == 29) {
            BWset = 5;
          } else if (scanner_filter == 3) {
            BWset = 6;
          } else if (scanner_filter == 4) {
            BWset = 7;
          } else if (scanner_filter == 5) {
            BWset = 8;
          } else if (scanner_filter == 7) {
            BWset = 9;
          } else if (scanner_filter == 8) {
            BWset = 10;
          } else if (scanner_filter == 9) {
            BWset = 11;
          } else if (scanner_filter == 10) {
            BWset = 12;
          } else if (scanner_filter == 11) {
            BWset = 13;
          } else if (scanner_filter == 12) {
            BWset = 14;
          } else if (scanner_filter == 13) {
            BWset = 15;
          } else if (scanner_filter == 15) {
            BWset = 16;
          }
          doBW();
          ShowFreq(5);
          tft.setTextFont(4);
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.setCursor (90, 60);
          tft.print("SCANNING...");
            for (freq_scan = scanner_start; freq_scan <= scanner_end; freq_scan += scanner_step)
            {
              radio.setFrequency(freq_scan,65,110);
              Serial.print(freq_scan*10, DEC);
              Serial.print('=');
              delay(10);
              radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);
              Serial.print((SStatus / 10)+10, DEC);
              Serial.print(',');
            }
            Serial.print('\n');
            tft.setTextFont(4);
            tft.setTextColor(TFT_BLACK, TFT_BLACK);
            tft.setCursor (90, 60);
            tft.print("SCANNING...");
            ShowFreq(0);
            radio.setFrequency(frequencyold,65,110);
            radio.setFMABandw();
          }
          break;

        case 'Y':
          VolSet = atoi(buff + 1);
          if (VolSet == 0) {
            radio.setMute();
            XDRMute = true;
            SQ = true;
          } else {
            radio.setVolume((VolSet-70)/10);
            XDRMute = false;
          }
          Serial.print("Y");
          Serial.print(VolSet);
          Serial.print("\n");
        break;

        case 'X':
          Serial.print("X\n");
          ESP.restart();
        break;
      }
    }
  }

  if (USBstatus == true) {
    Serial.print("S");
    if (StereoToggle == false) {
      Serial.print("S");
    } else if (Stereostatus == true) {
      Serial.print("s");
    } else {
      Serial.print("m");
    }
    Serial.print(((SStatus*100)+10875)/1000);
    Serial.print(',');
    Serial.print(WAM / 10, DEC);
    Serial.print(',');
    Serial.print(USN / 10, DEC);
    Serial.print("\n");
    }
}

void serial_hex(uint8_t val) {
  Serial.print((val >> 4) & 0xF, HEX);
  Serial.print(val & 0xF, HEX);
}
