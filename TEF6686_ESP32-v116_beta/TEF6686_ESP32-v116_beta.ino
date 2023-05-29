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
  v1.14 - Bugfix: When signallevel drops down in digits, old digits were still displayed.
  v1.15 - Fix AM bandwidth setting on F6805 chipset
        - Added microcode for whole TEF668x family (Lithio)
        - Added C/N readout
        - S-meter and modulation meter update (Thanks David DSP)
        - Added splash screen with tuner info
        - Fixed problem when tuning beyond bandlimits on AM
        - FM level calibrated
        - Function added to choose between regular and optical rotary encoder (hold rotary button during boot)
  v1.16 - Fixed some bugs to make the script compatible with new boardmanagers (v2.x)
        - Rewritten rotary encoder routines
        - New RDS routine
        - Massive code cleanup
        - Fixed canvas when changing bands
        - Radiotext auto scroll when it does not fit in the window
        - Added RDS CT (clock only)
        - Added FullSearchRDS for TEF6687 & TEF6689

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
  Hold rotary button to switch between normal and optical rotary encoder

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
#include <TFT_eSPI.h>         // https://github.com/Bodmer/TFT_eSPI
#include <TimeLib.h>          // https://github.com/PaulStoffregen/Time

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
#define VERSION         "v1.16ARS"
#include "TFT_Colors.h"
TFT_eSPI tft = TFT_eSPI(320, 240);
#else
#define VERSION         "v1.16"
TFT_eSPI tft = TFT_eSPI(240, 320);
#endif

bool BWreset;
bool change2;
bool cleanup;
bool direction;
bool fullsearchrds;
bool LowLevelInit = false;
bool menu;
bool menuopen = false;
bool power = true;
bool RDSstatusold;
bool screenmute = false;
bool seek;
bool setupmode;
bool SQ;
bool Stereostatusold;
bool StereoToggle = true;
bool store;
bool tuned;
bool tunemode = false;
bool USBstatus = false;
bool XDRMute;
byte band;
byte BWset;
byte ContrastSet;
byte displayflip;
byte EQset;
byte freqoldcount;
byte iMSEQ;
byte iMSset;
byte LowLevelSet;
byte optenc;
byte rotarymode;
byte SNR;
byte SNRold;
byte SStatusoldcount;
byte stepsize;
byte TEF;
char buff[16];
char programServicePrevious[9];
char programTypePrevious[17];
char radioIdPrevious[5];
char radioTextPrevious[65];
int AGC;
int BWOld;
int charWidth = tft.textWidth("AA");
int ConverterSet;
int DeEmphasis;
int ForceMono;
int freqold;
int HighCutLevel;
int HighCutOffset;
int HighEdgeSet;
int LevelOffset;
int LowEdgeSet;
int lowsignaltimer;
int menuoption = 30;
int MStatusold;
int OStatusold;
int peakholdold;
int peakholdtimer;
int rotary;
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
int xPos = 6;
int yPos = 2;
int16_t OStatus;
int16_t SAvg;
int16_t SStatus;
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
String rds_clock;
String rds_clockold;
String RTold;
String StereoLevelString;
String VolString;
uint16_t BW;
uint16_t MStatus;
uint16_t USN;
uint16_t WAM;
uint8_t buff_pos = 0;
uint8_t RDSstatus;
unsigned int change;
unsigned int freq_scan;
unsigned int frequency;
unsigned int frequency_AM;
unsigned int frequencyold;
unsigned int scanner_end;
unsigned int scanner_start;
unsigned int scanner_step;
unsigned long peakholdmillis;
unsigned long rtticker;

TEF6686 radio;
TFT_eSprite sprite = TFT_eSprite(&tft);

void setup() {
  setupmode = true;
  EEPROM.begin(56);
  if (EEPROM.readByte(41) != 15) {
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
    EEPROM.writeByte(41, 15);
    EEPROM.writeByte(42, 0);
    EEPROM.writeByte(43, 20);
    EEPROM.writeByte(44, 1);
    EEPROM.writeByte(45, 1);
    EEPROM.writeByte(46, 0);
    EEPROM.writeUInt(47, 828);
    EEPROM.writeByte(52, 0);
    EEPROM.writeByte(53, 0);
    EEPROM.writeByte(54, 0);
    EEPROM.writeByte(55, 0);
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
  TEF = EEPROM.readByte(54);
  optenc = EEPROM.readByte(55);
  EEPROM.commit();
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

  TEF = EEPROM.readByte(54);

  if (TEF != 101 && TEF != 102 && TEF != 205) SetTunerPatch();

  radio.init(TEF);
  uint16_t device;
  uint16_t hw;
  uint16_t sw;
  radio.getIdentification(device, hw, sw);
  if (TEF != (highByte(hw) * 100 + highByte(sw))) SetTunerPatch();

  analogWrite(CONTRASTPIN, ContrastSet * 2 + 27);
  analogWrite(SMETERPIN, 0);

  radio.getIdentification(device, hw, sw);
  uint8_t version = highByte(hw) * 100 + highByte(sw);

  pinMode(MODEBUTTON, INPUT);
  pinMode(BWBUTTON, INPUT);
  pinMode(ROTARY_BUTTON, INPUT);
  pinMode(ROTARY_PIN_A, INPUT);
  pinMode(ROTARY_PIN_B, INPUT);
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B), read_encoder, CHANGE);

  if (digitalRead(BWBUTTON) == LOW) {
    if (rotarymode == 0) rotarymode = 1; else rotarymode = 0;
    EEPROM.writeByte(52, rotarymode);
    EEPROM.commit();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Rotary direction changed", 150, 70, 4);
    tft.drawCentreString("Please release button", 150, 100, 4);
    while (digitalRead(BWBUTTON) == LOW) delay(50);
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
    while (digitalRead(MODEBUTTON) == LOW) delay(50);
  }

  if (digitalRead(PWRBUTTON) == LOW) {
    analogWrite(SMETERPIN, 511);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Calibrate analog meter", 150, 70, 4);
    tft.drawCentreString("Release button when ready", 150, 100, 4);
    while (digitalRead(PWRBUTTON) == LOW) delay(50);
    analogWrite(SMETERPIN, 0);
  }

  if (digitalRead(ROTARY_BUTTON) == LOW) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    if (optenc == 0) {
      optenc = 1;
      tft.drawCentreString("encoder set to optical", 150, 70, 4);
    } else {
      optenc = 0;
      tft.drawCentreString("encoder set to standard", 150, 70, 4);
    }
    EEPROM.writeByte(55, optenc);
    EEPROM.commit();
    tft.drawCentreString("Please release button", 150, 100, 4);
    while (digitalRead(ROTARY_BUTTON) == LOW) delay(50);
  }

  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  tft.pushImage (0, 99, 211, 140, pe5pvblogo);
  tft.pushImage (239, 200, 80, 30, nxplogo);
  tft.setTextColor(TFT_SKYBLUE);
  tft.drawCentreString("FM/AM receiver", 160, 10, 4);
  tft.setTextColor(TFT_YELLOW);
  tft.drawCentreString("Software " + String(VERSION), 160, 30, 2);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Lithio", 260, 180, 2);

  if (lowByte(device) == 14) {
    fullsearchrds = false;
    tft.drawString("Tuner: TEF6686 Lithio (" + String(version) + ")", 80, 60, 2);
  } else if (lowByte(device) == 1) {
    fullsearchrds = true;
    tft.drawString("Tuner: TEF6687 Lithio FMSI (" + String(version) + ")", 80, 60, 2);
  } else if (lowByte(device) == 9) {
    fullsearchrds = false;
    tft.drawString("Tuner: TEF6688 Lithio DR (" + String(version) + ")", 80, 60, 2);
  } else if (lowByte(device) == 3) {
    fullsearchrds = true;
    tft.drawString("Tuner: TEF6689 Lithio FMSI DR (" + String(version) + ")", 80, 60, 2);
  } else {
    tft.setTextColor(TFT_RED);
    tft.drawString("Tuner: !None!", 80, 60, 2);
    while (true);
    for (;;);
  }
  tft.drawString("Patch: v" + String(TEF), 80, 75, 2);
  delay(1500);

  radio.setVolume(VolSet);
  radio.setOffset(LevelOffset);
  radio.setStereoLevel(StereoLevel);
  radio.setHighCutLevel(HighCutLevel);
  radio.setHighCutOffset(HighCutOffset);
  radio.clearRDS(fullsearchrds);
  radio.setMute();
  LowLevelInit = true;

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
  sprite.createSprite(313, 18);
}

