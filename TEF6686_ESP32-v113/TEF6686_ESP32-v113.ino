/*
********************************************************************************
  Advanced Tuner software for NXP TEF668x ESP32
   (c) 2021 Sjef Verhoeven pe5pvb@het-bar.net
    Based on Catena / NXP semiconductors API
********************************************************************************
  v1.00 - Release
  v1.01 - Add interrupt on both Rotary pins
        - Add default settings
        - Add Standby function
        - Forced reboot after XDR-GTK disconnect
  v1.02 - Fixed signalstrenght display when
          value is 0 after boot or isn't changed
          after menu close
        - Added stepsize option
        - Show "<" when signallevel is below
          0dBuV. Showing negative value is very
          unstable.
        - Cosmetic improvements
  v1.03 - When signallevel is < set value all
          statusses will only be updated
          every 0.5 seconds. Threshold can be
          set in the menu. RDS is then disabled
          This will also improve I2C and ADC
          noise on low level signals.
        - System can now show negative dBuV.
  v1.04 - Optimised TEF initialisation
        - Fix on peakhold modulation meter
        - Added Stereo squelch (turn knob right)
        - Added Squelch off (turn knob left)
        - Show current Squelch threshold
  v1.05 - Fixed bug: Patch was not loaded
          correctly into tuner after boot
        - In Low level mode, RDS logo disabled
        - Fix Stereo/mono toggle when using
          XDR-GTK
        - Fixed Squelch level readout when
          leaving menu. This could cause auto-
          scan to not work.
        - Fix: RDS logo was not disabled in low
          signal level mode
        - Changed advice for TFT frequency to
          reduce unwanted noise
        - Optimised soft-boot
  v1.06 - Slow down I2C speed to reduce noise
        - Fix auto scan when squelch level is
          below 0dBuV
        - Fix show Hz (not kHz) on high cut
          corner frequency
  v1.07 - Added multipath suppresion (iMS) and
          Channel Equalizer (EQ)
        - Long press rotary to toggle iMS and EQ
        - Added standby LED on D19
        - XDR-GTK: Use the antenna rotor buttons
          to start scan up or down
        - XDR-GTK: Enable IF+ to force low
          signal level mode
        - XDR-GTK: DX1 = Normal
                   DX2 = iMS enabled
                   DX3 = EQ enabled
                   DX4 = Both enabled
        - Small modifications on XDR-GTK routine
          for compatibility with modified
          XDR-GTK TEF6686 edition
        - Fixed small bug with displaytext in
          scan mode
  v1.08 - Fixed interrupted canvas lines on TFT
        - Added function to mute TFT using XDR-
          GTK. Use RF+ to use this function
        - Level offset is still as set when
          using XDR-GTK
        - Detach interrupts when writing
          frequency to display, to prevent
          display crash
        - Fixed bug when squelch is set to -1
        - Fixed bug that volume offset was reset
          to 0 after changing frequency
  v1.09 - Stereo indication didn't work when RF+
          was set in XDR-GTK
        - Minor fix: When for some reason the
          signallevel was an invalid value, now
          the limit is -10 to +120dBuV
        - No more short mute when changing frequency
        - tuned indication only on when signal
          valid.
        - In Auto mode frequency is now shown and
          direction can be changed on the fly.
          to stop searching, just press any key.
        - Minor bugfix in low signal lever timer.
  v1.10 - Stand-by function was bricked after last update.
        - Code optimised for compiler v2.0.0.
        - Startup frequency was always 100.0MHz. Fixed.
  v1.11 - Maximum frequency changed to 108MHz. (110MHz is not possible).
        - Added AM reception.
        - Hold BW button during boot to change direction of
          rotary encoder.
        - Hold Mode button during boot to flip the screen.
        - When RT was too long the canvas was overwritten.
  v1.12 - Stand-by key caused an issue when in menu mode.
        - Canvas was broken on some points in the screen.
        - Added possibility to connect analog signalmeter (thanks David DSP)
        - Added slow signalmeter riding (thanks David DSP)
        - S-meter for AM is corrected to the right values.
  v1.13 - Fix for frequency offset in AM.
        - Fix for flickering signalmeter when fluctuation is < 1dBuV.
        - Added support for David DSP's converter board.
********************************************************************************

  Analog signalmeter:
                       to meter
                          |
                R         V
  IO27 -------=====-----=====---|
               1 k     5 k POT

********************************************************************************
  Setup:
  During boot:
  Hold BW button to change rotary encoder direction
  Hold Mode button the flip screen
  Hold Standby button to calibrate analogue s-meter

  Manual:
  STANDBY SHORT PRESS: Switch band
  STANDBY LONG PRESS: Standby
  ROTARY SHORT PRESS: Set stepsize or navigate
  ROTARY LONG PRESS: Toggle iMS & EQ
  MODE SHORT PRESS: Switch between auto/manual
                    or exit menu
  MODE LONG PRESS: Open menu
  BW SHORT PRESS: Switch bandwidth setting
  BW LONG PRESS: Switch mono, or auto stereo
* ***********************************************


  Use this settings in TFT_eSPI User_Setup.h
  #define ILI9341_DRIVER
  #define TFT_CS          5
  #define TFT_DC          17
  #define TFT_RST         16
  #define LOAD_GLCD
  #define LOAD_FONT2
  #define LOAD_FONT4
  #define LOAD_FONT6
  #define LOAD_FONT7
  #define LOAD_FONT8
  #define LOAD_GFXFF
  #define SMOOTH_FONT
  #define SPI_FREQUENCY   10000000

  ALL OTHER SETTINGS SHOULD BE REMARKED!!!!

*/

#include "TEF6686.h"
#include "constants.h"
#include <EEPROM.h>
#include <Wire.h>
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
#define STANDBYLED      19
#define SMETERPIN       27
//#define ARS       // uncomment for BGR type display (ARS version)

#ifdef ARS
#define VERSION         "v1.13ARS"
#include "TFT_Colors.h"
TFT_eSPI tft = TFT_eSPI(320, 240);
#else
#define VERSION         "v1.13"
TFT_eSPI tft = TFT_eSPI(240, 320);
#endif

RotaryEncoder encoder(ROTARY_PIN_A, ROTARY_PIN_B, ROTARY_BUTTON);
int16_t position = 0;

void ICACHE_RAM_ATTR encoderISR() {
  encoder.readAB();
}
void ICACHE_RAM_ATTR encoderButtonISR() {
  encoder.readPushButton();
}

bool setupmode;
bool direction;
bool seek;
bool screenmute = false;
bool power = true;
bool change2;
bool BWreset;
bool menu;
bool menuopen = false;
bool LowLevelInit = false;
bool RDSstatusold;
bool SQ;
bool Stereostatusold;
bool StereoToggle = true;
bool store;
bool tunemode = false;
bool USBstatus = false;
bool XDRMute;
byte band;
byte BWset;
byte ContrastSet;
byte displayflip;
byte freqoldcount;
byte rotarymode;
byte SStatusoldcount;
byte stepsize;
byte LowLevelSet;
byte iMSset;
byte EQset;
byte iMSEQ;
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
int lowsignaltimer;
int scanner_filter;
int Sqstatusold;
int Squelch;
int Squelchold;
int SStatusold;
int StereoLevel;
int Stereostatus;
int VolSet;
int volume;
int XDRBWset;
int XDRBWsetold;
uint16_t BW;
uint16_t MStatus;
int16_t OStatus;
int16_t SStatus;
uint16_t USN;
uint16_t WAM;
String ContrastString;
String ConverterString;
String HighCutLevelString;
String HighCutOffsetString;
String HighEdgeString;
String LevelOffsetString;
String LowEdgeString;
String LowLevelString;
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
int16_t SAvg;
unsigned int change;
unsigned int freq_scan;
unsigned int frequency;
unsigned int frequency_AM;
unsigned int frequencyold;
unsigned int scanner_end;
unsigned int scanner_start;
unsigned int scanner_step;
unsigned long peakholdmillis;

TEF6686 radio;
RdsInfo rdsInfo;

