extern uint16_t Radio_GetCurrentFreq(void);
extern void Radio_ChangeFreqOneStep(uint8_t UpDown, uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge);
extern void Radio_SetFreq(uint16_t Freq, uint16_t LowEdge, uint16_t HighEdge);
bool Radio_CheckStereo(void);