void loop() {
  if (digitalRead(PWRBUTTON) == LOW && USBstatus == false) PWRButtonPress();

  if (power == true) {
    if (seek == true) Seek(direction);

    if (SStatus / 10 > LowLevelSet && LowLevelInit == false && menu == false && band == 0) {
      radio.clearRDS(fullsearchrds);
      if (screenmute == false) {
        tft.setTextColor(TFT_WHITE);
        tft.drawString("20", 20, 153, 1);
        tft.drawString("40", 50, 153, 1);
        tft.drawString("60", 80, 153, 1);
        tft.drawString("80", 110, 153, 1);
        tft.drawString("100", 134, 153, 1);
        tft.drawString("120", 164, 153, 1);
        tft.drawString("%", 196, 153, 1);
        tft.drawString("M", 6, 136, 2);
        tft.drawString("PI:", 216, 195, 2);
        tft.drawString("PS:", 6, 195, 2);
        tft.drawString("PTY:", 6, 168, 2);
        tft.drawLine(20, 150, 200, 150, TFT_DARKGREY);
      }
      LowLevelInit = true;
    }

    if (SStatus / 10 <= LowLevelSet && band == 0) {
      if (LowLevelInit == true && menu == false) {
        radio.clearRDS(fullsearchrds);
        if (screenmute == false) {
          tft.setTextColor(TFT_BLACK );
          tft.drawString(PSold, 38, 192, 4);
          tft.drawString(PIold, 244, 192, 4);
          tft.drawString(RTold, 6, 222, 2);
          tft.drawString(PTYold, 38, 168, 2);
          tft.fillRect(20, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(34, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(48, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(62, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(76, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(90, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(104, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(118, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(132, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(146, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(160, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(174, 139, 12, 8, TFT_GREYOUT);
          tft.fillRect(188, 139, 12, 8, TFT_GREYOUT);
          tft.setTextColor(TFT_GREYOUT);
          tft.drawString("20", 20, 153, 1);
          tft.drawString("40", 50, 153, 1);
          tft.drawString("60", 80, 153, 1);
          tft.drawString("80", 110, 153, 1);
          tft.drawString("100", 134, 153, 1);
          tft.drawString("120", 164, 153, 1);
          tft.drawString("%", 196, 153, 1);
          tft.drawString("M", 6, 136, 2);
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
        if (band == 0) radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus); else radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
        if (screenmute == true) readRds();
        if (menu == false) doSquelch();
      }

    } else {
      if (band == 0) radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus); else radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
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
        if (band == 0) {
          showPI();
          showPTY();
          showCT();
          showPS();
          showRadioText();
          showPS();
          ShowStereoStatus();
        }
        ShowOffset();
        ShowSignalLevel();
        ShowBW();
      }
    }

    XDRGTKRoutine();

    if (menu == true && menuopen == true && menuoption == 110)
    {
      if (band == 0) radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus); else radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
      if (millis() >= lowsignaltimer + 500 || change2 == true) {
        lowsignaltimer = millis();
        change2 = false;
        if (SStatus > SStatusold || SStatus < SStatusold)
        {
          String count = String(SStatus / 10, DEC);
          if (screenmute == false) {
            tft.setTextColor(TFT_BLACK);
            if (SStatusold >= 0) {
              if (SStatusoldcount <= 1) tft.setCursor (100, 140);
              if (SStatusoldcount == 2) tft.setCursor (73, 140);
              if (SStatusoldcount >= 3) tft.setCursor (46, 140);
            } else {
              if (SStatusoldcount <= 1) tft.setCursor (100, 140);
              if (SStatusoldcount == 2) tft.setCursor (83, 140);
              if (SStatusoldcount >= 3) tft.setCursor (56, 140);
            }
            tft.setTextFont(6);
            tft.print(SStatusold / 10);
            tft.print(".");
            if (SStatusold < 0)
            {
              String negative = String (SStatusold % 10, DEC);
              if (SStatusold % 10 == 0) tft.print("0");  else tft.print(negative[1]);
            } else {
              tft.print(SStatusold % 10);
            }

            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            if (SStatus >= 0) {
              if (count.length() == 1) tft.setCursor (100, 140);
              if (count.length() == 2) tft.setCursor (73, 140);
              if (count.length() == 3) tft.setCursor (46, 140);
            } else {
              if (count.length() == 1) tft.setCursor (100, 140);
              if (count.length() == 2) tft.setCursor (83, 140);
              if (count.length() >= 3) tft.setCursor (56, 140);
            }
            tft.setTextFont(6);
            tft.print(SStatus / 10);
            tft.print(".");
            if (SStatus < 0)
            {
              String negative = String (SStatus % 10, DEC);
              if (SStatus % 10 == 0) tft.print("0"); else tft.print(negative[1]);
            } else {
              tft.print(SStatus % 10);
            }
            SStatusold = SStatus;
            SStatusoldcount = count.length();
          }
        }
      }
    }

    if (rotary == -1) KeyUp();
    if (rotary == 1) KeyDown();

    if (digitalRead(ROTARY_BUTTON) == LOW) ButtonPress();
    if (digitalRead(MODEBUTTON) == LOW && screenmute == false) ModeButtonPress();
    if (digitalRead(BWBUTTON) == LOW && screenmute == false) BWButtonPress();

    if (store == true) change++;

    if (change > 200 && store == true) {
      detachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A));
      detachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B));
      EEPROM.writeUInt(0, radio.getFrequency());
      EEPROM.writeUInt(47, radio.getFrequency_AM());
      EEPROM.writeByte(46, band);
      EEPROM.commit();
      store = false;
      attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A), read_encoder, CHANGE);
      attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B), read_encoder, CHANGE);
    }
  }
}

