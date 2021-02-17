#include "TEF6686.h"

char* ptyLUT[51] = {
      "None",
      "News",
      "Current Affairs",
      "Information",
      "Sport",
      "Education",
      "Drama",
      "Culture",
      "Science",
      "Variable",
      "Pop Music",
      "Rock Music",
      "Easy Listening",
      "Light Classical",
      "SeriousClassical",
      "Other Music",
      "Weather",
      "Finance",
      "Childrens Prog",
      "Social Affairs",
      "Religious Talk",
      "Phone-In Talk",
      "Travel",
      "Leisure",
      "Jazz Music",
      "Country Music",
      "National Music",
      "Oldies Music",
      "Folk Music",
      "Documentary",
      "Emergency Test",
      "!!!ALERT!!!",
      "Current Affairs",
      "Education",
      "Drama",
      "Culture",
      "Science",
      "Varied Speech",
      "Easy Listening",
      "Light Classics",
      "Serious Classics",
      "Other Music",
      "Finance",
      "Children's Progs",
      "Social Affairs",
      "Phone In",
      "Travel & Touring",
      "Leisure & Hobby",
      "National Music",
      "Folk Music",
      "Documentary"};

uint8_t TEF6686::init() {
  int16_t bootstatus;
  Tuner_I2C_Init();
  getBootStatus(bootstatus);
  if (bootstatus == 0) {
    Tuner_Patch();
    delay(50);     
    if (digitalRead(15) == LOW) {Tuner_Init9216();} else {Tuner_Init4000();}
    power(1);
    Tuner_Init();
  }
}

void TEF6686::power(uint16_t mode) {
  devTEF_APPL_Set_OperationMode(mode);
}

void TEF6686::setFrequency(uint16_t frequency, uint16_t LowEdge, uint16_t HighEdge) {
  Radio_SetFreq(frequency, LowEdge, HighEdge);
}

uint16_t TEF6686::getFrequency() {
  return Radio_GetCurrentFreq();
}

void TEF6686::setOffset(int16_t offset) {
  devTEF_Radio_Set_LevelOffset(offset*10);
}

void TEF6686::setFMBandw(uint16_t bandwidth) {
  devTEF_Radio_Set_Bandwidth(0,bandwidth *10,1000,1000);
}
void TEF6686::setFMABandw() {
  devTEF_Radio_Set_Bandwidth(1,3110,1000,1000);
}

uint8_t TEF6686::getStereoStatus() {
  return Radio_CheckStereo();
}

uint16_t TEF6686::seekUp(uint16_t LowEdge, uint16_t HighEdge, uint16_t threshold) {
  return seek(1, LowEdge, HighEdge, threshold);
}

uint16_t TEF6686::setMono(uint16_t mono) {
  devTEF_Radio_Set_Stereo_Min(mono);
}

uint16_t TEF6686::seekDown(uint16_t LowEdge, uint16_t HighEdge, uint16_t threshold) {
  return seek(0, LowEdge, HighEdge, threshold);
}

uint16_t TEF6686::tuneUp(uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge) {
  return tune(1, stepsize, LowEdge, HighEdge);
}

uint16_t TEF6686::tuneDown(uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge) {
  return tune(0, stepsize, LowEdge, HighEdge);
}

void TEF6686::setVolume(uint16_t volume) {
  devTEF_Audio_Set_Volume(volume);
}

void TEF6686::setMute() {
  devTEF_Audio_Set_Mute(1);
}

void TEF6686::setUnMute() {
  devTEF_Audio_Set_Mute(0);
}

void TEF6686::setAGC(uint16_t start) {
  if (start == 0) {devTEF_Radio_Set_RFAGC(920);}
  if (start == 1) {devTEF_Radio_Set_RFAGC(900);}
  if (start == 2) {devTEF_Radio_Set_RFAGC(870);}
  if (start == 3) {devTEF_Radio_Set_RFAGC(840);}
}

void TEF6686::setDeemphasis(uint16_t timeconstant) {
  if (timeconstant == 0) {devTEF_Radio_Set_Deemphasis(500);}
  if (timeconstant == 1) {devTEF_Radio_Set_Deemphasis(750);}
  if (timeconstant == 2) {devTEF_Radio_Set_Deemphasis(0);}
}

