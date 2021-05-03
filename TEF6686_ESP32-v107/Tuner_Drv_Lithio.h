typedef enum
{ TEF_FM    = 32,
  TEF_AUDIO = 48,
  TEF_APPL  = 64
} TEF_MODULE;

typedef enum
{ Cmd_Tune_To             =  1,
  Cmd_Set_Bandwidth       = 10,
  Cmd_Set_RFAGC           = 11,
  Cmd_Set_MphSuppression  = 20,
  Cmd_Set_ChannelEqualizer    = 22,
  Cmd_Set_Deemphasis      = 31,
  Cmd_Set_LevelOffset     = 39,
  Cmd_Set_Highcut_Level   = 52,
  Cmd_Set_Highcut_Noise   = 53,
  Cmd_Set_Highcut_Mph     = 54,
  Cmd_Set_Highcut_Max     = 55,
  Cmd_Set_Stereo_Level    = 62,
  Cmd_Set_Stereo_Noise    = 63,
  Cmd_Set_Stereo_Mph      = 64,
  Cmd_Set_Stereo_Min      = 66,
  Cmd_Get_Quality_Status  = 128,
  Cmd_Get_Quality_Data    = 129,
  Cmd_Get_RDS_Data        = 131,
  Cmd_Get_Signal_Status   = 133,
} TEF_RADIO_COMMAND;

typedef enum
{ Cmd_Set_Volume = 10,
  Cmd_Set_Mute = 11,
} TEF_AUDIO_COMMAND;

typedef enum
{
  Cmd_Set_OperationMode = 1,
  Cmd_Get_Operation_Status = 128
} TEF_APPL_COMMAND;

uint8_t devTEF_Set_Cmd(TEF_MODULE module, uint8_t cmd, uint16_t len,...);
uint8_t devTEF_Radio_Tune_To (uint16_t frequency );
uint8_t devTEF_Radio_Get_Quality_Status (int16_t *level, int16_t *usn, int16_t *wam, int16_t *offset,int16_t *bandwidth,int16_t *mod);
uint8_t devTEF_Radio_Get_Quality_Data (uint8_t *usn,uint8_t *wam,uint16_t *offset);
uint8_t devTEF_APPL_Get_Operation_Status(int16_t *bootstatus);
uint8_t devTEF_Audio_Set_Mute(uint16_t mode);
uint8_t devTEF_Audio_Set_Volume(int16_t volume);
uint8_t devTEF_Radio_Get_Stereo_Status(uint16_t *status);
uint8_t devTEF_APPL_Set_OperationMode(uint16_t mode);
uint8_t devTEF_Radio_Get_RDS_Data(uint16_t *status,uint16_t *A_block,uint16_t *B_block,uint16_t *C_block,uint16_t *D_block, uint16_t *dec_error);
uint8_t devTEF_Radio_Set_Bandwidth(uint16_t mode,uint16_t bandwidth,uint16_t control_sensitivity,uint16_t low_level_sensitivity);
uint8_t devTEF_Radio_Set_LevelOffset(int16_t offset);
uint8_t devTEF_Radio_Set_Stereo_Level(uint16_t mode,uint16_t start,uint16_t slope);
uint8_t devTEF_Radio_Set_Stereo_Noise(uint16_t mode,uint16_t start,uint16_t slope);
uint8_t devTEF_Radio_Set_Stereo_Mph(uint16_t mode,uint16_t start,uint16_t slope);
uint8_t devTEF_Radio_Set_Stereo_Min(uint16_t mode);
uint8_t devTEF_Radio_Set_MphSuppression(uint16_t mph);
uint8_t devTEF_Radio_Set_ChannelEqualizer(uint16_t eq);
uint8_t devTEF_Radio_Set_RFAGC(uint16_t start);
uint8_t devTEF_Radio_Set_Deemphasis(uint16_t timeconstant);
uint8_t devTEF_Radio_Set_Highcut_Max(uint16_t mode,uint16_t limit);
uint8_t devTEF_Radio_Set_Highcut_Level(uint16_t mode,uint16_t start,uint16_t slope);
uint8_t devTEF_Radio_Set_Highcut_Noise(uint16_t mode,uint16_t start,uint16_t slope);
uint8_t devTEF_Radio_Set_Highcut_Mph(uint16_t mode,uint16_t start,uint16_t slope);