void PWRButtonPress() {
  if (menu == false) {
    unsigned long counterold = millis();
    unsigned long counter = millis();
    while (digitalRead(PWRBUTTON) == LOW && counter - counterold <= 1000) counter = millis();

    if (counter - counterold < 1000) {
      if (power == false) {
        ESP.restart();
      } else {
        if (band == 0) band = 1; else band = 0;
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
    while (digitalRead(PWRBUTTON) == LOW) delay(50);
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
    if (radio.getFrequency_AM() > 0) frequency_AM = radio.getFrequency_AM();
    radio.setFrequency_AM(frequency_AM);
    freqold = frequency_AM;
    doBW;
    radio.getStatus_AM(SStatus, USN, WAM, OStatus, BW, MStatus);
    if (screenmute == false) BuildDisplay();
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
    radio.setFrequency(frequency, LowEdgeSet, HighEdgeSet, fullsearchrds);
    freqold = frequency_AM;
    doBW;
    radio.getStatus(SStatus, USN, WAM, OStatus, BW, MStatus);
    if (screenmute == false) BuildDisplay();
  }
}

void BWButtonPress() {
  if (menu == false) {
    seek = false;
    unsigned long counterold = millis();
    unsigned long counter = millis();
    while (digitalRead(BWBUTTON) == LOW && counter - counterold <= 1000) counter = millis();

    if (counter - counterold < 1000) {
      BWset++;
      doBW();
    } else {
      doStereoToggle();
    }
  }
  while (digitalRead(BWBUTTON) == LOW) delay(50);
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
    while (digitalRead(MODEBUTTON) == LOW && counter - counterold <= 1000) counter = millis();

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
        tft.setTextColor(TFT_BLACK);
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
    radio.clearRDS(fullsearchrds);
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
  while (digitalRead(MODEBUTTON) == LOW) delay(50);
  delay(100);
}

void ShowStepSize() {
  tft.fillRect(224, 38, 15, 4, TFT_GREYOUT);
  tft.fillRect(193, 38, 15, 4, TFT_GREYOUT);
  if (band == 0) tft.fillRect(148, 38, 15, 4, TFT_GREYOUT); else tft.fillRect(162, 38, 15, 4, TFT_GREYOUT);
  if (stepsize == 1) tft.fillRect(224, 38, 15, 4, TFT_GREEN);
  if (stepsize == 2) tft.fillRect(193, 38, 15, 4, TFT_GREEN);
  if (stepsize == 3) {
    if (band == 0) tft.fillRect(148, 38, 15, 4, TFT_GREEN); else tft.fillRect(162, 38, 15, 4, TFT_GREEN);
  }
}

void RoundStep() {
  if (band == 0) {
    int freq = radio.getFrequency();
    if (freq % 10 < 3) {
      radio.setFrequency(freq - (freq % 10 - 5) - 5, LowEdgeSet, HighEdgeSet, fullsearchrds);
    }
    else if (freq % 10 > 2 && freq % 10 < 8) {
      radio.setFrequency(freq - (freq % 10 - 5) , LowEdgeSet, HighEdgeSet, fullsearchrds);
    }
    else if (freq % 10 > 7) {
      radio.setFrequency(freq - (freq % 10 - 5) + 5, LowEdgeSet, HighEdgeSet, fullsearchrds);
    }
  } else {
    int freq = radio.getFrequency_AM();
    if (freq < 2000) radio.setFrequency_AM((freq / 9) * 9); else radio.setFrequency_AM((freq / 5) * 5);
  }
  while (digitalRead(ROTARY_BUTTON) == LOW) delay(50);

  if (band == 0) EEPROM.writeUInt(0, radio.getFrequency()); else EEPROM.writeUInt(47, radio.getFrequency_AM());
  EEPROM.commit();
}

void ButtonPress() {
  if (menu == false) {
    seek = false;
    unsigned long counterold = millis();
    unsigned long counter = millis();
    while (digitalRead(ROTARY_BUTTON) == LOW && counter - counterold <= 1000) counter = millis();

    if (counter - counterold < 1000) {
      if (tunemode == false) {
        stepsize++;
        if (stepsize > 3) stepsize = 0;

        if (screenmute == false) ShowStepSize();

        EEPROM.writeByte(42, stepsize);
        EEPROM.commit();
        if (stepsize == 0) {
          RoundStep();
          ShowFreq(0);
        }
      }
    } else {
      if (iMSEQ == 0) iMSEQ = 1;

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

          tft.setTextColor(TFT_YELLOW);
          if (VolSet > 0) tft.drawRightString("+" + String(VolSet, DEC), 165, 110, 4); else tft.drawRightString(String(VolSet, DEC), 165, 110, 4);
          break;

        case 50:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Converter:", 150, 70, 4);
          tft.drawString("MHz", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);

          tft.drawRightString(String(ConverterSet, DEC), 165, 110, 4);
          break;

        case 70:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Low bandedge:", 150, 70, 4);
          tft.drawString("MHz", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(LowEdgeSet + ConverterSet, DEC), 165, 110, 4);
          break;

        case 90:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("High bandedge:", 150, 70, 4);
          tft.drawString("MHz", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(HighEdgeSet + ConverterSet, DEC), 165, 110, 4);
          break;

        case 110:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("RF Level offset:", 150, 70, 4);
          tft.drawString("dB", 170, 110, 4);
          tft.drawString("dBuV", 190, 157, 4);
          tft.setTextColor(TFT_YELLOW);
          if (LevelOffset > 0) tft.drawRightString("+" + String(LevelOffset, DEC), 165, 110, 4); else tft.drawRightString(String(LevelOffset, DEC), 165, 110, 4);
          SStatusold = 2000;
          change2 = true;
          break;

        case 130:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Stereo threshold:", 150, 70, 4);
          if (StereoLevel != 0) tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          if (StereoLevel != 0) tft.drawRightString(String(StereoLevel, DEC), 165, 110, 4); else tft.drawRightString("Off", 165, 110, 4);
          break;

        case 150:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("High Cut corner:", 150, 70, 4);
          if (HighCutLevel != 0) tft.drawString("Hz", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          if (HighCutLevel != 0) tft.drawRightString(String(HighCutLevel * 100, DEC), 165, 110, 4); else tft.drawRightString("Off", 165, 110, 4);
          break;

        case 170:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Highcut threshold:", 150, 70, 4);
          if (HighCutOffset != 0) tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          if (HighCutOffset != 0) tft.drawRightString(String(HighCutOffset, DEC), 165, 110, 4); else tft.drawRightString("Off", 165, 110, 4);
          break;

        case 190:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Low level threshold:", 150, 70, 4);
          tft.drawString("dBuV", 150, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(LowLevelSet, DEC), 145, 110, 4);
          break;

        case 210:
          tft.setTextColor(TFT_WHITE);
          tft.drawCentreString("Contrast:", 150, 70, 4);
          tft.drawString("%", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(ContrastSet, DEC), 165, 110, 4);
          break;
      }
    } else {
      menuopen = false;
      BuildMenu();
    }
  }
  while (digitalRead(ROTARY_BUTTON) == LOW) delay(50);
}

void KeyUp() {
  rotary = 0;
  if (menu == false) {
    if (tunemode == true) {
      direction = true;
      seek = true;
      Seek(direction);
    } else {
      if (band == 0) frequency = radio.tuneUp(stepsize, LowEdgeSet, HighEdgeSet, fullsearchrds); else frequency_AM = radio.tuneUp_AM(stepsize);
    }
      if (USBstatus == true) if (band == 0) Serial.println("T" + String(frequency * 10)); else Serial.println("T" + String(frequency_AM));
    radio.clearRDS(fullsearchrds);
    change = 0;
    ShowFreq(0);
    store = true;
  } else {
    if (menuopen == false) {
      tft.drawRoundRect(10, menuoption, 300, 18, 5, TFT_BLACK);
      menuoption += 20;
      if (menuoption > 210) menuoption = 30;
      tft.drawRoundRect(10, menuoption, 300, 18, 5, TFT_WHITE);
    } else {
      switch (menuoption) {
        case 30:
          tft.setTextColor(TFT_BLACK);
          if (VolSet > 0) tft.drawRightString("+" + String(VolSet, DEC), 165, 110, 4); else tft.drawRightString(String(VolSet, DEC), 165, 110, 4);
          VolSet++;
          if (VolSet > 10) VolSet = 10;
          tft.setTextColor(TFT_YELLOW);
          if (VolSet > 0) tft.drawRightString("+" + String(VolSet, DEC), 165, 110, 4); else tft.drawRightString(String(VolSet, DEC), 165, 110, 4);
          radio.setVolume(VolSet);
          break;

        case 50:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(ConverterSet, DEC), 165, 110, 4);
          ConverterSet++;
            if (ConverterSet > 2400 || ConverterSet <= 200) if (ConverterSet == 1) ConverterSet = 200; else ConverterSet = 0;
          if (ConverterSet >= 200) {
            Wire.beginTransmission(0x12);
            Wire.write(ConverterSet >> 8);
            Wire.write(ConverterSet & (0xFF));
            Wire.endTransmission();
          }
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(ConverterSet, DEC), 165, 110, 4);
          break;

        case 70:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(LowEdgeSet + ConverterSet, DEC), 165, 110, 4);
          LowEdgeSet ++;
          if (LowEdgeSet > 107) LowEdgeSet = 65;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(LowEdgeSet + ConverterSet, DEC), 165, 110, 4);
          break;

        case 90:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(HighEdgeSet + ConverterSet, DEC), 165, 110, 4);
          HighEdgeSet ++;
          if (HighEdgeSet > 108) HighEdgeSet = 66;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(HighEdgeSet + ConverterSet, DEC), 165, 110, 4);
          break;

        case 110:
          tft.setTextColor(TFT_BLACK);
          if (LevelOffset > 0) tft.drawRightString("+" + String(LevelOffset, DEC), 165, 110, 4); else tft.drawRightString(String(LevelOffset, DEC), 165, 110, 4);
          LevelOffset++;
          if (LevelOffset > 15) LevelOffset = -25;
          tft.setTextColor(TFT_YELLOW);
          if (LevelOffset > 0) tft.drawRightString("+" + String(LevelOffset, DEC), 165, 110, 4); else tft.drawRightString(String(LevelOffset, DEC), 165, 110, 4);          radio.setOffset(LevelOffset);
          change2 = true;
          break;

        case 130:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(StereoLevel, DEC), 165, 110, 4);
          StereoLevel ++;
            if (StereoLevel > 60 || StereoLevel <= 30) if (StereoLevel == 1) StereoLevel = 30; else StereoLevel = 0;
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (StereoLevel != 0) tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          if (StereoLevel != 0) tft.drawRightString(String(StereoLevel, DEC), 165, 110, 4); else tft.drawRightString("Off", 165, 110, 4);
          radio.setStereoLevel(StereoLevel);
          break;

        case 150:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(HighCutLevel * 100, DEC), 165, 110, 4);
          HighCutLevel ++;
          if (HighCutLevel > 70) HighCutLevel = 15;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(HighCutLevel * 100, DEC), 165, 110, 4);
          radio.setHighCutLevel(HighCutLevel);
          break;

        case 170:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(HighCutOffset, DEC), 165, 110, 4);
          HighCutOffset ++;
            if (HighCutOffset > 60 || HighCutOffset <= 20) if (HighCutOffset == 1) HighCutOffset = 20; else HighCutOffset = 0;
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (HighCutOffset != 0) tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          if (HighCutOffset != 0) tft.drawRightString(String(HighCutOffset, DEC), 165, 110, 4); else tft.drawRightString("Off", 165, 110, 4);
          radio.setHighCutOffset(HighCutOffset);
          break;

        case 190:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(LowLevelSet, DEC), 145, 110, 4);
          LowLevelSet++;
          if (LowLevelSet > 40 || LowLevelSet <= 0) LowLevelSet = 0;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(LowLevelSet, DEC), 145, 110, 4);
          break;

        case 210:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(ContrastSet, DEC), 165, 110, 4);
          ContrastSet ++;
          if (ContrastSet > 100) ContrastSet = 1;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(ContrastSet, DEC), 165, 110, 4);
          analogWrite(CONTRASTPIN, ContrastSet * 2 + 27);
          break;
      }
    }
  }
}

