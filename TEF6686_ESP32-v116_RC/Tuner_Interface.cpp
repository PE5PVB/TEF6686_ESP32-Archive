#include "TEF6686.h"
#include "Tuner_Patch_Lithio_V101_p119.h"
#include "Tuner_Patch_Lithio_V102_p224.h"
#include "Tuner_Patch_Lithio_V205_p512.h"
#include <Wire.h>

bool debug_tx = false;
bool debug_rx = false;

static const unsigned char tuner_init_tab[] = {
  7, 0x20, 0x0B, 0x01, 0x03, 0x98, 0x00, 0x00,
  5, 0x20, 0x14, 0x01, 0x00, 0x00,
  5, 0x20, 0x16, 0x01, 0x00, 0x00,
  7, 0x20, 0x17, 0x01, 0x00, 0x00, 0x03, 0xE8,
  5, 0x20, 0x20, 0x01, 0x00, 0x01,
  5, 0x20, 0x1F, 0x01, 0x01, 0xF4,
  9, 0x20, 0x2A, 0x01, 0x00, 0x02, 0x00, 0x96, 0x00, 0xDC,
  9, 0x21, 0x2A, 0x01, 0x00, 0x02, 0x00, 0x96, 0x00, 0xDC,
  7, 0x20, 0x2D, 0x01, 0x00, 0x00, 0x00, 0xC8,
  11, 0x20, 0x32, 0x01, 0x00, 0x3C, 0x00, 0x78, 0x00, 0xC8, 0x00, 0xC8,
  11, 0x20, 0x33, 0x01, 0x00, 0x00, 0x00, 0xFA, 0x00, 0x82, 0x01, 0xF4,
  9, 0x20, 0x36, 0x01, 0x00, 0x00, 0x01, 0x68, 0x01, 0x2C,
  7, 0x20, 0x37, 0x01, 0x00, 0x00, 0x0F, 0xA0,
  7, 0x20, 0x39, 0x01, 0x00, 0x00, 0x00, 0x64,
  7, 0x20, 0x3A, 0x01, 0x00, 0x00, 0x00, 0xAA,
  5, 0x20, 0x3B, 0x01, 0x00, 0x01,
  11, 0x20, 0x3C, 0x01, 0x00, 0x3C, 0x00, 0x78, 0x00, 0x64, 0x00, 0xC8,
  11, 0x20, 0x3D, 0x01, 0x00, 0x00, 0x00, 0xD2, 0x00, 0x5A, 0x01, 0xF4,
  11, 0x20, 0x46, 0x01, 0x01, 0xF4, 0x07, 0xD0, 0x00, 0xC8, 0x00, 0xC8,
  11, 0x20, 0x47, 0x01, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x78, 0x02, 0x9E,
  9, 0x20, 0x48, 0x01, 0x00, 0x00, 0x02, 0x58, 0x00, 0xF0,
  9, 0x20, 0x49, 0x01, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x8C,
  9, 0x20, 0x4A, 0x01, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x8C,
  7, 0x20, 0x4B, 0x01, 0x00, 0x00, 0x0F, 0xA0,
  5, 0x20, 0x56, 0x01, 0x03, 0x84,
  7, 0x30, 0x15, 0x01, 0x00, 0x80, 0x00, 0x01
};

static const unsigned char tuner_init_tab4000[] = {
  3, 0x14, 0x00, 0x01,
  2, 0xff, 50,
  9, 0x40, 0x04, 0x01, 0x00, 0x3D, 0x09, 0x00, 0x00, 0x00,
  5, 0x40, 0x05, 0x01, 0x00, 0x01,
  2, 0xff, 100,
};

static const unsigned char tuner_init_tab9216[] = {
  3, 0x14, 0x00, 0x01,
  2, 0xff, 50,
  9, 0x40, 0x04, 0x01, 0x00, 0x8C, 0xA0, 0x00, 0x00, 0x00,
  5, 0x40, 0x05, 0x01, 0x00, 0x01,
  2, 0xff, 100,
};

