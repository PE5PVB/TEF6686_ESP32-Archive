#define TEF6686_h

#include "Arduino.h"
#include "Tuner_Api.h"
#include "Tuner_Drv_Lithio.h"
#include "Tuner_Interface.h"


struct RdsInfo {
  char programType[17]; 
  char programService[9];
  char programServiceUnsafe[9];
  char programId[5];
  char radioText[65];
  bool newRadioText;
};

class TEF6686 {
  public:
    uint16_t getFrequency();
    uint8_t getStatus(int16_t &level, int16_t &USN, int16_t &WAM, int16_t &offset, int16_t &bandwidth, int16_t &modulation);
    uint16_t seekDown(uint16_t LowEdge, uint16_t HighEdge, uint16_t threshold);
    uint16_t seekSync(uint8_t up, uint16_t LowEdge, uint16_t HighEdge, uint16_t threshold);
    uint16_t seekUp(uint16_t LowEdge, uint16_t HighEdge, uint16_t threshold);
    uint16_t setMono(uint16_t mono);
    uint16_t tuneDown(uint16_t LowEdge, uint16_t HighEdge);
    uint16_t tuneUp(uint16_t LowEdge, uint16_t HighEdge);
    uint8_t getStereoStatus();
    uint8_t init();
    uint8_t readRDS(uint16_t  &rdsB,uint16_t  &rdsC,uint16_t  &rdsD,uint16_t  &rdsErr);
    void clearRDS();
    void getRDS(RdsInfo* rdsInfo);
    void powerOff();        
    void powerOn();
    void setAGC(uint16_t start);
    void setDeemphasis(uint16_t timeconstant);
    void setFMABandw();
    void setFMBandw(uint16_t bandwidth);
    void setFrequency(uint16_t frequency, uint16_t LowEdge, uint16_t HighEdge);
    void setHighCutLevel(uint16_t limit);
    void setHighCutOffset(uint16_t start);
    void setMute();
    void setOffset(int16_t offset);
    void setStereoLevel(uint16_t start);
    void setUnMute();
    void setVolume(uint16_t volume);
     
  private:
    bool psAB;
    char rdsProgramId[5];
    char rdsProgramService[9];
    char rdsProgramType[17];
    char rdsRadioText[65];
    char unsafePs[2][8];
    uint16_t seek(uint8_t up, uint16_t LowEdge, uint16_t HighEdge, uint16_t threshold);
    uint16_t seekMode;
    uint16_t seekStartFrequency;
    uint16_t tune(uint8_t up, uint16_t LowEdge, uint16_t HighEdge);
    uint32_t psErrors = 0xFFFFFFFF;
    uint8_t isRdsNewRadioText;
    uint8_t prevAddress = 3;uint8_t rdsAb;
    uint8_t psCharIsSet = 0;
    void rdsFormatString(char* str, uint16_t length);
};