void setup() {
  setupmode = true;
  EEPROM.begin(54);
  if (EEPROM.readByte(41) != 11) {
    EEPROM.writeByte(2, 0);
    EEPROM.writeByte(3, 0);
    EEPROM.writeUInt(0, 10000);
    EEPROM.writeInt(4, 0);
    EEPROM.writeInt(8, 0);
    EEPROM.writeInt(12, 87);
    EEPROM.writeInt(16, 108);
    EEPROM.writeInt(20, 50);
    EEPROM.writeInt(24, 0);
    EEPROM.writeInt(28, 0);
    EEPROM.writeInt(32, 70);
    EEPROM.writeInt(36, 0);
    EEPROM.writeByte(41, 11);
    EEPROM.writeByte(42, 0);
    EEPROM.writeByte(43, 20);
    EEPROM.writeByte(44, 1);
    EEPROM.writeByte(45, 1);
    EEPROM.writeByte(46, 0);
    EEPROM.writeUInt(47, 828);
    EEPROM.writeByte(52, 0);
    EEPROM.writeByte(53, 0);
    EEPROM.commit();
  }
  frequency = EEPROM.readUInt(0);
  VolSet = EEPROM.readInt(4);
  ConverterSet = EEPROM.readInt(8);
  LowEdgeSet = EEPROM.readInt(12);
  HighEdgeSet = EEPROM.readInt(16);
  ContrastSet = EEPROM.readInt(20);
  LevelOffset = EEPROM.readInt(24);
  StereoLevel = EEPROM.readInt(28);
  HighCutLevel = EEPROM.readInt(32);
  HighCutOffset = EEPROM.readInt(36);
  stepsize = EEPROM.readByte(42);
  LowLevelSet = EEPROM.readByte(43);
  iMSset = EEPROM.readByte(44);
  EQset = EEPROM.readByte(45);
  band = EEPROM.readByte(46);
  frequency_AM = EEPROM.readUInt(47);
  rotarymode = EEPROM.readByte(52);
  displayflip = EEPROM.readByte(53);
  EEPROM.commit();
  encoder.begin();
  btStop();
  Serial.begin(115200);

  if (iMSset == 1 && EQset == 1) {
    iMSEQ = 2;
  }
  if (iMSset == 0 && EQset == 1) {
    iMSEQ = 3;
  }
  if (iMSset == 1 && EQset == 0) {
    iMSEQ = 4;
  }
  if (iMSset == 0 && EQset == 0) {
    iMSEQ = 1;
  }
  tft.init();

  if (displayflip == 0) {
#ifdef ARS
    tft.setRotation(0);
#else
    tft.setRotation(3);
#endif
  } else {
#ifdef ARS
    tft.setRotation(2);
#else
    tft.setRotation(1);
#endif
  }

  radio.init();
  radio.setVolume(VolSet);
  radio.setOffset(LevelOffset);
  radio.setStereoLevel(StereoLevel);
  radio.setHighCutLevel(HighCutLevel);
  radio.setHighCutOffset(HighCutOffset);
  radio.clearRDS();
  radio.setMute();
  LowLevelInit = true;
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A),  encoderISR,       CHANGE);
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B),  encoderISR,       CHANGE);
  attachInterrupt(digitalPinToInterrupt(ROTARY_BUTTON), encoderButtonISR, FALLING);
  pinMode(MODEBUTTON, INPUT);
  pinMode(BWBUTTON, INPUT);
  pinMode (CONTRASTPIN, OUTPUT);
  pinMode(SMETERPIN, OUTPUT);
  analogWrite(CONTRASTPIN, ContrastSet * 2 + 27);
  analogWrite(SMETERPIN, 0);

  if (digitalRead(BWBUTTON) == LOW) {
    if (rotarymode == 0) {
      rotarymode = 1;
    } else {
      rotarymode = 0;
    }
    EEPROM.writeByte(52, rotarymode);
    EEPROM.commit();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Rotary direction changed", 150, 70, 4);
    tft.drawCentreString("Please release button", 150, 100, 4);
    while (digitalRead(BWBUTTON) == LOW) {
      delay(50);
    }
  }

  if (digitalRead(MODEBUTTON) == LOW) {
    if (displayflip == 0) {
      displayflip = 1;
      tft.setRotation(1);
    } else {
      displayflip = 0;
      tft.setRotation(3);
    }
    EEPROM.writeByte(53, displayflip);
    EEPROM.commit();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Screen flipped", 150, 70, 4);
    tft.drawCentreString("Please release button", 150, 100, 4);
    while (digitalRead(MODEBUTTON) == LOW) {
      delay(50);
    }
  }

  if (digitalRead(PWRBUTTON) == LOW) {
    analogWrite(SMETERPIN, 511);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Calibrate analog meter", 150, 70, 4);
    tft.drawCentreString("Release button when ready", 150, 100, 4);
    while (digitalRead(PWRBUTTON) == LOW) {
      delay(50);
    }
    analogWrite(SMETERPIN, 0);
  }

  if (ConverterSet >= 200) {
    Wire.beginTransmission(0x12);
    Wire.write(ConverterSet >> 8);
    Wire.write(ConverterSet & (0xFF));
    Wire.endTransmission();
  }

  SelectBand();
  ShowSignalLevel();
  ShowBW();
  setupmode = false;
}

void loop() {
  if (digitalRead(PWRBUTTON) == LOW && USBstatus == false) {
    PWRButtonPress();
  }

  if (power == true) {

    if (seek == true) {
      Seek(direction);
    }

    if (SStatus / 10 > LowLevelSet && LowLevelInit == false && menu == false && band == 0) {
      radio.clearRDS();
      if (screenmute == false) {
        tft.setTextColor(TFT_WHITE);
        tft.drawString("20", 20, 153, 1);
        tft.drawString("40", 50, 153, 1);
        tft.drawString("60", 80, 153, 1);
        tft.drawString("80", 110, 153, 1);
        tft.drawString("100", 134, 153, 1);
        tft.drawString("120", 164, 153, 1);
        tft.drawString("%", 196, 153, 1);
        tft.drawString("M", 8, 138, 2);
        tft.drawString("PI:", 216, 195, 2);
        tft.drawString("PS:", 6, 195, 2);
        tft.drawString("PTY:", 6, 168, 2);
        tft.drawLine(20, 150, 200, 150, TFT_DARKGREY);
      }
      LowLevelInit = true;
    }

    if (SStatus / 10 <= LowLevelSet && band == 0) {
      if (LowLevelInit == true && menu == false) {
        radio.clearRDS();
        if (screenmute == false) {
          tft.setTextColor(TFT_BLACK);
          tft.drawString(PSold, 38, 192, 4);
          tft.drawString(PIold, 244, 192, 4);
          tft.drawString(RTold, 6, 222, 2);
          tft.drawString(PTYold, 38, 168, 2);
          tft.fillRect(20, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(34, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(48, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(62, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(76, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(90, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(104, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(118, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(132, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(146, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(160, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(174, 143, 12, 4, TFT_GREYOUT);
          tft.fillRect(188, 143, 12, 4, TFT_GREYOUT);
          tft.setTextColor(TFT_GREYOUT);
          tft.drawString("20", 20, 153, 1);
          tft.drawString("40", 50, 153, 1);
          tft.drawString("60", 80, 153, 1);
          tft.drawString("80", 110, 153, 1);
          tft.drawString("100", 134, 153, 1);
          tft.drawString("120", 164, 153, 1);
          tft.drawString("%", 196, 153, 1);
          tft.drawString("M", 8, 138, 2);
          tft.drawString("PI:", 216, 195, 2);
          tft.drawString("PS:", 6, 195, 2);
          tft.drawString("PTY:", 6, 168, 2);
          tft.drawLine(20, 150, 200, 150, TFT_GREYOUT);
          tft.drawBitmap(110, 5, RDSLogo, 67, 22, TFT_GREYOUT);
        }
        LowLevelInit = false;
      }
      if (millis() >= lowsignaltimer + 300) {
        lowsignaltimer = millis();
        if (band == 0) {
          radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);
        } else {
          radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
        }
        if (screenmute == true) {
          readRds();
        }
        if (menu == false) {
          doSquelch();
        }
      }
    } else {
      if (band == 0) {
        radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);
      } else {
        radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
      }
      if (menu == false) {
        doSquelch();
        readRds();
        if (screenmute == false) {
          ShowModLevel();
        }
      }
    }

    if (menu == false) {
      if (screenmute == false) {
        showPI();
        showPTY();
        showPS();
        showRadioText();
        showPS();
        ShowStereoStatus();
        ShowOffset();
        ShowSignalLevel();
        ShowBW();
      }
    }
    XDRGTKRoutine();

    if (encoder.getPushButton() == true) {
      ButtonPress();
    }

    if (menu == true && menuopen == true && menuoption == 110)
    {
      if (band == 0) {
        radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);
      } else {
        radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
      }
      if (millis() >= lowsignaltimer + 500 || change2 == true) {
        lowsignaltimer = millis();
        change2 = false;
        if (SStatus > SStatusold || SStatus < SStatusold)
        {
          String count = String(SStatus / 10, DEC);
          if (screenmute == false) {
            tft.setTextColor(TFT_BLACK, TFT_BLACK);
            if (SStatusold >= 0) {
              if (SStatusoldcount <= 1) {
                tft.setCursor (100, 140);
              }
              if (SStatusoldcount == 2) {
                tft.setCursor (73, 140);
              }
              if (SStatusoldcount >= 3) {
                tft.setCursor (46, 140);
              }
            } else {
              if (SStatusoldcount <= 1) {
                tft.setCursor (100, 140);
              }
              if (SStatusoldcount == 2) {
                tft.setCursor (83, 140);
              }
              if (SStatusoldcount >= 3) {
                tft.setCursor (56, 140);
              }
            }
            tft.setTextFont(6);
            tft.print(SStatusold / 10);
            tft.print(".");
            if (SStatusold < 0)
            {
              String negative = String (SStatusold % 10, DEC);
              if (SStatusold % 10 == 0) {
                tft.print("0");
              } else {
                tft.print(negative[1]);
              }
            } else {
              tft.print(SStatusold % 10);
            }

            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            if (SStatus >= 0) {
              if (count.length() == 1) {
                tft.setCursor (100, 140);
              }
              if (count.length() == 2) {
                tft.setCursor (73, 140);
              }
              if (count.length() == 3) {
                tft.setCursor (46, 140);
              }
            } else {
              if (count.length() == 1) {
                tft.setCursor (100, 140);
              }
              if (count.length() == 2) {
                tft.setCursor (83, 140);
              }
              if (count.length() >= 3) {
                tft.setCursor (56, 140);
              }
            }
            tft.setTextFont(6);
            tft.print(SStatus / 10);
            tft.print(".");
            if (SStatus < 0)
            {
              String negative = String (SStatus % 10, DEC);
              if (SStatus % 10 == 0) {
                tft.print("0");
              } else {
                tft.print(negative[1]);
              }
            } else {
              tft.print(SStatus % 10);
            }
            SStatusold = SStatus;
            SStatusoldcount = count.length();
          }
        }
      }
    }
    if (position < encoder.getPosition()) {
      if (rotarymode == 0) {
        KeyUp();
      } else {
        KeyDown();
      }
    }

    if (position > encoder.getPosition()) {
      if (rotarymode == 0) {
        KeyDown();
      } else {
        KeyUp();
      }
    }

    if (digitalRead(MODEBUTTON) == LOW && screenmute == false) {
      ModeButtonPress();
    }
    if (digitalRead(BWBUTTON) == LOW && screenmute == false) {
      BWButtonPress();
    }

    if (store == true) {
      change++;
    }
    if (change > 200 && store == true) {
      detachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A));
      detachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B));
      detachInterrupt(digitalPinToInterrupt(ROTARY_BUTTON));
      EEPROM.writeUInt(0, radio.getFrequency());
      EEPROM.writeUInt(47, radio.getFrequency_AM());
      EEPROM.writeByte(46, band);
      EEPROM.commit();
      store = false;
      attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A),  encoderISR,       CHANGE);
      attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B),  encoderISR,       CHANGE);
      attachInterrupt(digitalPinToInterrupt(ROTARY_BUTTON), encoderButtonISR, FALLING);
    }
  }
}