void KeyDown() {
  rotary = 0;
  if (menu == false) {
    if (tunemode == true) {
      direction = false;
      seek = true;
      Seek(direction);
    } else {
      if (band == 0) frequency = radio.tuneDown(stepsize, LowEdgeSet, HighEdgeSet, fullsearchrds); else frequency_AM = radio.tuneDown_AM(stepsize);
    }
      if (USBstatus == true) if (band == 0) Serial.println("T" + String(frequency * 10)); else Serial.println("T" + String(frequency_AM));
    radio.clearRDS(fullsearchrds);
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
          if (VolSet > 0) tft.drawRightString("+" + String(VolSet, DEC), 165, 110, 4); else tft.drawRightString(String(VolSet, DEC), 165, 110, 4);
          VolSet--;
          if (VolSet < -10) VolSet = -10;
          tft.setTextColor(TFT_YELLOW);
          if (VolSet > 0) tft.drawRightString("+" + String(VolSet, DEC), 165, 110, 4); else tft.drawRightString(String(VolSet, DEC), 165, 110, 4);          radio.setVolume(VolSet);
          break;

        case 50:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(ConverterSet, DEC), 165, 110, 4);
          ConverterSet--;
            if (ConverterSet < 200) if (ConverterSet < 0) ConverterSet = 2400; else ConverterSet = 0;
          if (ConverterSet >= 200) {
            Wire.beginTransmission(0x12);
            Wire.write(ConverterSet >> 8);
            Wire.write(ConverterSet & (0xFF));
            Wire.endTransmission();
          }
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(ConverterSet, DEC), 165, 110, 4);
          break;

        case 70:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(LowEdgeSet + ConverterSet, DEC), 165, 110, 4);
          LowEdgeSet --;
          if (LowEdgeSet < 65) LowEdgeSet = 107;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(LowEdgeSet + ConverterSet, DEC), 165, 110, 4);
          break;

        case 90:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(HighEdgeSet + ConverterSet, DEC), 165, 110, 4);
          HighEdgeSet --;
          if (HighEdgeSet < 66) HighEdgeSet = 108;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(HighEdgeSet + ConverterSet, DEC), 165, 110, 4);
          break;

        case 110:
          tft.setTextColor(TFT_BLACK);
          if (LevelOffset > 0) tft.drawRightString("+" + String(LevelOffset, DEC), 165, 110, 4); else tft.drawRightString(String(LevelOffset, DEC), 165, 110, 4);
          LevelOffset--;
          if (LevelOffset < -25) LevelOffset = 15;
          tft.setTextColor(TFT_YELLOW);
          if (LevelOffset > 0) tft.drawRightString("+" + String(LevelOffset, DEC), 165, 110, 4); else tft.drawRightString(String(LevelOffset, DEC), 165, 110, 4);          radio.setOffset(LevelOffset);
          change2 = true;
          break;

        case 130:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(StereoLevel, DEC), 165, 110, 4);
          StereoLevel --;
            if (StereoLevel < 30) if (StereoLevel < 0) StereoLevel = 60; else StereoLevel = 0;
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (StereoLevel != 0) {
            tft.drawString("dBuV", 170, 110, 4);
          }
          tft.setTextColor(TFT_YELLOW);
          if (StereoLevel != 0) tft.drawRightString(String(StereoLevel, DEC), 165, 110, 4); else tft.drawRightString("Off", 165, 110, 4);
          radio.setStereoLevel(StereoLevel);
          break;

        case 150:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(HighCutLevel * 100, DEC), 165, 110, 4);
          HighCutLevel --;
          if (HighCutLevel < 15) HighCutLevel = 70;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(HighCutLevel * 100, DEC), 165, 110, 4);
          radio.setHighCutLevel(HighCutLevel);
          break;

        case 170:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(HighCutOffset, DEC), 165, 110, 4);
          HighCutOffset --;
            if (HighCutOffset < 20) if (HighCutOffset < 0) HighCutOffset = 60; else HighCutOffset = 0;
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString("Off", 165, 110, 4);
          tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_WHITE);
          if (HighCutOffset != 0) tft.drawString("dBuV", 170, 110, 4);
          tft.setTextColor(TFT_YELLOW);
          if (HighCutOffset != 0) tft.drawRightString(String(HighCutOffset, DEC), 165, 110, 4); else tft.drawRightString("Off", 165, 110, 4);
          radio.setHighCutOffset(HighCutOffset);
          break;

        case 190:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(LowLevelSet, DEC), 145, 110, 4);
          LowLevelSet--;
          if (LowLevelSet > 40) LowLevelSet = 40;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(LowLevelSet, DEC), 145, 110, 4);
          break;


        case 210:
          tft.setTextColor(TFT_BLACK);
          tft.drawRightString(String(ContrastSet, DEC), 165, 110, 4);
          ContrastSet --;
          if (ContrastSet < 1) ContrastSet = 100;
          tft.setTextColor(TFT_YELLOW);
          tft.drawRightString(String(ContrastSet, DEC), 165, 110, 4);
          analogWrite(CONTRASTPIN, ContrastSet * 2 + 27);
          break;
      }
    }
  }
}



