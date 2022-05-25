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
    uint16_t getFrequency_AM();
    uint16_t tuneDown(uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge);
    uint16_t tuneUp(uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge);
    uint16_t tuneDown_AM(uint8_t stepsize);
    uint16_t tuneUp_AM(uint8_t stepsize);
    bool getStatus(int16_t &level, uint16_t &USN, uint16_t &WAM, int16_t &offset, uint16_t &bandwidth, uint16_t &modulation);
    bool getStatus_AM(int16_t &level, uint16_t &USN, uint16_t &WAM, int16_t &offset, uint16_t &bandwidth, uint16_t &modulation);
    bool getIdentification(uint16_t &device, uint16_t &hw_version, uint16_t &sw_version);
    bool getBootStatus(uint8_t &bootstatus);
    void setMono(uint8_t mono);
    bool getStereoStatus();
    uint8_t init(byte TEF);
    bool readRDS(uint16_t  &rdsB, uint16_t  &rdsC, uint16_t  &rdsD, uint16_t  &rdsErr);
    void clearRDS();
    void getRDS(RdsInfo* rdsInfo);
    void power(uint8_t mode);
    void setAGC(uint8_t start);
    void setiMS(uint16_t mph);
    void setEQ(uint16_t eq);
    void setDeemphasis(uint8_t timeconstant);
    void setFMABandw();
    void setFMBandw(uint16_t bandwidth);
    void setAMBandw(uint16_t bandwidth);
    void setFrequency(uint16_t frequency, uint16_t LowEdge, uint16_t HighEdge);
    void setFrequency_AM(uint16_t frequency);
    void setHighCutLevel(uint16_t limit);
    void setHighCutOffset(uint16_t start);
    void setMute();
    void setOffset(int16_t offset);
    void setStereoLevel(uint16_t start);
    void setUnMute();
    void setVolume(int16_t volume);

  private:
    bool psAB;
    char rdsProgramId[5];
    char rdsProgramService[9];
    char rdsProgramType[17];
    char rdsRadioText[65];
    char unsafePs[2][8];
    uint16_t tune(uint8_t up, uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge);
    uint16_t tune_AM(uint8_t up, uint8_t stepsize);
    uint32_t psErrors = 0xFFFFFFFF;
    uint8_t isRdsNewRadioText;
    uint8_t prevAddress = 3; uint8_t rdsAb;
    uint8_t psCharIsSet = 0;
    void rdsFormatString(char* str, uint16_t length);
};