void PWRButtonPress() {
  if (menu == false) {
    unsigned long counterold = millis();
    unsigned long counter = millis();
    while (digitalRead(PWRBUTTON) == LOW && counter - counterold <= 1000) {
      counter = millis();
    }
    if (counter - counterold < 1000) {
      if (power == false) {
        ESP.restart();
      } else {
        if (band == 0) {
          band = 1;
        } else {
          band = 0;
        }
        StoreFrequency();
        SelectBand();
      }
    } else {
      if (power == false) {
        ESP.restart();
      } else {
        power = false;
        analogWrite(SMETERPIN, 0);
        analogWrite(CONTRASTPIN, 0);
        pinMode (STANDBYLED, OUTPUT);
        digitalWrite(STANDBYLED, LOW);
        StoreFrequency();
        radio.power(1);
      }
    }
    while (digitalRead(PWRBUTTON) == LOW) {
      delay(50);
    }
    delay(100);
  }
}

void StoreFrequency() {
  EEPROM.writeUInt(0, radio.getFrequency());
  EEPROM.writeUInt(47, radio.getFrequency_AM());
  EEPROM.writeByte(46, band);
  EEPROM.commit();
}

void SelectBand() {
  if (band == 1) {
    seek = false;
    tunemode = false;
    BWreset = true;
    BWset = 2;
    if (radio.getFrequency_AM() > 0) {
      frequency_AM = radio.getFrequency_AM();
    }
    radio.setFrequency_AM(frequency_AM);
    freqold = frequency_AM;
    doBW;
    radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
    if (screenmute == false) {
      BuildDisplay();
    }
    tft.drawString("PI:", 216, 195, 2);
    tft.drawString("PS:", 6, 195, 2);
    tft.drawString("PTY:", 6, 168, 2);
    tft.drawBitmap(110, 5, RDSLogo, 67, 22, TFT_GREYOUT);
    tft.drawRoundRect(249, 56, 30, 20, 5, TFT_GREYOUT);
    tft.setTextColor(TFT_GREYOUT);
    tft.drawCentreString("iMS", 265, 58, 2);
    tft.drawRoundRect(287, 56, 30, 20, 5, TFT_GREYOUT);
    tft.setTextColor(TFT_GREYOUT);
    tft.drawCentreString("EQ", 303, 58, 2);
  } else {
    LowLevelInit == false;
    BWreset = true;
    BWset = 0;
    radio.power(0);
    delay(50);
    radio.setFrequency(frequency, LowEdgeSet, HighEdgeSet);
    freqold = frequency_AM;
    doBW;
    radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);
    if (screenmute == false) {
      BuildDisplay();
    }
  }
}

void BWButtonPress() {
  if (menu == false) {
    seek = false;
    unsigned long counterold = millis();
    unsigned long counter = millis();
    while (digitalRead(BWBUTTON) == LOW && counter - counterold <= 1000) {
      counter = millis();
    }
    if (counter - counterold < 1000) {
      BWset++;
      doBW();
    } else {
      doStereoToggle();
    }
  }
  while (digitalRead(BWBUTTON) == LOW) {
    delay(50);
  }
  delay(100);
}

void doStereoToggle() {
  if (StereoToggle == true) {
    if (screenmute == false) {
      tft.drawCircle(81, 15, 10, TFT_BLACK);
      tft.drawCircle(81, 15, 9, TFT_BLACK);
      tft.drawCircle(91, 15, 10, TFT_BLACK);
      tft.drawCircle(91, 15, 9, TFT_BLACK);
      tft.drawCircle(86, 15, 10, TFT_SKYBLUE);
      tft.drawCircle(86, 15, 9, TFT_SKYBLUE);
    }
    radio.setMono(2);
    StereoToggle = false;
  } else {
    if (screenmute == false) {
      tft.drawCircle(86, 15, 10, TFT_BLACK);
      tft.drawCircle(86, 15, 9, TFT_BLACK);
    }
    radio.setMono(0);
    Stereostatusold = false;
    StereoToggle = true;
  }
}

void ModeButtonPress() {
  if (menu == false) {
    seek = false;
    unsigned long counterold = millis();
    unsigned long counter = millis();
    while (digitalRead(MODEBUTTON) == LOW && counter - counterold <= 1000) {
      counter = millis();
    }
    if (counter - counterold <= 1000) {
      doTuneMode();
    } else {
      if (USBstatus == true) {
        ShowFreq(1);
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
        }
      }
    }
  } else {
    OStatusold = 1000;
    Stereostatusold = false;
    SStatusold = 2000;
    BWOld = 0;
    radio.clearRDS();
    RDSstatus = 0;
    BuildDisplay();
    ShowSignalLevel();
    ShowBW();
    menu = false;
    menuopen = false;
    LowLevelInit = true;
    EEPROM.writeInt(4, VolSet);
    EEPROM.writeInt(8, ConverterSet);
    EEPROM.writeInt(12, LowEdgeSet);
    EEPROM.writeInt(16, HighEdgeSet);
    EEPROM.writeInt(20, ContrastSet);
    EEPROM.writeInt(24, LevelOffset);
    EEPROM.writeInt(28, StereoLevel);
    EEPROM.writeInt(32, HighCutLevel);
    EEPROM.writeInt(36, HighCutOffset);
    EEPROM.writeByte(43, LowLevelSet);
    EEPROM.commit();
  }
  while (digitalRead(MODEBUTTON) == LOW) {
    delay(50);
  }
  delay(100);
}

void ShowStepSize() {
  tft.fillRect(224, 38, 15, 4, TFT_GREYOUT);
  tft.fillRect(193, 38, 15, 4, TFT_GREYOUT);
  if (band == 0) {
    tft.fillRect(148, 38, 15, 4, TFT_GREYOUT);
  } else {
    tft.fillRect(162, 38, 15, 4, TFT_GREYOUT);
  }
  if (stepsize == 1) {
    tft.fillRect(224, 38, 15, 4, TFT_GREEN);
  }
  if (stepsize == 2) {
    tft.fillRect(193, 38, 15, 4, TFT_GREEN);
  }
  if (stepsize == 3) {
    if (band == 0) {
      tft.fillRect(148, 38, 15, 4, TFT_GREEN);
    } else {
      tft.fillRect(162, 38, 15, 4, TFT_GREEN);
    }
  }
}

void RoundStep() {
  if (band == 0) {
    int freq = radio.getFrequency();
    if (freq % 10 < 3) {
      radio.setFrequency(freq - (freq % 10 - 5) - 5, LowEdgeSet, HighEdgeSet);
    }
    else if (freq % 10 > 2 && freq % 10 < 8) {
      radio.setFrequency(freq - (freq % 10 - 5) , LowEdgeSet, HighEdgeSet);
    }
    else if (freq % 10 > 7) {
      radio.setFrequency(freq - (freq % 10 - 5) + 5, LowEdgeSet, HighEdgeSet);
    }
  } else {
    int freq = radio.getFrequency_AM();
    if (freq < 2000) {
      radio.setFrequency_AM((freq / 9) * 9);
    } else {
      radio.setFrequency_AM((freq / 5) * 5);
    }
  }
  while (digitalRead(ROTARY_BUTTON) == LOW) {
    delay(50);
  }
  if (band == 0) {
    EEPROM.writeUInt(0, radio.getFrequency());
  } else {
    EEPROM.writeUInt(47, radio.getFrequency_AM());
  }
  EEPROM.commit();
}