bool Tuner_WriteBuffer(unsigned char *buf, uint16_t len)
{
  Wire.beginTransmission(0x64);
  if (debug_tx == true) {
    Serial.println();
  }
  for (uint16_t i = 0; i < len; i++) {
    if (debug_tx == true) {
      if (buf[i] < 0x10) {
        Serial.print("0");
      }
      Serial.print(buf[i], HEX);
      Serial.print(" ");
    }
    Wire.write(buf[i]);
  }
  uint8_t r = Wire.endTransmission();
  delay(2);
  return (r == 0) ? 1 : 0;
}

bool Tuner_ReadBuffer(unsigned char *buf, uint16_t len)
{
  Wire.requestFrom(0x64, len);
  if (debug_rx == true) {
    Serial.println();
  }
  if (Wire.available() == len) {
    for (uint16_t i = 0; i < len; i++) {
      if (debug_rx == true) {
        if (buf[i] < 0x10) {
          Serial.print("0");
        }
        Serial.print(buf[i], HEX);
        Serial.print(" ");
      }
      buf[i] = Wire.read();
    }
    return 1;
  }
  return 0;
}

bool Tuner_Patch_Load(const unsigned char *pLutBytes, uint16_t size)
{
  unsigned char buf[24 + 1];
  uint16_t i, len;
  uint16_t r;
  buf[0] = 0x1b;

  while (size)
  {
    len = (size > 24) ? 24 : size;
    size -= len;

    for (i = 0; i < len; i++)
      buf[1 + i] = pLutBytes[i];

    pLutBytes += len;

    if (1 != (r = Tuner_WriteBuffer(buf, len + 1)))
    {
      break;
    }
  }
  return r;
}


bool Tuner_Table_Write(const unsigned char *tab)
{
  if (tab[1] == 0xff)
  {
    delay(tab[2]);
    return 1;
  } else {
    return Tuner_WriteBuffer((unsigned char *)&tab[1], tab[0]);
  }
}

void Tuner_Patch(byte TEF) {
  Wire.beginTransmission(0x64);
  Wire.write(0x1e);
  Wire.write(0x5a);
  Wire.write(0x01);
  Wire.write(0x5a);
  Wire.write(0x5a);
  Wire.endTransmission();
  delay(100);
  Wire.beginTransmission(0x64);
  Wire.write(0x1c);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100);
  Wire.beginTransmission(0x64);
  Wire.write(0x1c);
  Wire.write(0x00);
  Wire.write(0x74);
  Wire.endTransmission();
  if (TEF == 101) {
    Tuner_Patch_Load(pPatchBytes101, PatchSize101);
  } else if (TEF == 102) {
    Tuner_Patch_Load(pPatchBytes102, PatchSize102);
  } else if (TEF == 205) {
    Tuner_Patch_Load(pPatchBytes205, PatchSize205);
  }
  Wire.beginTransmission(0x64);
  Wire.write(0x1c);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(100);
  Wire.beginTransmission(0x64);
  Wire.write(0x1c);
  Wire.write(0x00);
  Wire.write(0x75);
  Wire.endTransmission();
  if (TEF == 101) {
    Tuner_Patch_Load(pLutBytes101, LutSize101);
  } else if (TEF == 102) {
    Tuner_Patch_Load(pLutBytes102, LutSize102);
  } else if (TEF == 205) {
    Tuner_Patch_Load(pLutBytes205, LutSize205);
  }
  Wire.beginTransmission(0x64);
  Wire.write(0x1c);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
}

void Tuner_I2C_Init() {
  Wire.begin();
  delay(5);
}

bool Tuner_Init(void) {
  uint16_t r;
  const unsigned char *p = tuner_init_tab;

  for (uint16_t i = 0; i < sizeof(tuner_init_tab); i += (p[i] + 1))
  {
    if (1 != (r = Tuner_Table_Write(p + i)))
      break;
  }
  return r;
}

bool Tuner_Init4000(void) {
  uint16_t r;
  const unsigned char *p = tuner_init_tab4000;

  for (uint16_t i = 0; i < sizeof(tuner_init_tab4000); i += (p[i] + 1))
  {
    if (1 != (r = Tuner_Table_Write(p + i)))
      break;
  }
  return r;
}

bool Tuner_Init9216(void) {
  uint16_t r;
  const unsigned char *p = tuner_init_tab9216;

  for (uint16_t i = 0; i < sizeof(tuner_init_tab9216); i += (p[i] + 1))
  {
    if (1 != (r = Tuner_Table_Write(p + i)))
      break;
  }
  return r;
}
