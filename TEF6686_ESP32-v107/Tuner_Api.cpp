#include "TEF6686.h"

static uint8_t CheckIfStep;
uint16_t Radio_CurrentFreq;
uint8_t Radio_CurrentStation;

void Radio_SetFreq(uint16_t Freq, uint16_t LowEdge, uint16_t HighEdge)
{
  if((Freq>HighEdge*100)||(Freq<LowEdge*100)){Freq = LowEdge*100;}
  Radio_CurrentFreq=Freq;
  devTEF_Radio_Tune_To(Freq);
}

void Radio_ChangeFreqOneStep(uint8_t UpDown, uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge )
{
  byte temp;
  if (stepsize == 0) {temp = 5;}
  if (stepsize == 1) {temp = 1;}
  if (stepsize == 2) {temp = 10;}
  if (stepsize == 3) {temp = 100;}
  if(UpDown==1) {
    Radio_CurrentFreq+=temp;
    if(Radio_CurrentFreq>HighEdge*100){Radio_CurrentFreq=LowEdge*100;}
  } else {
    Radio_CurrentFreq-=temp;  
    if(Radio_CurrentFreq<LowEdge*100){Radio_CurrentFreq=HighEdge*100;}   
  }
}

int16_t Radio_Get_Level()
{
  int16_t level;
  int16_t usn;
  int16_t wam;
  int16_t offset;
  int16_t BW;
  int16_t mod;
  devTEF_Radio_Get_Quality_Status(&level, &usn, &wam, &offset, &BW, &mod);
  return level;
  
}

uint16_t Radio_GetCurrentFreq(void)
{
   return Radio_CurrentFreq;
}

uint8_t Radio_CheckStereo(void)
{
  uint16_t status;
  uint8_t stereo = 0;
  if (1==devTEF_Radio_Get_Stereo_Status(&status)){stereo = ((status >> 15) & 1) ? 1 : 0;}  
  return stereo;
}

static int Radio_Get_Data(uint8_t *usn, uint8_t *wam, uint16_t *offset)
{
  if(1 == devTEF_Radio_Get_Quality_Data (usn,wam,offset))
  {
    return 1;
  }
  return 0;
}

static uint32_t Radio_Check_Timer;

void Radio_CheckStation(int16_t threshold)
{
  uint8_t usn, wam;
  uint16_t offset;
  switch(CheckIfStep)
  {
    case 10:
      CheckIfStep = 20;
    break;

    case 20:
      delay(4);        
      CheckIfStep=30;
    break;   

    case 30:
    case 31:
      if(Radio_Get_Level() < threshold)
      {
        CheckIfStep=90;
      }
      else{
        if(++CheckIfStep > 31)
        {
          CheckIfStep=40;
          delay(40);
        }
        else
          delay(5);
      }
      break;
      
    case 40:
      CheckIfStep = 90;

      if(1 == Radio_Get_Data(&usn,&wam,&offset))
      {
        if((usn<27)&&(wam < 23)&&(offset < 100))

        CheckIfStep = 100 ;
      }
      break;
      
        case 90:
    break; 
    
        case 100:
    break; 
    
        default:
    CheckIfStep = 90;
    break;        
  }
}

uint8_t Radio_CheckStationStatus(void)
{
  return CheckIfStep;
}

void Radio_CheckStationInit(void)
{
  CheckIfStep=10;
}