void ButtonPress() {
  if (menu == false) {
    seek = false;
    unsigned long counterold = millis();
    unsigned long counter = millis();
    while (digitalRead(ROTARY_BUTTON) == LOW && counter - counterold <= 1000) {
      counter = millis();
    }
    if (counter - counterold < 1000) {
      if (tunemode == false) {
        stepsize++;
        if (stepsize > 3) {
          stepsize = 0;
        }
        if (screenmute == false) {
          ShowStepSize();
        }
        EEPROM.writeByte(42, stepsize);
        EEPROM.commit();
        if (stepsize == 0) {
          RoundStep();
          ShowFreq(0);
        }
      }
    } else {
      if (iMSEQ == 0) {
        iMSEQ = 1;
      }
      if (iMSEQ == 4) {
        iMSset = 0;
        EQset = 0;
        updateiMS();
        updateEQ();
        iMSEQ = 0;
      }
      if (iMSEQ == 3) {
        iMSset = 1;
        EQset = 0;
        updateiMS();
        updateEQ();
        iMSEQ = 4;
      }
      if (iMSEQ == 2) {
        iMSset = 0;
        EQset = 1;
        updateiMS();
        updateEQ();
        iMSEQ = 3;
      }
      if (iMSEQ == 1) {
        iMSset = 1;
        EQset = 1;
        updateiMS();
        updateEQ();
        iMSEQ = 2;
      }
      EEPROM.writeByte(44, iMSset);
      EEPROM.writeByte(45, EQset);
      EEPROM.commit();
    }
  } else {
    if (menuopen == false) {
      menuopen = true;
      tft.drawRoundRect(30, 40, 240, 160, 5, TFT_WHITE);
      tft.fillRoundRect(32, 42, 236, 156, 5, TFT_BLACK);
      switch (menuoption) {
        case 30:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Volume:", 150, 70, 4);
          tft.drawString("dB", 170, 110, 4);

          if (VolSet > 0) {
            VolString = (String)"+" + VolSet, DEC;
          } else {
            VolString = String(VolSet, DEC);
          }
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
          LowEdgeString = String(LowEdgeSet + ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          break;

        case 90:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("High bandedge:", 150, 70, 4);
          tft.drawString("MHz", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          HighEdgeString = String(HighEdgeSet + ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          break;

        case 110:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("RF Level offset:", 150, 70, 4);
          tft.drawString("dB", 170, 110, 4);
          tft.drawString("dBuV", 190, 157, 4);
          if (LevelOffset > 0) {
            LevelOffsetString = (String)"+" + LevelOffset, DEC;
          } else {
            LevelOffsetString = String(LevelOffset, DEC);
          }
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          SStatusold = 2000;
          change2 = true;
          break;

        case 130:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Stereo threshold:", 150, 70, 4);
          if (StereoLevel != 0) {
            tft.drawString("dBuV", 170, 110, 4);
          }
          tft.setTextColor(TFT_YELLOW);
          if (StereoLevel != 0) {
            StereoLevelString = String(StereoLevel, DEC);
          } else {
            StereoLevelString = String("Off");
          }
          tft.drawRightString(StereoLevelString, 165, 110, 4);
          break;

        case 150:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("High Cut corner:", 150, 70, 4);
          if (HighCutLevel != 0) {
            tft.drawString("Hz", 170, 110, 4);
          }
          tft.setTextColor(TFT_YELLOW);
          if (HighCutLevel != 0) {
            HighCutLevelString = String(HighCutLevel * 100, DEC);
          } else {
            HighCutLevelString = String("Off");
          }
          tft.drawRightString(HighCutLevelString, 165, 110, 4);
          break;

        case 170:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Highcut threshold:", 150, 70, 4);
          if (HighCutOffset != 0) {
            tft.drawString("dBuV", 170, 110, 4);
          }
          tft.setTextColor(TFT_YELLOW);
          if (HighCutOffset != 0) {
            HighCutOffsetString = String(HighCutOffset, DEC);
          } else {
            HighCutOffsetString = String("Off");
          }
          tft.drawRightString(HighCutOffsetString, 165, 110, 4);
          break;

        case 190:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Low level threshold:", 150, 70, 4);
          tft.drawString("dBuV", 150, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          LowLevelString = String(LowLevelSet, DEC);
          tft.drawRightString(LowLevelString, 145, 110, 4);
          break;

        case 210:
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
  while (digitalRead(ROTARY_BUTTON) == LOW) {
    delay(50);
  }
}

void KeyUp() {
  if (menu == false) {
    if (tunemode == true) {
      direction = true;
      seek = true;
      Seek(direction);
    } else {
      if (band == 0) {
        frequency = radio.tuneUp(stepsize, LowEdgeSet, HighEdgeSet);
      } else {
        frequency_AM = radio.tuneUp_AM(stepsize);
      }
    }
    if (USBstatus == true) {
      Serial.print("T");
      if (band == 0) {
        Serial.println(frequency * 10);
      } else {
        Serial.println(frequency_AM);
      }
    }
    radio.clearRDS();
    change = 0;
    ShowFreq(0);
    store = true;
  } else {
    if (menuopen == false) {
      tft.drawRoundRect(10, menuoption, 300, 18, 5, TFT_BLACK);
      menuoption += 20;
      if (menuoption > 210) {
        menuoption = 30;
      }
      tft.drawRoundRect(10, menuoption, 300, 18, 5, TFT_WHITE);
    } else {
      switch (menuoption) {
        case 30:
          tft.setTextColor(TFT_BLACK);
          if (VolSet > 0) {
            VolString = (String)"+" + VolSet, DEC;
          } else {
            VolString = String(VolSet, DEC);
          }
          tft.drawRightString(VolString, 165, 110, 4);
          VolSet++;
          if (VolSet > 10) {
            VolSet = 10;
          }
          tft.setTextColor(TFT_YELLOW);
          if (VolSet > 0) {
            VolString = (String)"+" + VolSet, DEC;
          } else {
            VolString = String(VolSet, DEC);
          }
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
          if (ConverterSet >= 200) {
            Wire.beginTransmission(0x12);
            Wire.write(ConverterSet >> 8);
            Wire.write(ConverterSet & (0xFF));
            Wire.endTransmission();
          }
          tft.setTextColor(TFT_YELLOW);
          ConverterString = String(ConverterSet, DEC);
          tft.drawRightString(ConverterString, 165, 110, 4);
          break;

        case 70:
          tft.setTextColor(TFT_BLACK);
          LowEdgeString = String(LowEdgeSet + ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          LowEdgeSet ++;
          if (LowEdgeSet > 107) {
            LowEdgeSet = 65;
          }
          tft.setTextColor(TFT_YELLOW);
          LowEdgeString = String(LowEdgeSet + ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          break;

        case 90:
          tft.setTextColor(TFT_BLACK);
          HighEdgeString = String(HighEdgeSet + ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          HighEdgeSet ++;
          if (HighEdgeSet > 108) {
            HighEdgeSet = 66;
          }
          tft.setTextColor(TFT_YELLOW);
          HighEdgeString = String(HighEdgeSet + ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          break;

        case 110:
          tft.setTextColor(TFT_BLACK);
          if (LevelOffset > 0) {
            LevelOffsetString = (String)"+" + LevelOffset, DEC;
          } else {
            LevelOffsetString = String(LevelOffset, DEC);
          }
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          LevelOffset++;
          if (LevelOffset > 15) {
            LevelOffset = -25;
          }
          tft.setTextColor(TFT_YELLOW);
          if (LevelOffset > 0) {
            LevelOffsetString = (String)"+" + LevelOffset, DEC;
          } else {
            LevelOffsetString = String(LevelOffset, DEC);
          }
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          radio.setOffset(LevelOffset);
          change2 = true;
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
          if (StereoLevel != 0) {
            StereoLevelString = String(StereoLevel, DEC);
          } else {
            StereoLevelString = String("Off");
          }
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (StereoLevel != 0) {
            tft.drawString("dBuV", 170, 110, 4);
          }
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(StereoLevelString, 165, 110, 4);
          radio.setStereoLevel(StereoLevel);
          break;

        case 150:
          tft.setTextColor(TFT_BLACK);
          HighCutLevelString = String(HighCutLevel * 100, DEC);
          tft.drawRightString(HighCutLevelString, 165, 110, 4);
          HighCutLevel ++;
          if (HighCutLevel > 70) {
            HighCutLevel = 15;
          }
          HighCutLevelString = String(HighCutLevel * 100, DEC);
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
          if (HighCutOffset != 0) {
            HighCutOffsetString = String(HighCutOffset, DEC);
          } else {
            HighCutOffsetString = String("Off");
          }
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (HighCutOffset != 0) {
            tft.drawString("dBuV", 170, 110, 4);
          }
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(HighCutOffsetString, 165, 110, 4);
          radio.setHighCutOffset(HighCutOffset);
          break;

        case 190:
          tft.setTextColor(TFT_BLACK);
          LowLevelString = String(LowLevelSet, DEC);
          tft.drawRightString(LowLevelString, 145, 110, 4);
          LowLevelSet++;
          if (LowLevelSet > 40 || LowLevelSet <= 0) {
            LowLevelSet = 0;
          }
          tft.setTextColor(TFT_YELLOW);
          LowLevelString = String(LowLevelSet, DEC);
          tft.drawRightString(LowLevelString, 145, 110, 4);
          break;

        case 210:
          tft.setTextColor(TFT_BLACK);
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          ContrastSet ++;
          if (ContrastSet > 100) {
            ContrastSet = 1;
          }
          tft.setTextColor(TFT_YELLOW);
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          analogWrite(CONTRASTPIN, ContrastSet * 2 + 27);
          break;

      }
    }
  }
  while (digitalRead(ROTARY_PIN_A) == LOW || digitalRead(ROTARY_PIN_B) == LOW) {
    delay(5);
  }
  position = encoder.getPosition();
}

void KeyDown() {
  if (menu == false) {
    if (tunemode == true) {
      direction = false;
      seek = true;
      Seek(direction);
    } else {
      if (band == 0) {
        frequency = radio.tuneDown(stepsize, LowEdgeSet, HighEdgeSet);
      } else {
        frequency_AM = radio.tuneDown_AM(stepsize);
      }
    }
    if (USBstatus == true) {
      Serial.print("T");
      if (band == 0) {
        Serial.println(frequency * 10);
      } else {
        Serial.println(frequency_AM);
      }
    }
    radio.clearRDS();
    change = 0;
    ShowFreq(0);
    store = true;
  } else {
    if (menuopen == false) {
      tft.drawRoundRect(10, menuoption, 300, 18, 5, TFT_BLACK);
      menuoption -= 20;
      if (menuoption < 30) {
        menuoption = 210;
      }
      tft.drawRoundRect(10, menuoption, 300, 18, 5, TFT_WHITE);
    } else {
      switch (menuoption) {
        case 30:

          tft.setTextColor(TFT_BLACK);
          if (VolSet > 0) {
            VolString = (String)"+" + VolSet, DEC;
          } else {
            VolString = String(VolSet, DEC);
          }
          tft.drawRightString(VolString, 165, 110, 4);
          VolSet--;
          if (VolSet < -10) {
            VolSet = -10;
          }
          tft.setTextColor(TFT_YELLOW);
          if (VolSet > 0) {
            VolString = (String)"+" + VolSet, DEC;
          } else {
            VolString = String(VolSet, DEC);
          }
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
          if (ConverterSet >= 200) {
            Wire.beginTransmission(0x12);
            Wire.write(ConverterSet >> 8);
            Wire.write(ConverterSet & (0xFF));
            Wire.endTransmission();
          }
          tft.setTextColor(TFT_YELLOW);
          ConverterString = String(ConverterSet, DEC);
          tft.drawRightString(ConverterString, 165, 110, 4);
          break;

        case 70:
          tft.setTextColor(TFT_BLACK);
          LowEdgeString = String(LowEdgeSet + ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          LowEdgeSet --;
          if (LowEdgeSet < 65) {
            LowEdgeSet = 107;
          }
          tft.setTextColor(TFT_YELLOW);
          LowEdgeString = String(LowEdgeSet + ConverterSet, DEC);
          tft.drawRightString(LowEdgeString, 165, 110, 4);
          break;

        case 90:
          tft.setTextColor(TFT_BLACK);
          HighEdgeString = String(HighEdgeSet + ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          HighEdgeSet --;
          if (HighEdgeSet < 66) {
            HighEdgeSet = 108;
          }
          tft.setTextColor(TFT_YELLOW);
          HighEdgeString = String(HighEdgeSet + ConverterSet, DEC);
          tft.drawRightString(HighEdgeString, 165, 110, 4);
          break;

        case 110:
          tft.setTextColor(TFT_BLACK);
          if (LevelOffset > 0) {
            LevelOffsetString = (String)"+" + LevelOffset, DEC;
          } else {
            LevelOffsetString = String(LevelOffset, DEC);
          }
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          LevelOffset--;
          if (LevelOffset < -25) {
            LevelOffset = 15;
          }
          tft.setTextColor(TFT_YELLOW);
          if (LevelOffset > 0) {
            LevelOffsetString = (String)"+" + LevelOffset, DEC;
          } else {
            LevelOffsetString = String(LevelOffset, DEC);
          }
          tft.drawRightString(LevelOffsetString, 165, 110, 4);
          radio.setOffset(LevelOffset);
          change2 = true;
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

          if (StereoLevel != 0) {
            StereoLevelString = String(StereoLevel, DEC);
          } else {
            StereoLevelString = String("Off");
          }
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (StereoLevel != 0) {
            tft.drawString("dBuV", 170, 110, 4);
          }
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(StereoLevelString, 165, 110, 4);
          radio.setStereoLevel(StereoLevel);
          break;

        case 150:
          tft.setTextColor(TFT_BLACK);
          HighCutLevelString = String(HighCutLevel * 100, DEC);
          tft.drawRightString(HighCutLevelString, 165, 110, 4);
          HighCutLevel --;
          if (HighCutLevel < 15) {
            HighCutLevel = 70;
          }
          HighCutLevelString = String(HighCutLevel * 100, DEC);
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

          if (HighCutOffset != 0) {
            HighCutOffsetString = String(HighCutOffset, DEC);
          } else {
            HighCutOffsetString = String("Off");
          }
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (HighCutOffset != 0) {
            tft.drawString("dBuV", 170, 110, 4);
          }
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(HighCutOffsetString, 165, 110, 4);
          radio.setHighCutOffset(HighCutOffset);
          break;

        case 190:
          tft.setTextColor(TFT_BLACK);
          LowLevelString = String(LowLevelSet, DEC);
          tft.drawRightString(LowLevelString, 145, 110, 4);
          LowLevelSet--;
          if (LowLevelSet > 40) {
            LowLevelSet = 40;
          }
          tft.setTextColor(TFT_YELLOW);
          LowLevelString = String(LowLevelSet, DEC);
          tft.drawRightString(LowLevelString, 145, 110, 4);
          break;


        case 210:
          tft.setTextColor(TFT_BLACK);
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          ContrastSet --;
          if (ContrastSet < 1) {
            ContrastSet = 100;
          }
          tft.setTextColor(TFT_YELLOW);
          ContrastString = String(ContrastSet, DEC);
          tft.drawRightString(ContrastString, 165, 110, 4);
          analogWrite(CONTRASTPIN, ContrastSet * 2 + 27);
          break;
      }
    }
  }
  while (digitalRead(ROTARY_PIN_A) == LOW || digitalRead(ROTARY_PIN_B) == LOW) {
    delay(5);
  }
  position = encoder.getPosition();
}



void readRds() {
  if (band == 0) {
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
  if (SStatus / 10 > LowLevelSet) {
    if ((RDSstatus == 1) && (strlen(rdsInfo.programService) == 8) && !strcmp(rdsInfo.programService, programServicePrevious, 8)) {
      tft.setTextColor(TFT_BLACK);
      tft.drawString(PSold, 38, 192, 4);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString(rdsInfo.programService, 38, 192, 4);
      PSold = rdsInfo.programService;
      strcpy(programServicePrevious, rdsInfo.programService);
    }
  }
}


void showRadioText() {
  if ((RDSstatus == 1) && !strcmp(rdsInfo.radioText, radioTextPrevious, 65)) {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(RTold, 6, 222, 2);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(rdsInfo.radioText, 6, 222, 2);
    tft.drawRect(0, 0, 320, 240, TFT_BLUE);
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
  tft.drawRect(0, 0, 320, 240, TFT_BLUE);
  tft.drawLine(0, 23, 320, 23, TFT_BLUE);
  tft.setTextColor(TFT_SKYBLUE);
  tft.drawString("PRESS MODE TO EXIT AND STORE", 20, 4, 2);
  tft.setTextColor(TFT_WHITE);
  tft.drawRightString(VERSION, 305, 4, 2);
  tft.drawRoundRect(10, menuoption, 300, 18, 5, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.drawRightString("dB", 305, 30, 2);
  tft.drawRightString("MHz", 305, 50, 2);
  tft.drawRightString("MHz", 305, 70, 2);
  tft.drawRightString("MHz", 305, 90, 2);
  tft.drawRightString("dB", 305, 110, 2);
  if (StereoLevel != 0) {
    tft.drawRightString("dBuV", 305, 130, 2);
  }
  if (HighCutLevel != 0) {
    tft.drawRightString("Hz", 305, 150, 2);
  }
  if (HighCutOffset != 0) {
    tft.drawRightString("dBuV", 305, 170, 2);
  }
  tft.drawRightString("dBuV", 305, 190, 2);
  tft.drawRightString("%", 305, 210, 2);
  tft.drawString("Set volume", 20, 30, 2);
  tft.drawString("Set converter offset", 20, 50, 2);
  tft.drawString("Set low bandedge", 20, 70, 2);
  tft.drawString("Set high bandedge", 20, 90, 2);
  tft.drawString("Set level offset", 20, 110, 2);
  tft.drawString("Set Stereo sep. threshold", 20, 130, 2);
  tft.drawString("Set high cut corner frequency", 20, 150, 2);
  tft.drawString("Set High cut threshold", 20, 170, 2);
  tft.drawString("Set low level threshold", 20, 190, 2);
  tft.drawString("Set Display brightness", 20, 210, 2);

  if (VolSet > 0) {
    VolString = (String)"+" + VolSet, DEC;
  } else {
    VolString = String(VolSet, DEC);
  }
  ConverterString = String(ConverterSet, DEC);
  LowEdgeString = String(LowEdgeSet + ConverterSet, DEC);
  HighEdgeString = String(HighEdgeSet + ConverterSet, DEC);
  LowLevelString = String(LowLevelSet, DEC);
  ContrastString = String(ContrastSet, DEC);

  if (StereoLevel != 0) {
    StereoLevelString = String(StereoLevel, DEC);
  } else {
    StereoLevelString = String("Off");
  }
  if (HighCutLevel != 0) {
    HighCutLevelString = String(HighCutLevel * 100, DEC);
  } else {
    HighCutLevelString = String("Off");
  }
  if (LevelOffset > 0) {
    LevelOffsetString = (String)"+" + LevelOffset, DEC;
  } else {
    LevelOffsetString = String(LevelOffset, DEC);
  }
  if (HighCutOffset != 0) {
    HighCutOffsetString = String(HighCutOffset, DEC);
  } else {
    HighCutOffsetString = String("Off");
  }
  tft.setTextColor(TFT_YELLOW);
  tft.drawRightString(VolString, 270, 30, 2);
  tft.drawRightString(ConverterString, 270, 50, 2);
  tft.drawRightString(LowEdgeString, 270, 70, 2);
  tft.drawRightString(HighEdgeString, 270, 90, 2);
  tft.drawRightString(LevelOffsetString, 270, 110, 2);
  tft.drawRightString(StereoLevelString, 270, 130, 2);
  tft.drawRightString(HighCutLevelString, 270, 150, 2);
  tft.drawRightString(HighCutOffsetString, 270, 170, 2);
  tft.drawRightString(LowLevelString, 270, 190, 2);
  tft.drawRightString(ContrastString, 270, 210, 2);
  analogWrite(SMETERPIN, 0);
}

void MuteScreen(int setting)
{
  if (setting == 0) {
    screenmute = 0;
    setupmode = true;
    BuildDisplay();
    setupmode = false;
  } else {
    screenmute = 1;
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Screen is muted!", 160, 30, 4);
    tft.drawCentreString("To unmute uncheck RF+ box", 160, 60, 2);
  }
}

void BuildDisplay()
{
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, 320, 240, TFT_BLUE);
  tft.drawLine(0, 30, 320, 30, TFT_BLUE);
  tft.drawLine(0, 100, 320, 100, TFT_BLUE);
  tft.drawLine(64, 30, 64, 0, TFT_BLUE);
  tft.drawLine(210, 100, 210, 218, TFT_BLUE);
  tft.drawLine(268, 30, 268, 0, TFT_BLUE);
  tft.drawLine(0, 165, 210, 165, TFT_BLUE);
  tft.drawLine(0, 187, 320, 187, TFT_BLUE);
  tft.drawLine(0, 218, 320, 218, TFT_BLUE);
  tft.drawLine(108, 30, 108, 0, TFT_BLUE);
  tft.drawLine(174, 30, 174, 0, TFT_BLUE);
  tft.drawLine(20, 120, 200, 120, TFT_DARKGREY);
  tft.drawLine(20, 150, 200, 150, TFT_DARKGREY);

  tft.setTextColor(TFT_WHITE);
  tft.drawString("SQ:", 216, 168, 2);
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
  tft.drawString("80", 110, 153, 1);
  tft.drawString("100", 134, 153, 1);
  tft.drawString("120", 164, 153, 1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("kHz", 225, 6, 4);
  tft.setTextColor(TFT_WHITE);
  if (band == 0) {
    tft.drawString("MHz", 256, 78, 4);
  } else {
    tft.drawString("kHz", 256, 78, 4);
  }
  tft.setTextColor(TFT_WHITE);
  tft.drawString("dBuV", 280, 155, 2);
  tft.drawPixel(295, 166, TFT_WHITE);
  tft.drawPixel(295, 167, TFT_WHITE);
  tft.drawPixel(295, 168, TFT_WHITE);
  tft.drawPixel(295, 169, TFT_WHITE);
  tft.drawPixel(295, 170, TFT_WHITE);
  RDSstatusold = false;
  Stereostatusold = false;
  ShowFreq(0);
  updateTuneMode();
  updateBW();
  ShowUSBstatus();
  ShowStepSize();
  updateiMS();
  updateEQ();
  Squelchold = -2;
  SStatusold = 2000;
  SStatus = 100;
  tft.drawCircle(81, 15, 10, TFT_GREYOUT);
  tft.drawCircle(81, 15, 9, TFT_GREYOUT);
  tft.drawCircle(91, 15, 10, TFT_GREYOUT);
  tft.drawCircle(91, 15, 9, TFT_GREYOUT);
  tft.drawBitmap(110, 5, RDSLogo, 67, 22, TFT_GREYOUT);
  if (StereoToggle == false) {
    tft.drawCircle(86, 15, 10, TFT_SKYBLUE);
    tft.drawCircle(86, 15, 9, TFT_SKYBLUE);
  }
}

void ShowFreq(int mode)
{
  if (setupmode == false) {
    if (band == 1) {
      if (freqold < 2000 && radio.getFrequency_AM() >= 2000 && stepsize == 0)
      {
        radio.setFrequency_AM(2000);
      }

      if (freqold >= 2000 && radio.getFrequency_AM() < 2000 && stepsize == 0)
      {
        radio.setFrequency_AM(1998);
      }
    }
  }

  if (screenmute == false) {
    detachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A));
    detachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B));
    if (band == 1) {
      unsigned int freq = radio.getFrequency_AM();
      String count = String(freq, DEC);
      if (count.length() != freqoldcount || mode != 0) {
        tft.setTextColor(TFT_BLACK, TFT_BLACK);
        tft.drawRightString(String(freqold), 248, 45, 7);
      }
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawRightString(String(freq), 248, 45, 7);
      freqold = freq;
      freqoldcount = count.length();
    } else {
      unsigned int freq = radio.getFrequency() + ConverterSet * 100;
      String count = String(freq / 100, DEC);
      if (count.length() != freqoldcount || mode != 0) {
        tft.setTextColor(TFT_BLACK, TFT_BLACK);
        if (freqoldcount <= 2) {
          tft.setCursor (108, 45);
        }
        if (freqoldcount == 3) {
          tft.setCursor (76, 45);
        }
        if (freqoldcount == 4) {
          tft.setCursor (44, 45);
        }
        tft.setTextFont(7);
        tft.print(freqold / 100);
        tft.print(".");
        if (freqold % 100 < 10) {
          tft.print("0");
        }
        tft.print(freqold % 100);
      }

      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      if (mode == 0) {
        if (count.length() <= 2) {
          tft.setCursor (108, 45);
        }
        if (count.length() == 3) {
          tft.setCursor (76, 45);
        }
        if (count.length() == 4) {
          tft.setCursor (44, 45);
        }
        tft.setTextFont(7);
        tft.print(freq / 100);
        tft.print(".");
        if (freq % 100 < 10) {
          tft.print("0");
        }
        tft.print(freq % 100);
        freqold = freq;
        freqoldcount = count.length();
      } else if (mode == 1) {
        tft.setTextColor(TFT_BLACK, TFT_BLACK);
        if (freqoldcount <= 2) {
          tft.setCursor (98, 45);
        }
        if (freqoldcount == 3) {
          tft.setCursor (71, 45);
        }
        if (freqoldcount == 4) {
          tft.setCursor (44, 45);
        }
        tft.setTextFont(7);
        tft.print(freqold / 100);
        tft.print(".");
        if (freqold % 100 < 10) {
          tft.print("0");
        }
        tft.print(freqold % 100);
      }
    }
    attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A),  encoderISR,       CHANGE);
    attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B),  encoderISR,       CHANGE);
  }
}

void ShowSignalLevel()
{
  SAvg = (((SAvg * 9) + 5) / 10) + SStatus;
  SStatus = SAvg / 10;
  int16_t smeter = 0;
  float sval = 0;
  if (SStatus > 0)
  {
    if (SStatus < 1000)
    {
      sval = 51 * ((pow(10, (((float)SStatus) / 1000))) - 1);
      smeter = int16_t(sval);
    }
    else
    {
      smeter = 511;
    }
  }
  if (menu == false) {
    analogWrite(SMETERPIN, smeter);
  }
  if (SStatus > (SStatusold + 3) || SStatus < (SStatusold - 3))
  {
    if (SStatus > 1200) {
      SStatus = 1200;
    }
    if (SStatus < -100) {
      SStatus = -100;
    }
    String count = String(SStatus / 10, DEC);
    if (SStatusold != 2000) {
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      if (SStatusold >= 0) {
        if (SStatusoldcount <= 1) {
          tft.setCursor (267, 110);
        }
        if (SStatusoldcount == 2) {
          tft.setCursor (240, 110);
        }
        if (SStatusoldcount >= 3) {
          tft.setCursor (213, 110);
        }
      } else {
        if (SStatusoldcount <= 1) {
          tft.setCursor (267, 110);
        }
        if (SStatusoldcount == 2) {
          tft.setCursor (250, 110);
        }
        if (SStatusoldcount >= 3) {
          tft.setCursor (223, 110);
        }
      }
      tft.setTextFont(6);
      if (SStatusold / 10 != SStatusold / 10) {
        tft.print(SStatusold / 10);
      }
      tft.setTextFont(4);
      tft.print(".");
      if (SStatusold < 0)
      {
        String negative = String (SStatusold % 10, DEC);
        if (SStatusold % 10 == 0) {
          tft.print("0");
        } else {
          tft.print(negative[1]);
        }
      } else {
        tft.print(SStatusold % 10);
      }
    }
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    if (SStatus >= 0) {
      if (count.length() == 1) {
        tft.setCursor (267, 110);
      }
      if (count.length() == 2) {
        tft.setCursor (240, 110);
      }
      if (count.length() == 3) {
        tft.setCursor (213, 110);
      }
    } else {
      if (count.length() == 1) {
        tft.setCursor (267, 110);
      }
      if (count.length() == 2) {
        tft.setCursor (250, 110);
      }
      if (count.length() >= 3) {
        tft.setCursor (223, 110);
      }
    }
    tft.setTextFont(6);
    tft.print(SStatus / 10);
    tft.setTextFont(4);
    tft.print(".");
    if (SStatus < 0)
    {
      String negative = String (SStatus % 10, DEC);
      if (SStatus % 10 == 0) {
        tft.print("0");
      } else {
        tft.print(negative[1]);
      }
    } else {
      tft.print(SStatus % 10);
    }

    if (band == 0) {
      if (SStatus / 10 > 0) {
        tft.fillRect(20, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(20, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 6) {
        tft.fillRect(34, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(34, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 12) {
        tft.fillRect(48, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(48, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 18) {
        tft.fillRect(62, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(62, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 26) {
        tft.fillRect(76, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(76, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 32) {
        tft.fillRect(90, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(90, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 38) {
        tft.fillRect(104, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(104, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 44) {
        tft.fillRect(118, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(118, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 50) {
        tft.fillRect(132, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(132, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 56) {
        tft.fillRect(146, 113, 12, 4, TFT_RED);
      } else {
        tft.fillRect(146, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 62) {
        tft.fillRect(160, 113, 12, 4, TFT_RED);
      } else {
        tft.fillRect(160, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 68) {
        tft.fillRect(174, 113, 12, 4, TFT_RED);
      } else {
        tft.fillRect(174, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 74) {
        tft.fillRect(188, 113, 12, 4, TFT_RED);
      } else {
        tft.fillRect(188, 113, 12, 4, TFT_GREYOUT);
      }
    } else {
      if (SStatus / 10 > -14) {
        tft.fillRect(20, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(20, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > -8) {
        tft.fillRect(34, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(34, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > -2) {
        tft.fillRect(48, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(48, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 0) {
        tft.fillRect(62, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(62, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 10) {
        tft.fillRect(76, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(76, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 16) {
        tft.fillRect(90, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(90, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 22) {
        tft.fillRect(104, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(104, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 28) {
        tft.fillRect(118, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(118, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 37) {
        tft.fillRect(132, 113, 12, 4, TFT_GREEN);
      } else {
        tft.fillRect(132, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 44) {
        tft.fillRect(146, 113, 12, 4, TFT_RED);
      } else {
        tft.fillRect(146, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 54) {
        tft.fillRect(160, 113, 12, 4, TFT_RED);
      } else {
        tft.fillRect(160, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 64) {
        tft.fillRect(174, 113, 12, 4, TFT_RED);
      } else {
        tft.fillRect(174, 113, 12, 4, TFT_GREYOUT);
      }
      if (SStatus / 10 > 74) {
        tft.fillRect(188, 113, 12, 4, TFT_RED);
      } else {
        tft.fillRect(188, 113, 12, 4, TFT_GREYOUT);
      }
    }
    SStatusold = SStatus;
    SStatusoldcount = count.length();
  }
}

void ShowRDSLogo(bool RDSstatus)
{
  if (screenmute == false) {
    if (RDSstatus != RDSstatusold)
    {
      if (RDSstatus == true)
      {
        tft.drawBitmap(110, 5, RDSLogo, 67, 22, TFT_SKYBLUE);
      } else {
        tft.drawBitmap(110, 5, RDSLogo, 67, 22, TFT_GREYOUT);
      }
      RDSstatusold = RDSstatus;
    }
  }
}

void ShowStereoStatus()
{
  if (StereoToggle == true)
  {
    if (band == 0) {
      Stereostatus = radio.getStereoStatus();
    } else {
      Stereostatus = 0;
    }
    if (Stereostatus != Stereostatusold)
    {
      if (Stereostatus == true && screenmute == false)
      {
        tft.drawCircle(81, 15, 10, TFT_RED);
        tft.drawCircle(81, 15, 9, TFT_RED);
        tft.drawCircle(91, 15, 10, TFT_RED);
        tft.drawCircle(91, 15, 9, TFT_RED);
      } else {
        if (screenmute == false) {
          tft.drawCircle(81, 15, 10, TFT_GREYOUT);
          tft.drawCircle(81, 15, 9, TFT_GREYOUT);
          tft.drawCircle(91, 15, 10, TFT_GREYOUT);
          tft.drawCircle(91, 15, 9, TFT_GREYOUT);
        }
      }
      Stereostatusold = Stereostatus;
    }
  }
}

void ShowOffset()
{
  if (OStatus != OStatusold) {
    if (band == 0) {
      if (OStatus < -500)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_RED);
      } else if (OStatus < -250 && OStatus > -500)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_RED);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      } else if (USN < 250 && WAM < 250 && OStatus > -250 && OStatus < 250 && SQ == false)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREEN);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      } else if (OStatus > 250 && OStatus < 500)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_RED);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      } else if (OStatus > 500)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_RED);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      } else {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      }
    } else {
      if (OStatus <= -3)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_RED);
      } else if (OStatus < -2 && OStatus > -3)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_RED);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      } else if (OStatus > -2 && OStatus < 2 && SQ == false)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREEN);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      } else if (OStatus > 2 && OStatus < 3)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_RED);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      } else if (OStatus >= 3)
      {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_RED);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      } else {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
      }
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
    tft.drawRightString(BWOldString, 218, 6, 4);
    if (BWset == 0) {
      tft.setTextColor(TFT_SKYBLUE);
    } else {
      tft.setTextColor(TFT_YELLOW);
    }
    tft.drawRightString(BWString, 218, 6, 4);
    BWOld = BW;
    BWreset = false;
  }
}

void ShowModLevel()
{
  int hold;
  if (SQ == false) {} else {
    MStatus = 0;
    MStatusold = 1;
  }

  if (MStatus != MStatusold || MStatus < 10)
  {
    if (MStatus > 0) {
      tft.fillRect(20, 143, 12, 4, TFT_GREEN);
      hold = 20;
    } else {
      tft.fillRect(20, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 10) {
      tft.fillRect(34, 143, 12, 4, TFT_GREEN);
      hold = 34;
    } else {
      tft.fillRect(34, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 20) {
      tft.fillRect(48, 143, 12, 4, TFT_GREEN);
      hold = 48;
    } else {
      tft.fillRect(48, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 30) {
      tft.fillRect(62, 143, 12, 4, TFT_GREEN);
      hold = 62;
    } else {
      tft.fillRect(62, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 40) {
      tft.fillRect(76, 143, 12, 4, TFT_GREEN);
      hold = 76;
    } else {
      tft.fillRect(76, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 50) {
      tft.fillRect(90, 143, 12, 4, TFT_GREEN);
      hold = 90;
    } else {
      tft.fillRect(90, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 60) {
      tft.fillRect(104, 143, 12, 4, TFT_GREEN);
      hold = 104;
    } else {
      tft.fillRect(104, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 70) {
      tft.fillRect(118, 143, 12, 4, TFT_GREEN);
      hold = 118;
    } else {
      tft.fillRect(118, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 80) {
      tft.fillRect(132, 143, 12, 4, TFT_YELLOW);
      hold = 132;
    } else {
      tft.fillRect(132, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 90) {
      tft.fillRect(146, 143, 12, 4, TFT_RED);
      hold = 146;
    } else {
      tft.fillRect(146, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 100) {
      tft.fillRect(160, 143, 12, 4, TFT_RED);
      hold = 160;
    } else {
      tft.fillRect(160, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 110) {
      tft.fillRect(174, 143, 12, 4, TFT_RED);
      hold = 174;
    } else {
      tft.fillRect(174, 143, 12, 4, TFT_GREYOUT);
    }
    if (MStatus > 120) {
      tft.fillRect(188, 143, 12, 4, TFT_RED);
      hold = 188;
    } else {
      tft.fillRect(188, 143, 12, 4, TFT_GREYOUT);
    }

    if (peakholdold < hold) {
      peakholdold = hold;
    }
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
  if (USBstatus == false) {
    Squelch = analogRead(PIN_POT) / 4 - 100;
    if (Squelch > 920) {
      Squelch = 920;
    }
    if (seek == false && menu == false && Squelch != Squelchold) {
      tft.setTextFont(2);
      tft.setTextColor(TFT_BLACK);
      tft.setCursor (240, 168);
      if (Squelchold == -100) {
        tft.print("OFF");
      } else if (Squelchold == 920) {
        tft.print("STEREO");
      } else {
        tft.print(Squelchold / 10);
      }
      tft.setTextColor(TFT_WHITE);
      tft.setCursor (240, 168);
      if (Squelch == -100) {
        tft.print("OFF");
      } else if (Squelch == 920) {
        tft.print("STEREO");
      } else {
        tft.print(Squelch / 10);
      }
      Squelchold = Squelch;
    }
  }
  if (seek == false && USBstatus == true) {
    if (XDRMute == false) {
      if (Squelch != -1) {
        if (Squelch < SStatus || Squelch == -100) {
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
      if (screenmute == false) {
        if (Squelch != Squelchold) {
          tft.setTextFont(2);
          tft.setTextColor(TFT_BLACK);
          tft.setCursor (240, 168);
          if (Squelchold == -1) {
            tft.print("STEREO");
          } else {
            tft.print(Squelchold / 10);
          }
          tft.setTextColor(TFT_WHITE);
          tft.setCursor (240, 168);
          if (Squelch == -1) {
            tft.print("STEREO");
          } else {
            tft.print(Squelch / 10);
          }
          Squelchold = Squelch;
        }
      }
    }
  } else {
    if (seek == false && Squelch != 920) {
      if (Squelch < SStatus || Squelch == -100) {
        radio.setUnMute();
        SQ = false;
      } else {
        radio.setMute();
        SQ = true;
      }
    } else {
      if (seek == false && Stereostatus == true) {
        radio.setUnMute();
        SQ = false;
      } else {
        radio.setMute();
        SQ = true;
      }
    }
  }
  ShowSquelch();
}

void ShowSquelch() {
  if (menu == false) {
    if (SQ == false) {
      tft.drawRoundRect(3, 79, 40, 20, 5, TFT_GREYOUT);
      tft.setTextColor(TFT_GREYOUT);
      tft.drawCentreString("MUTE", 24, 81, 2);
    } else {
      tft.drawRoundRect(3, 79, 40, 20, 5, TFT_WHITE);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("MUTE", 24, 81, 2);
    }
  }
}


void updateBW() {
  if (BWset == 0) {
    if (screenmute == false) {
      tft.drawRoundRect(249, 35, 68, 20, 5, TFT_WHITE);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("AUTO BW", 283, 37, 2);
    }
    radio.setFMABandw();
  } else {
    if (screenmute == false) {
      tft.drawRoundRect(249, 35, 68, 20, 5, TFT_GREYOUT);
      tft.setTextColor(TFT_GREYOUT);
      tft.drawCentreString("AUTO BW", 283, 37, 2);
    }
  }
}

void updateiMS() {
  if (band == 0) {
    if (iMSset == 0) {
      if (screenmute == false) {
        tft.drawRoundRect(249, 56, 30, 20, 5, TFT_WHITE);
        tft.setTextColor(TFT_WHITE);
        tft.drawCentreString("iMS", 265, 58, 2);
      }
      radio.setiMS(1);
    } else {
      if (screenmute == false) {
        tft.drawRoundRect(249, 56, 30, 20, 5, TFT_GREYOUT);
        tft.setTextColor(TFT_GREYOUT);
        tft.drawCentreString("iMS", 265, 58, 2);
      }
      radio.setiMS(0);
    }
  }
}

void updateEQ() {
  if (band == 0) {
    if (EQset == 0) {
      if (screenmute == false) {
        tft.drawRoundRect(287, 56, 30, 20, 5, TFT_WHITE);
        tft.setTextColor(TFT_WHITE);
        tft.drawCentreString("EQ", 303, 58, 2);
      }
      radio.setEQ(1);
    } else {
      if (screenmute == false) {
        tft.drawRoundRect(287, 56, 30, 20, 5, TFT_GREYOUT);
        tft.setTextColor(TFT_GREYOUT);
        tft.drawCentreString("EQ", 303, 58, 2);
      }
      radio.setEQ(0);
    }
  }
}

void doBW() {
  if (band == 0) {
    if (BWset > 16) {
      BWset = 0;
    }

    if (BWset == 1) {
      radio.setFMBandw(56);
    }
    if (BWset == 2) {
      radio.setFMBandw(64);
    }
    if (BWset == 3) {
      radio.setFMBandw(72);
    }
    if (BWset == 4) {
      radio.setFMBandw(84);
    }
    if (BWset == 5) {
      radio.setFMBandw(97);
    }
    if (BWset == 6) {
      radio.setFMBandw(114);
    }
    if (BWset == 7) {
      radio.setFMBandw(133);
    }
    if (BWset == 8) {
      radio.setFMBandw(151);
    }
    if (BWset == 9) {
      radio.setFMBandw(168);
    }
    if (BWset == 10) {
      radio.setFMBandw(184);
    }
    if (BWset == 11) {
      radio.setFMBandw(200);
    }
    if (BWset == 12) {
      radio.setFMBandw(217);
    }
    if (BWset == 13) {
      radio.setFMBandw(236);
    }
    if (BWset == 14) {
      radio.setFMBandw(254);
    }
    if (BWset == 15) {
      radio.setFMBandw(287);
    }
    if (BWset == 16) {
      radio.setFMBandw(311);
    }
  } else {
    if (BWset > 4) {
      BWset = 1;
    }

    if (BWset == 1) {
      radio.setAMBandw(3);
    }
    if (BWset == 2) {
      radio.setAMBandw(4);
    }
    if (BWset == 3) {
      radio.setAMBandw(6);
    }
    if (BWset == 4) {
      radio.setAMBandw(8);
    }
  }
  updateBW();
  BWreset = true;
}

void doTuneMode() {
  if (band == 0) {
    if (tunemode == true) {
      tunemode = false;
    } else {
      tunemode = true;
    }
    updateTuneMode();
    if (stepsize != 0) {
      stepsize = 0;
      RoundStep();
      ShowStepSize();
      ShowFreq(0);
    }
  }
}

void updateTuneMode() {
  if (tunemode == true) {
    tft.drawRoundRect(3, 57, 40, 20, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("AUTO", 24, 59, 2);

    tft.drawRoundRect(3, 35, 40, 20, 5, TFT_GREYOUT);
    tft.setTextColor(TFT_GREYOUT);
    tft.drawCentreString("MAN", 24, 37, 2);
  } else {
    tft.drawRoundRect(3, 57, 40, 20, 5, TFT_GREYOUT);
    tft.setTextColor(TFT_GREYOUT);
    tft.drawCentreString("AUTO", 24, 59, 2);

    tft.drawRoundRect(3, 35, 40, 20, 5, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("MAN", 24, 37, 2);
  }
}
void ShowUSBstatus() {
  if (USBstatus == true)
  {
    tft.drawBitmap(272, 6, USBLogo, 43, 21, TFT_SKYBLUE);
  } else {
    tft.drawBitmap(272, 6, USBLogo, 43, 21, TFT_GREYOUT);
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
          if (band != 0) {
            band = 0;
            SelectBand();
          }
          Serial.print("T" + String(frequency * 10));
          Serial.print("A0\n");
          Serial.print("D0\n");
          Serial.print("G00\n");
          USBstatus = true;
          ShowUSBstatus();
          if (menu == true) {
            ModeButtonPress();
          }
          if (Squelch != Squelchold) {
            if (screenmute == false) {
              tft.setTextFont(2);
              tft.setTextColor(TFT_BLACK);
              tft.setCursor (240, 168);
              if (Squelchold == -100) {
                tft.print("OFF");
              } else if (Squelchold > 920) {
                tft.print("STEREO");
              } else {
                tft.print(Squelchold / 10);
              }
            }
          }
          break;

        case 'A':
          AGC = atol(buff + 1);
          Serial.print("A");
          Serial.print(AGC);
          Serial.print("\n");
          radio.setAGC(AGC);
          break;

        case 'C':
          byte scanmethod;
          scanmethod = atol(buff + 1);
          Serial.print("C");
          if (seek == false) {
            if (scanmethod == 1) {
              Serial.print("1\n");
              direction = true;
              seek = true;
              Seek(direction);
            }
            if (scanmethod == 2) {
              Serial.print("2\n");
              direction = false;
              seek = true;
              Seek(direction);
            }
          } else {
            seek = false;
          }
          Serial.print("C0\n");
          break;

        case 'N':
          doStereoToggle();
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
          } else if (XDRBWset == 1) {
            BWset = 2;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 2) {
            BWset = 3;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 3) {
            BWset = 4;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 4) {
            BWset = 5;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 5) {
            BWset = 6;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 6) {
            BWset = 7;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 7) {
            BWset = 8;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 8) {
            BWset = 9;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 9) {
            BWset = 10;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 10) {
            BWset = 11;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 11) {
            BWset = 12;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 12) {
            BWset = 13;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 13) {
            BWset = 14;
            XDRBWsetold = XDRBWset;
          } else if (XDRBWset == 14) {
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
            MuteScreen(0);
            LowLevelSet = EEPROM.readByte(43);
            EEPROM.commit();
            Serial.print("00\n");
          }
          if (LevelOffset == 10) {
            MuteScreen(1);
            LowLevelSet = EEPROM.readByte(43);
            EEPROM.commit();
            Serial.print("10\n");
          }
          if (LevelOffset == 1) {
            MuteScreen(0);
            LowLevelSet = 120;
            Serial.print("01\n");
          }
          if (LevelOffset == 11) {
            LowLevelSet = 120;
            MuteScreen(1);
            Serial.print("11\n");
          }
          break;

        case 'M':
          byte XDRband;
          XDRband = atol(buff + 1);
          if (XDRband == 0) {
            band = 0;
            SelectBand();
            Serial.print("M0\n");
            Serial.print("T" + String(frequency * 10) + "\n");
          } else {
            band = 1;
            SelectBand();
            Serial.print("M1\n");
            Serial.print("T" + String(frequency_AM) + "\n");
          }
          break;

        case 'T':
          unsigned int freqtemp;
          freqtemp = atoi(buff + 1);
          if (seek == true) {
            seek = false;
          }
          if (freqtemp > 143 && freqtemp < 27001) {
            frequency_AM = freqtemp;
            if (band != 1) {
              band = 1;
              SelectBand();
            } else {
              radio.setFrequency_AM(frequency_AM);
            }
            Serial.print("M1\n");
          } else if (freqtemp > 64999 && freqtemp < 108001) {
            frequency = freqtemp / 10;
            if (band != 0) {
              band = 0;
              SelectBand();
              Serial.print("M0\n");
            } else {
              radio.setFrequency(frequency, 65, 108);
            }
          }
          if (band == 0) {
            Serial.print("T" + String(frequency * 10) + "\n");
          } else {
            Serial.print("T" + String(frequency_AM) + "\n");
          }
          radio.clearRDS();
          ShowFreq(0);
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
            radio.setFrequency(scanner_start, 65, 108);
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
            if (screenmute == false) {
              ShowFreq(1);
              tft.setTextFont(4);
              tft.setTextColor(TFT_WHITE, TFT_BLACK);
              tft.setCursor (90, 60);
              tft.print("SCANNING...");
            }
            frequencyold = frequency / 10;
            for (freq_scan = scanner_start; freq_scan <= scanner_end; freq_scan += scanner_step)
            {
              radio.setFrequency(freq_scan, 65, 108);
              Serial.print(freq_scan * 10, DEC);
              Serial.print('=');
              delay(10);
              if (band == 0) {
                radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);
              } else {
                radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
              }
              Serial.print((SStatus / 10) + 10, DEC);
              Serial.print(',');
            }
            Serial.print('\n');
            if (screenmute == false) {
              tft.setTextFont(4);
              tft.setTextColor(TFT_BLACK, TFT_BLACK);
              tft.setCursor (90, 60);
              tft.print("SCANNING...");
            }
            radio.setFrequency(frequencyold, 65, 108);
            if (screenmute == false) {
              ShowFreq(0);
            }
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
            radio.setVolume((VolSet - 70) / 10);
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

        case 'Z':
          byte iMSEQX;
          iMSEQX = atol(buff + 1);
          Serial.print("Z");
          if (iMSEQX == 0) {
            iMSset = 1;
            EQset = 1;
            iMSEQ = 2;
          }
          if (iMSEQX == 1) {
            iMSset = 0;
            EQset = 1;
            iMSEQ = 3;
          }
          if (iMSEQX == 2) {
            iMSset = 1;
            EQset = 0;
            iMSEQ = 4;
          }
          if (iMSEQX == 3) {
            iMSset = 0;
            EQset = 0;
            iMSEQ = 1;
          }
          updateiMS();
          updateEQ();
          Serial.print(iMSEQX);
          Serial.print("\n");
          break;
      }
    }
  }

  if (USBstatus == true) {
    Stereostatus = radio.getStereoStatus();
    Serial.print("S");
    if (StereoToggle == false) {
      Serial.print("S");
    } else if (Stereostatus == true && band == 0) {
      Serial.print("s");
    } else {
      Serial.print("m");
    }
    if (SStatus > (SStatusold + 10) || SStatus < (SStatusold - 10)) {
      Serial.print(((SStatus * 100) + 10875) / 1000);
    } else {
      Serial.print(((SStatusold * 100) + 10875) / 1000);
    }
    Serial.print(',');
    Serial.print(100 - (WAM / 10), DEC);
    Serial.print(',');
    Serial.print(USN / 10, DEC);
    Serial.print("\n");
  }
}

void serial_hex(uint8_t val) {
  Serial.print((val >> 4) & 0xF, HEX);
  Serial.print(val & 0xF, HEX);
}

void Seek(bool mode) {
  if (band == 0) {
    radio.setMute();
    if (mode == false) {
      frequency = radio.tuneDown(stepsize, LowEdgeSet, HighEdgeSet);
    } else {
      frequency = radio.tuneUp(stepsize, LowEdgeSet, HighEdgeSet);
    }
    delay(50);
    ShowFreq(0);
    if (USBstatus == true) {
      if (band == 0) {
        Serial.print("T" + String(frequency * 10) + "\n");
      } else {
        Serial.print("T" + String(frequency_AM) + "\n");
      }
    }

    radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);

    if ((USN < 200) && (WAM < 230) && (OStatus < 80 && OStatus > -80) && (Squelch < SStatus || Squelch == 920)) {
      seek = false;
      radio.setUnMute();
      store = true;
    } else {
      seek = true;
    }
  }
}