void readRds() {
  if (band == 0) {
    RDSstatus = radio.readRDS();
    ShowRDSLogo(RDSstatus);

    if (RDSstatus == 0) {
      tft.setTextColor(TFT_BLACK);
      tft.drawString(PIold, 244, 192, 4);
      tft.drawString(PSold, 38, 192, 4);
      tft.drawString(RTold, 6, 222, 2);
      tft.drawString(PTYold, 38, 168, 2);
    }

    if (RDSstatus == 1 && USBstatus == true) {
      Serial.print("P" + String(radio.rds.picode));
      Serial.print("\nR");
      Serial.print(String(((radio.rds.rdsB >> 8) >> 4) & 0xF, HEX) + String((radio.rds.rdsB >> 8) & 0xF, HEX));
      Serial.print(String(((radio.rds.rdsB) >> 4) & 0xF, HEX) + String((radio.rds.rdsB) & 0xF, HEX));
      Serial.print(String(((radio.rds.rdsC >> 8) >> 4) & 0xF, HEX) + String((radio.rds.rdsC >> 8) & 0xF, HEX));
      Serial.print(String(((radio.rds.rdsC) >> 4) & 0xF, HEX) + String((radio.rds.rdsC) & 0xF, HEX));
      Serial.print(String(((radio.rds.rdsD >> 8) >> 4) & 0xF, HEX) + String((radio.rds.rdsD >> 8) & 0xF, HEX));
      Serial.print(String(((radio.rds.rdsD) >> 4) & 0xF, HEX) + String((radio.rds.rdsD) & 0xF, HEX));
      Serial.print(String(((radio.rds.errors >> 8) >> 4) & 0xF, HEX) + String((radio.rds.errors >> 8) & 0xF, HEX));
      Serial.print("\n");
    }
  }
}

void showPI() {
  if ((RDSstatus == 1) && strcmp(radio.rds.picode, radioIdPrevious)) {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(PIold, 244, 192, 4);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(radio.rds.picode, 244, 192, 4);
    PIold = radio.rds.picode;
    strcpy(radioIdPrevious, radio.rds.picode);
  }
}

void showPTY() {
  if ((RDSstatus == 1) && strcmp(radio.rds.stationType, programTypePrevious)) {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(PTYold, 38, 168, 2);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(radio.rds.stationType, 38, 168, 2);
    PTYold = radio.rds.stationType;
    strcpy(programTypePrevious, radio.rds.stationType);
  }
}

void showPS() {
  if ((RDSstatus == 1) && strcmp(radio.rds.stationName, programServicePrevious)) {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(PSold, 38, 192, 4);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(radio.rds.stationName, 38, 192, 4);
    PSold = radio.rds.stationName;
    strcpy(programServicePrevious, radio.rds.stationName);
  }
}

void showRadioText() {
  if (RDSstatus == 1) {
    if (millis() - rtticker >= 350) {
      xPos -= charWidth;
      if (xPos < -tft.textWidth(radio.rds.stationText) + (charWidth * 42)) xPos = 6;
      sprite.fillSprite(TFT_BLACK);
      sprite.setTextColor(TFT_YELLOW);
      sprite.drawString(radio.rds.stationText, xPos, yPos, 2);
      sprite.pushSprite(6, 220);
      rtticker = millis();
      RTold = radio.rds.stationText;
      strcpy(radioTextPrevious, radio.rds.stationText);
      cleanup = true;
    }
  } else if (cleanup == true) {
    sprite.fillSprite(TFT_BLACK);
    sprite.pushSprite(6, 220);
    cleanup = false;
  }
}