void TEF6686::setStereoLevel(uint16_t start) {
  if (start == 0) {
    devTEF_Radio_Set_Stereo_Level(0,start*10,60);
    devTEF_Radio_Set_Stereo_Noise(0,240,200);
    devTEF_Radio_Set_Stereo_Mph(0,240,200);
  } else {
    devTEF_Radio_Set_Stereo_Level(3,start*10,60);
    devTEF_Radio_Set_Stereo_Noise(3,240,200);
    devTEF_Radio_Set_Stereo_Mph(3,240,200);
  }
}

void TEF6686::setHighCutOffset(uint16_t start) {
  if (start == 0) {
    devTEF_Radio_Set_Highcut_Level(0,start*10,300);
    devTEF_Radio_Set_Highcut_Noise(0,360,300);
    devTEF_Radio_Set_Highcut_Mph(0,360,300);
  } else {
    devTEF_Radio_Set_Highcut_Level(3,start*10,300);
    devTEF_Radio_Set_Highcut_Noise(3,360,300);
    devTEF_Radio_Set_Highcut_Mph(3,360,300);    
  }
}

void TEF6686::setHighCutLevel(uint16_t limit) {
    devTEF_Radio_Set_Highcut_Max(1,limit*100);
}

uint8_t TEF6686::getBootStatus(int16_t &bootstatus) {
  uint8_t result = devTEF_APPL_Get_Operation_Status(&bootstatus);
  return bootstatus;
}


uint8_t TEF6686::getStatus(int16_t &level, int16_t &USN, int16_t &WAM, int16_t &offset, int16_t &bandwidth, int16_t &modulation) {
  uint8_t result = devTEF_Radio_Get_Quality_Status(&level, &USN, &WAM, &offset, &bandwidth, &modulation);
  return level;
  return USN;
  return WAM;
  return bandwidth;
  return modulation;
}

