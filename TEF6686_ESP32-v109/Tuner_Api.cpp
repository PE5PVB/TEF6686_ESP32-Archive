#include "TEF6686.h"

uint16_t Radio_CurrentFreq;

void Radio_SetFreq(uint16_t Freq, uint16_t LowEdge, uint16_t HighEdge)
{
  if ((Freq > HighEdge * 100) || (Freq < LowEdge * 100)) {
    Freq = LowEdge * 100;
  }
  Radio_CurrentFreq = Freq;
  devTEF_Radio_Tune_To(Freq);
  devTEF_Radio_Set_RDS();
}

void Radio_ChangeFreqOneStep(uint8_t UpDown, uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge )
{
  byte temp;
  if (stepsize == 0) {
    temp = 5;
  }
  if (stepsize == 1) {
    temp = 1;
  }
  if (stepsize == 2) {
    temp = 10;
  }
  if (stepsize == 3) {
    temp = 100;
  }
  if (UpDown == 1) {
    Radio_CurrentFreq += temp;
    if (Radio_CurrentFreq > HighEdge * 100) {
      Radio_CurrentFreq = LowEdge * 100;
    }
  } else {
    Radio_CurrentFreq -= temp;
    if (Radio_CurrentFreq < LowEdge * 100) {
      Radio_CurrentFreq = HighEdge * 100;
    }
  }
}

uint16_t Radio_GetCurrentFreq(void)
{
  return Radio_CurrentFreq;
}

uint8_t Radio_CheckStereo(void)
{
  uint16_t status;
  uint8_t stereo = 0;
  if (1 == devTEF_Radio_Get_Stereo_Status(&status)) {
    stereo = ((status >> 15) & 1) ? 1 : 0;
  }
  return stereo;
}

static int Radio_Get_Data(uint8_t *usn, uint8_t *wam, uint16_t *offset)
{
  if (1 == devTEF_Radio_Get_Quality_Data (usn, wam, offset))
  {
    return 1;
  }
  return 0;
}