void showCT() {
  if ((RDSstatus == 1) && radio.rds.hasCT == true) {
    int timeoffset;
    if (radio.rds.offsetplusmin == true) timeoffset = (-1 * radio.rds.offset) / 2; else timeoffset = radio.rds.offset / 2;
    setTime((radio.rds.hours + timeoffset), radio.rds.minutes, 0, 0, 0, 0);
    rds_clock = ((hour() < 10 ? "0" : "") + String(hour()) + ":" + (minute() < 10 ? "0" : "") + String(minute()));
  } else {
    rds_clock = "";
  }
  if (rds_clock != rds_clockold) {
    tft.setTextColor(TFT_BLACK);
    tft.drawRightString(rds_clockold, 205, 168, 2);
    tft.setTextColor(TFT_YELLOW);
    tft.drawRightString(rds_clock, 205, 168, 2);
    rds_clockold = rds_clock;
  }
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
  if (StereoLevel != 0) tft.drawRightString("dBuV", 305, 130, 2);
  if (HighCutLevel != 0) tft.drawRightString("Hz", 305, 150, 2);
  if (HighCutOffset != 0) tft.drawRightString("dBuV", 305, 170, 2);
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
  tft.setTextColor(TFT_YELLOW);
  if (VolSet > 0) tft.drawRightString("+" + String(VolSet, DEC), 270, 30, 2); else tft.drawRightString(String(VolSet, DEC), 270, 30, 2);
  tft.drawRightString(String(ConverterSet, DEC), 270, 50, 2);
  tft.drawRightString(String(LowEdgeSet + ConverterSet, DEC), 270, 70, 2);
  tft.drawRightString(String(HighEdgeSet + ConverterSet, DEC), 270, 90, 2);
  if (LevelOffset > 0) tft.drawRightString("+" + String(LevelOffset, DEC), 270, 110, 2); else tft.drawRightString(String(LevelOffset, DEC), 270, 110, 2);
  if (StereoLevel != 0) tft.drawRightString(String(StereoLevel, DEC), 270, 130, 2); else tft.drawRightString("Off", 270, 130, 2);
  if (HighCutLevel != 0) tft.drawRightString(String(HighCutLevel * 100, DEC), 270, 150, 2); else tft.drawRightString("Off", 270, 150, 2);
  if (HighCutOffset != 0) tft.drawRightString(String(HighCutOffset, DEC), 270, 170, 2); else tft.drawRightString("Off", 270, 170, 2);
  tft.drawRightString(String(LowLevelSet, DEC), 270, 190, 2);
  tft.drawRightString(String(ContrastSet, DEC), 270, 210, 2);
  analogWrite(SMETERPIN, 0);
}

void MuteScreen(int setting) {
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

void BuildDisplay() {
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
  for (uint16_t segments = 0; segments < 94; segments++) {
    if (segments > 54) {
      if (((segments - 53) % 10) == 0)
        tft.fillRect(16 + (2 * segments), 117, 2, 3, TFT_RED);
    } else {
      if (((segments + 1) % 6) == 0)
        tft.fillRect(16 + (2 * segments), 117, 2, 3, TFT_GREEN);
    }
  }
  tft.setTextColor(TFT_WHITE);
  tft.drawString("SQ:", 216, 155, 2);
  tft.drawString("S/N", 250, 168, 2);
  tft.drawString("dB",  300, 168, 2);
  tft.drawString("S", 6, 106, 2);
  tft.drawString("M", 6, 136, 2);
  tft.drawString("PI:", 216, 195, 2);
  tft.drawString("PS:", 6, 195, 2);
  tft.drawString("PTY:", 6, 168, 2);
  tft.drawString("%", 196, 153, 1);
  tft.drawString("1", 24, 123, 1);
  tft.drawString("3", 48, 123, 1);
  tft.drawString("5", 72, 123, 1);
  tft.drawString("7", 96, 123, 1);
  tft.drawString("9", 120, 123, 1);
  tft.drawString("+10", 134, 123, 1);
  tft.drawString("+30", 174, 123, 1);
  tft.drawString("20", 20, 153, 1);
  tft.drawString("40", 50, 153, 1);
  tft.drawString("60", 80, 153, 1);
  tft.drawString("80", 110, 153, 1);
  tft.drawString("100", 134, 153, 1);
  tft.drawString("120", 164, 153, 1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("kHz", 225, 6, 4);
  tft.setTextColor(TFT_WHITE);
  if (band == 0) tft.drawString("MHz", 256, 78, 4); else tft.drawString("kHz", 256, 78, 4);
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
  rds_clockold = "";
  strcpy(programTypePrevious, "");
  strcpy(radioIdPrevious, "");
  strcpy(programServicePrevious, "");
  strcpy(radioTextPrevious, "");
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

void ShowFreq(int mode) {
  if (setupmode == false) {
    if (band == 1) {
      if (freqold < 2000 && radio.getFrequency_AM() >= 2000 && stepsize == 0) if (radio.getFrequency_AM() != 27000 && freqold != 144) radio.setFrequency_AM(2000);
      if (freqold >= 2000 && radio.getFrequency_AM() < 2000 && stepsize == 0) if (radio.getFrequency_AM() != 144 && freqold != 27000) radio.setFrequency_AM(1998);
    }
  }

  if (screenmute == false) {
    detachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A));
    detachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B));
    if (band == 1) {
      unsigned int freq = radio.getFrequency_AM();
      String count = String(freq, DEC);
      if (count.length() != freqoldcount || mode != 0) {
        tft.setTextColor(TFT_BLACK);
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
        tft.setTextColor(TFT_BLACK);
        if (freqoldcount <= 2) tft.setCursor (108, 45);
        if (freqoldcount == 3) tft.setCursor (76, 45);
        if (freqoldcount >= 4) tft.setCursor (44, 45);
        tft.setTextFont(7);
        tft.print(freqold / 100);
        tft.print(".");
        if (freqold % 100 < 10) tft.print("0");
        tft.print(freqold % 100);
      }

      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      if (mode == 0) {
        if (count.length() <= 2) tft.setCursor (108, 45);
        if (count.length() == 3) tft.setCursor (76, 45);
        if (count.length() >= 4) tft.setCursor (44, 45);
        tft.setTextFont(7);
        tft.print(freq / 100);
        tft.print(".");
        if (freq % 100 < 10) tft.print("0");
        tft.print(freq % 100);
        freqold = freq;
        freqoldcount = count.length();
      } else if (mode == 1) {
        tft.setTextColor(TFT_BLACK);
        if (freqoldcount <= 2) tft.setCursor (98, 45);
        if (freqoldcount == 3) tft.setCursor (71, 45);
        if (freqoldcount == 4) tft.setCursor (44, 45);
        tft.setTextFont(1);
        tft.print(freqold / 100);
        tft.print(".");
        if (freqold % 100 < 10) tft.print("0");
        tft.print(freqold % 100);
      }
    }
    attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_A), read_encoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ROTARY_PIN_B), read_encoder, CHANGE);
  }
}