uint8_t TEF6686::readRDS(uint16_t &rdsB,uint16_t &rdsC,uint16_t &rdsD,uint16_t &rdsErr) {
  uint8_t rdsBHigh, rdsBLow, rdsCHigh, rdsCLow, rdsDHigh, rdsDLow, isPsReady,rdsAHigh,rdsALow;

  uint16_t rdsStat, rdsA;
  uint16_t result = devTEF_Radio_Get_RDS_Data(&rdsStat, &rdsA, &rdsB, &rdsC, &rdsD, &rdsErr);

  bool dataAvailable = bitRead(rdsStat, 15); 
  bool dataLoss = bitRead(rdsStat, 14);
  bool dataType = bitRead(rdsStat, 13);
  bool groupVersion = bitRead(rdsStat, 12);
  bool sync = bitRead(rdsStat, 9);

  if (!dataAvailable) {
    return sync;
  }
  uint8_t errA = (rdsErr & 0b1100000000000000) >> 14;
  uint8_t errB = (rdsErr & 0b0011000000000000) >> 12;
  uint8_t errC = (rdsErr & 0b0000110000000000) >> 10;
  uint8_t errD = (rdsErr & 0b0000001100000000) >> 8;

  rdsAHigh = (uint8_t)(rdsA >> 8);
  rdsALow = (uint8_t)rdsA;
  rdsBHigh = (uint8_t)(rdsB >> 8);
  rdsBLow = (uint8_t)rdsB;
  rdsCHigh = (uint8_t)(rdsC >> 8);
  rdsCLow = (uint8_t)rdsC;
  rdsDHigh = (uint8_t)(rdsD >> 8);
  rdsDLow = (uint8_t)rdsD;
  uint8_t type = (rdsBHigh >> 4) & 15;

  if (errB <= 1) {
    uint8_t programType = ((rdsBHigh & 3) << 3) | ((rdsBLow >> 5) & 7);
    strcpy(rdsProgramType, (programType >= 0 && programType < 32) ? ptyLUT[programType] : "    PTY ERROR   ");
  }

  if (errA == 0) {
  char Hex[16]={ '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
  rdsProgramId[0]=Hex[(rdsA & 0xF000U) >> 12];
  rdsProgramId[1]=Hex[(rdsA & 0x0F00U) >> 8];
  rdsProgramId[2]=Hex[(rdsA & 0x00F0U) >> 4];
  rdsProgramId[3]=Hex[(rdsA & 0x000FU)];
  rdsProgramId[4]='\0';
  }

   if (type == 0) {
    uint8_t address = rdsBLow & 3;
    uint8_t errPs = errB > errD ? errB : errD;
    if (address >= 0 && address <= 3) {
      if (address < prevAddress) {
        psErrors = psErrors | 0x44444444;
        psAB = !psAB;
      }
      prevAddress = address;
      if (!bitRead(psCharIsSet, 7 - (psAB * 4 + address))  || errPs <= 1 || (bitRead(psErrors, 31 - (psAB * 16 + address * 4 + 2)) && !bitRead(psErrors, 31 - (psAB * 16 + address * 4)))) {
        bitWrite(psCharIsSet, 7 - (psAB * 4 + address), 1);
        bitWrite(psErrors, 31 - (psAB * 16 + address * 4), 0);
        bitWrite(psErrors, 31 - (psAB * 16 + address * 4 + 1), 0);
        if (errPs <= 1) {
          bitWrite(psErrors, 31 - (psAB * 16 + address * 4 + 2), 0);
          if (errPs == 1) {
            bitWrite(psErrors, 31 - (psAB * 16 + address * 4 + 3), 1);
          }
        }
        if (errPs == 0 || errPs == 2) {
          bitWrite(psErrors, 31 - (psAB * 16 + address * 4 + 3), 0);
        }

        if (rdsDHigh != '\0') {
          unsafePs[psAB][address * 2] = rdsDHigh;
        }  
        if (rdsDLow != '\0') {
          unsafePs[psAB][address * 2 + 1] = rdsDLow;
        }
      }    
      if ((psCharIsSet == 0xFF && strncmp(unsafePs[0], unsafePs[1], 8) == 0)  || (psAB ? (psErrors & 0xFFFF) == 0 : (psErrors & 0xFFFF0000) == 0)) {
        strncpy(rdsProgramService, unsafePs[psAB], 8);
        rdsProgramService[8] = '\0';
        rdsFormatString(rdsProgramService, 8);
        psCharIsSet = 0;
        psErrors = 0xFFFFFFFF;
      }
    } 
  } else if (type == 2 && errB == 0) {
      uint16_t addressRT = rdsBLow & 15;
      uint8_t ab = bitRead(rdsBLow, 4);
      uint8_t cr = 0;
      uint8_t len = 64;
      if (groupVersion == 0) {
        if (addressRT >= 0 && addressRT <= 15) {
          if (rdsCHigh != 0x0D) {
            rdsRadioText[addressRT*4] = rdsCHigh;
          }  else {
            len = addressRT * 4;
            cr = 1;
          }
        if (rdsCLow != 0x0D) {
          rdsRadioText[addressRT * 4 + 1] = rdsCLow;
        } else {
          len = addressRT * 4 + 1;
          cr = 1;
        }
        if (rdsDHigh != 0x0D) {
          rdsRadioText[addressRT * 4 + 2] = rdsDHigh;
        } else {
          len = addressRT * 4 + 2;
          cr = 1;
        }
        if (rdsDLow != 0x0D) {
          rdsRadioText[addressRT * 4 + 3] = rdsDLow;
        } else {
          len = addressRT * 4 + 3;
          cr = 1;
        }
      }
    } else {
      if (addressRT >= 0 && addressRT <= 7) {
        if (rdsDHigh != '\0') {
          rdsRadioText[addressRT * 2] = rdsDHigh;
        }  
        if (rdsDLow != '\0') {
          rdsRadioText[addressRT * 2 + 1] = rdsDLow;
        }
      }
    }
    if (cr) {
      for (uint8_t i = len; i < 64; i++) {
        rdsRadioText[i] = ' ';
      }
    }
    if (ab != rdsAb) {      
      for (uint8_t i = 0; i < 64; i++) {
        rdsRadioText[i] = ' ';
      }
      rdsRadioText[64] = '\0';     
      isRdsNewRadioText = 1;
    } else {
      isRdsNewRadioText = 0;
    }
    rdsAb = ab;
    rdsFormatString(rdsRadioText, 64);
  }
  return true;
  return rdsB;
  return rdsC;
  return rdsD;
  return rdsErr;
}
 
void TEF6686::getRDS(RdsInfo *rdsInfo) {
  strcpy(rdsInfo->programType, rdsProgramType);
  strcpy(rdsInfo->programId, rdsProgramId);
  strcpy(rdsInfo->programService, rdsProgramService);
  strcpy(rdsInfo->radioText, rdsRadioText);
}


void TEF6686::clearRDS() {
  strcpy(rdsProgramType, "");
  strcpy(rdsProgramId, "    ");
  strcpy(rdsProgramService, "        ");
  strcpy(rdsRadioText, "                                         ");
  psErrors = 0xFFFFFFFF;
  psCharIsSet = 0;
}

void TEF6686::rdsFormatString(char* str, uint16_t length) {  
  for (uint16_t i = 0; i < length; i++) {    
    if ((str[i] != 0 && str[i] < 32) || str[i] > 126 ) {
      str[i] = ' ';  
    }
  }
}

uint16_t TEF6686::seek(uint8_t up, uint16_t LowEdge, uint16_t HighEdge, uint16_t threshold) {
  uint16_t mode = 20;
  uint16_t startFrequency = Radio_GetCurrentFreq();
  while (true) {
    switch(mode){
      case 20:
        Radio_ChangeFreqOneStep(up, 0, LowEdge, HighEdge);

        Radio_SetFreq(Radio_GetCurrentFreq(), LowEdge, HighEdge);
        
        mode = 30;
        Radio_CheckStationInit();

        
      break;
      
      case 30:
        delay(20);
        Radio_CheckStation(threshold);
        if (Radio_CheckStationStatus() >= 90) {
          mode = 40;
        }   
      break;

      case 40:
        if (Radio_CheckStationStatus() == 90) {        
          mode = (startFrequency == Radio_GetCurrentFreq()) ? 50 : 20;
        }
        else if (Radio_CheckStationStatus() == 100) {
          mode = 50;
        }
      break;
      
      case 50:
        Radio_SetFreq(Radio_GetCurrentFreq(), LowEdge, HighEdge);
        return Radio_GetCurrentFreq();
    }
  }
  return 0;
}

uint16_t TEF6686::seekSync(uint8_t up, uint16_t LowEdge, uint16_t HighEdge, uint16_t threshold) {
  if (seekMode == 0) {  
    seekMode = 20;
    seekStartFrequency = Radio_GetCurrentFreq();
  }
  
  switch(seekMode) {
    case 20:
      Radio_ChangeFreqOneStep(up, 0, LowEdge, HighEdge);
      Radio_SetFreq(Radio_GetCurrentFreq(), LowEdge, HighEdge);
      seekMode = 30;
      Radio_CheckStationInit();

      return 0;
      
    case 30:
      delay(20);
      Radio_CheckStation(threshold);
      if (Radio_CheckStationStatus() >= 90) {seekMode = 40;}   
      return 0;

    case 40:
      if (Radio_CheckStationStatus() == 90) {
        seekMode = (seekStartFrequency == Radio_GetCurrentFreq()) ? 50 : 20;
      } else if (Radio_CheckStationStatus() == 100) {
        seekMode = 50;
      }
      return 0;
      
    case 50:
    seekMode = 0;
      Radio_SetFreq(Radio_GetCurrentFreq(), LowEdge, HighEdge);
    return 1;  
  }
  return 0;
} 

uint16_t TEF6686::tune(uint8_t up, uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge) {
  Radio_ChangeFreqOneStep(up, stepsize, LowEdge, HighEdge);
  Radio_SetFreq(Radio_GetCurrentFreq(), LowEdge, HighEdge);

  return Radio_GetCurrentFreq();
}
