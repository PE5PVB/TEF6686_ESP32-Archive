extern uint16_t Radio_GetCurrentFreq(void);
extern void Radio_ChangeFreqOneStep(uint8_t UpDown, uint8_t stepsize, uint16_t LowEdge, uint16_t HighEdge);
extern void Radio_SetFreq(uint16_t Freq, uint16_t LowEdge, uint16_t HighEdge);
uint16_t Radio_Get_Quality_Status(int16_t *level, int16_t *usn, int16_t *wam, int16_t *offset, int16_t *BW, int16_t *mod);
uint16_t Radio_Get_RDS_Data(uint32_t*rds_data);
uint8_t Radio_CheckStereo(void);