void ShowSignalLevel() {
  if (band == 0) SNR = int(0.46222375 * (float)(SStatus / 10) - 0.082495118 * (float)(USN / 10)) + 10; else SNR = -((int8_t)(USN / 10));

  if (SNR > (SNRold + 1) || SNR < (SNRold - 1)) {
    tft.setTextFont(2);
    tft.setCursor (280, 168);
    tft.setTextColor(TFT_BLACK);
    if (SNRold == 99) tft.print("--"); else  tft.print(SNRold);
    tft.setCursor (280, 168);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    if (tuned == true) {
      tft.print(SNR);
      SNRold = SNR;
    } else {
      tft.print("--");
      SNRold = 99;
    }
  }

  if (true)
  {
    int offset = (SStatus < 0) ? -5 : 5;

    if (abs((SAvg / 10) - SStatus) < 60) {
      SAvg = (((SAvg * 9) + offset) / 10) + SStatus;
      SStatus = SAvg / 10;
    } else {
      SAvg = SStatus * 10;
    }
  }

  int16_t smeter = 0;
  int16_t segments;
  float sval = 0;
  if (SStatus > 0)
  {
    if (SStatus < 1000)
    {
      sval = 51 * ((pow(10, (((float)SStatus) / 1000))) - 1);
      smeter = int16_t(sval);
    } else {
      smeter = 511;
    }
  }
  if (menu == false) analogWrite(SMETERPIN, smeter);

  if (SStatus > (SStatusold + 3) || SStatus < (SStatusold - 3))
  {
    if (SStatus > 1200) SStatus = 1200;
    if (SStatus < -400) SStatus = -400;
    String count = String(abs(SStatus / 10), DEC);
    tft.setTextColor(1, TFT_BLACK);
    tft.setCursor (213, 110);
    tft.setTextFont(6);
    if (SStatus >= 0) {
      if (count.length() == 1) tft.print("00");
      if (count.length() == 2) tft.print("0");
    } else {
      if (count.length() == 1) tft.print("0 ");
      if (count.length() == 2) tft.print(" ");
    }
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); //DSP
    if (SStatus < 0) tft.print("-");
    tft.print(abs(SStatus / 10));
    tft.setCursor (294, 110);
    tft.setTextFont(4);
    tft.print(".");
    if (SStatus < 0)
    {
      String negative = String (SStatus % 10, DEC);
      if (SStatus % 10 == 0) tft.print("0"); else tft.print(negative[1]);
    } else {
      tft.print(SStatus % 10);
    }

    if (band == 0) segments = (SStatus + 200) / 10; else segments = (SStatus + 200) / 10;

    tft.fillRect(16, 109, 2 * constrain(segments, 0, 54), 8, TFT_GREEN);
    tft.fillRect(16 + 2 * 54, 109, 2 * (constrain(segments, 54, 94) - 54), 8, TFT_RED);
    tft.fillRect(16 + 2 * constrain(segments, 0, 94), 109, 2 * (94 - constrain(segments, 0, 94)), 8, TFT_GREYOUT);

    SStatusold = SStatus;
    SStatusoldcount = count.length();
  }
}

void ShowRDSLogo(bool RDSstatus) {
  if (screenmute == false) {
    if (RDSstatus != RDSstatusold)
    {
      if (RDSstatus == true) tft.drawBitmap(110, 5, RDSLogo, 67, 22, TFT_SKYBLUE); else tft.drawBitmap(110, 5, RDSLogo, 67, 22, TFT_GREYOUT);
      RDSstatusold = RDSstatus;
    }
  }
}

void ShowStereoStatus() {
  if (StereoToggle == true)
  {
    if (band == 0) Stereostatus = radio.getStereoStatus(); else Stereostatus = 0;
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

void ShowOffset() {
  if (OStatus != OStatusold) {
    if (band == 0) {
      if (OStatus < -500) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_RED);
        tuned = false;
      } else if (OStatus < -250 && OStatus > -500) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_RED);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = false;
      } else if (USN < 250 && WAM < 250 && OStatus > -250 && OStatus < 250 && SQ == false) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREEN);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = true;
      } else if (OStatus > 250 && OStatus < 500) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_RED);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = false;
      } else if (OStatus > 500) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_RED);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = false;
      } else {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = false;
      }
    } else {
      if (OStatus <= -3) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_RED);
        tuned = false;
      } else if (OStatus < -2 && OStatus > -3) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_RED);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = false;
      } else if (OStatus > -2 && OStatus < 2 && SQ == false) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREEN);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = true;
      } else if (OStatus > 2 && OStatus < 3) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_RED);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = false;
      } else if (OStatus >= 3) {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_RED);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = false;
      } else {
        tft.fillTriangle(6, 8, 6, 22, 14, 14, TFT_GREYOUT);
        tft.fillTriangle(18, 8, 18, 22, 26, 14, TFT_GREYOUT);
        tft.fillCircle(32, 15, 3, TFT_GREYOUT);
        tft.fillTriangle(38, 14, 46, 8, 46, 22, TFT_GREYOUT);
        tft.fillTriangle(50, 14, 58, 8, 58, 22, TFT_GREYOUT);
        tuned = false;
      }
    }
    OStatusold = OStatus;
  }
}

void ShowBW() {
  if (BW != BWOld || BWreset == true) {
    tft.setTextColor(TFT_BLACK);
    tft.drawRightString(String (BWOld, DEC), 218, 6, 4);
    if (BWset == 0) tft.setTextColor(TFT_SKYBLUE); else tft.setTextColor(TFT_YELLOW);
    tft.drawRightString(String (BW, DEC), 218, 6, 4);
    BWOld = BW;
    BWreset = false;
  }
}

