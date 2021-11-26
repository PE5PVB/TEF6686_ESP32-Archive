void Tuner_I2C_Init(void);
void Tuner_Patch(void);
uint8_t Tuner_Init(void);
uint8_t Tuner_Init4000(void);
uint8_t Tuner_Init9216(void);
unsigned char Tuner_WriteBuffer(unsigned char *buf, uint16_t len);
unsigned char Tuner_ReadBuffer(unsigned char *buf, uint16_t len);