void ShowModLevel() {
  int segments;
  int color;
  int hold = 0;

  if (SQ != false) {
    MStatus = 0;
    MStatusold = 1;
  }

  if (MStatus != MStatusold || MStatus < 10) {
    for (segments = 0; segments < 13; segments++) {
      color = TFT_GREEN;
      if (segments > 8) color = TFT_ORANGE;
      if (segments > 9) color = TFT_RED;
      if (MStatus > (segments + 1) * 10)
      {
        hold = segments;
        tft.fillRect(20 + segments * 14, 139, 12, 8, color);
      } else {
        if (segments != peakholdold) tft.fillRect(20 + segments * 14, 139, 12, 8, TFT_GREYOUT);
      }
    }

    if (peakholdold < hold) peakholdold = hold;

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
    if (Squelch > 920) Squelch = 920;

    if (seek == false && menu == false && Squelch != Squelchold) {
      tft.setTextFont(2);
      tft.setTextColor(TFT_BLACK);
      tft.setCursor (216, 168);
      if (Squelchold == -100) {
        tft.print("OFF");
      } else if (Squelchold == 920) {
        tft.print("ST");
      } else {
        tft.print(Squelchold / 10);
      }
      tft.setTextColor(TFT_WHITE);
      tft.setCursor (216, 168);
      if (Squelch == -100) {
        tft.print("OFF");
      } else if (Squelch == 920) {
        tft.print("ST");
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
          tft.setCursor (216, 168);
          if (Squelchold == -1) tft.print("ST"); else tft.print(Squelchold / 10);
          tft.setTextColor(TFT_WHITE);
          tft.setCursor (216, 168);
          if (Squelch == -1) tft.print("ST"); else tft.print(Squelch / 10);
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
    if (BWset > 16) BWset = 0;

    switch (BWset) {
      case 1:
        radio.setFMBandw(56);
        break;

      case 2:
        radio.setFMBandw(64);
        break;

      case 3:
        radio.setFMBandw(72);
        break;

      case 4:
        radio.setFMBandw(84);
        break;

      case 5:
        radio.setFMBandw(97);
        break;

      case 6:
        radio.setFMBandw(114);
        break;

      case 7:
        radio.setFMBandw(133);
        break;

      case 8:
        radio.setFMBandw(151);
        break;

      case 9:
        radio.setFMBandw(168);
        break;

      case 10:
        radio.setFMBandw(184);
        break;

      case 11:
        radio.setFMBandw(200);
        break;

      case 12:
        radio.setFMBandw(217);
        break;

      case 13:
        radio.setFMBandw(236);
        break;

      case 14:
        radio.setFMBandw(254);
        break;

      case 15:
        radio.setFMBandw(287);
        break;

      case 16:
        radio.setFMBandw(311);
        break;
    }
  } else {
    if (BWset > 4) BWset = 1;

    switch (BWset) {
      case 1:
        radio.setAMBandw(3);
        break;

      case 2:
        radio.setAMBandw(4);
        break;

      case 3:
        radio.setAMBandw(6);
        break;

      case 4:
        radio.setAMBandw(8);
        break;
    }
  }
  updateBW();
  BWreset = true;
}

void doTuneMode() {
  if (band == 0) {
    if (tunemode == true) tunemode = false; else tunemode = true;
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
  if (USBstatus == true) tft.drawBitmap(272, 6, USBLogo, 43, 21, TFT_SKYBLUE); else tft.drawBitmap(272, 6, USBLogo, 43, 21, TFT_GREYOUT);
}

void XDRGTKRoutine() {
  if (Serial.available() > 0)
  {
    buff[buff_pos] = Serial.read();
    if (buff[buff_pos] != '\n' && buff_pos != 16 - 1) {
      buff_pos++;
    } else {
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
          Serial.print("T" + String(frequency * 10) + "A0\nD0\nG00\n");
          USBstatus = true;
          ShowUSBstatus();
          if (menu == true) ModeButtonPress();
          if (Squelch != Squelchold) {
            if (screenmute == false) {
              tft.setTextFont(2);
              tft.setTextColor(TFT_BLACK);
              tft.setCursor (240, 168);
              if (Squelchold == -100) tft.print("OFF"); else if (Squelchold > 920) tft.print("ST"); else tft.print(Squelchold / 10);
            }
          }
          break;

        case 'A':
          AGC = atol(buff + 1);
          Serial.print("A" + String(AGC) + "\n");
          radio.setAGC(AGC);
          break;

        case 'C':
          byte scanmethod;
          scanmethod = atol(buff + 1);
          if (seek == false) {
            if (scanmethod == 1) {
              Serial.print("C1\n");
              direction = true;
              seek = true;
              Seek(direction);
            }
            if (scanmethod == 2) {
              Serial.print("C2\n");
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
          Serial.print("D" + String(DeEmphasis) + "\n");
          radio.setDeemphasis(DeEmphasis);
          break;

        case 'F':
          XDRBWset = atol(buff + 1);
          if (XDRBWset < 16) {
            XDRBWsetold = XDRBWset;
            BWset = XDRBWset + 1;
          } else {
            XDRBWset = XDRBWsetold;
          }
          doBW();
          Serial.print("F" + String(XDRBWset) + "\n");
          break;

        case 'G':
          LevelOffset =  atol(buff + 1);
          if (LevelOffset == 0) {
            MuteScreen(0);
            LowLevelSet = EEPROM.readByte(43);
            EEPROM.commit();
            Serial.print("G00\n");
          }
          if (LevelOffset == 10) {
            MuteScreen(1);
            LowLevelSet = EEPROM.readByte(43);
            EEPROM.commit();
            Serial.print("G10\n");
          }
          if (LevelOffset == 1) {
            MuteScreen(0);
            LowLevelSet = 120;
            Serial.print("G01\n");
          }
          if (LevelOffset == 11) {
            LowLevelSet = 120;
            MuteScreen(1);
            Serial.print("G11\n");
          }
          break;

        case 'M':
          byte XDRband;
          XDRband = atol(buff + 1);
          if (XDRband == 0) {
            band = 0;
            SelectBand();
            Serial.print("M0\nT" + String(frequency * 10) + "\n");
          } else {
            band = 1;
            SelectBand();
            Serial.print("M1\nT" + String(frequency_AM) + "\n");
          }
          break;

        case 'T':
          unsigned int freqtemp;
          freqtemp = atoi(buff + 1);
          if (seek == true) seek = false;
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
              radio.setFrequency(frequency, 65, 108, fullsearchrds);
            }
          }
          if (band == 0) Serial.print("T" + String(frequency * 10) + "\n"); else Serial.print("T" + String(frequency_AM) + "\n");
          radio.clearRDS(fullsearchrds);
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
            radio.setFrequency(scanner_start, 65, 108, fullsearchrds);
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
              radio.setFrequency(freq_scan, 65, 108, fullsearchrds);
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
              tft.setTextColor(TFT_BLACK);
              tft.setCursor (90, 60);
              tft.print("SCANNING...");
            }
            radio.setFrequency(frequencyold, 65, 108, fullsearchrds);
            if (screenmute == false) ShowFreq(0);
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
          Serial.print("Y" + String(VolSet) + "\n");
          break;

        case 'X':
          Serial.print("X\n");
          ESP.restart();
          break;

        case 'Z':
          byte iMSEQX;
          iMSEQX = atol(buff + 1);
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
          Serial.print("Z" + String(iMSEQX) + "\n");
          break;
      }
    }
  }

  if (USBstatus == true) {
    Stereostatus = radio.getStereoStatus();
    if (StereoToggle == false) {
      Serial.print("SS");
    } else if (Stereostatus == true && band == 0) {
      Serial.print("Ss");
    } else {
      Serial.print("Sm");
    }
    if (SStatus > (SStatusold + 10) || SStatus < (SStatusold - 10)) Serial.print(((SStatus * 100) + 10875) / 1000); else Serial.print(((SStatusold * 100) + 10875) / 1000);
    Serial.print("," + String(WAM / 10, DEC) + "," + String(SNR, DEC) + "\n");
  }
}

void Seek(bool mode) {
  if (band == 0) {
    radio.setMute();
    if (mode == false) frequency = radio.tuneDown(stepsize, LowEdgeSet, HighEdgeSet, fullsearchrds); else frequency = radio.tuneUp(stepsize, LowEdgeSet, HighEdgeSet, fullsearchrds);
    delay(50);
    ShowFreq(0);
      if (USBstatus == true) if (band == 0) Serial.print("T" + String(frequency * 10) + "\n"); else Serial.print("T" + String(frequency_AM) + "\n");
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

void SetTunerPatch() {
  if (TEF != 101 && TEF != 102 && TEF != 205) {
    radio.init(102);
    uint16_t device;
    uint16_t hw;
    uint16_t sw;
    radio.getIdentification(device, hw, sw);
    TEF = highByte(hw) * 100 + highByte(sw);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    analogWrite(CONTRASTPIN, ContrastSet * 2 + 27);
    if (TEF == 0) tft.drawCentreString("Tuner not detected", 150, 70, 4); else tft.drawCentreString(String("Tuner version set: v") + String(TEF), 150, 70, 4);
    tft.drawCentreString("Please restart tuner", 150, 100, 4);
    EEPROM.writeByte(54, TEF);
    EEPROM.commit();
    while (true);
    for (;;);
  }
}

void read_encoder() {
  static uint8_t old_AB = 3;
  static int8_t encval = 0;
  static const int8_t enc_states[]  = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

  old_AB <<= 2;

  if (digitalRead(ROTARY_PIN_A)) old_AB |= 0x02;
  if (digitalRead(ROTARY_PIN_B)) old_AB |= 0x01;
  encval += enc_states[( old_AB & 0x0f )];

  if (optenc == 1) {
    if (encval > 2) {
      if (rotarymode == true) rotary = -1; else rotary = 1;
      encval = 0;
    } else if (encval < -2) {
      if (rotarymode == true) rotary = 1; else rotary = -1;
      encval = 0;
    }
  } else {
    if (encval > 3) {
      if (rotarymode == true) rotary = -1; else rotary = 1;
      encval = 0;
    } else if (encval < -3) {
      if (rotarymode == true) rotary = 1; else rotary = -1;
      encval = 0;
    }
  }
}
