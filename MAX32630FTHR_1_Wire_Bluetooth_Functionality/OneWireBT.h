#include "max32630fthr.h"
#include "OneWire.h"

#define OW_CONVERT_T_CMD        0x44
#define OW_READ_SCRATCHPAD_CMD  0xBE
#define OW_WRITE_SCRATCHPAD_CMD 0x4E
#define OW_READ_TIME_CMD        0x66
#define OW_SCRATCHPAD_LEN       9
#define ROM_ID_LEN              8


using namespace OneWire;

/* Define Globals */
extern OneWire::array<uint8_t, 8> Temp_Rom_ID;
extern OneWire::array<uint8_t, 8> Clock_Rom_ID;

extern DigitalOut rLED;
extern DigitalOut gLED;
extern DigitalOut bLED;

extern MCU_OWM owm;

//OneWire::MCU_OWM owm;

void OWM_Initialize(MCU_OWM owm);
OneWireMaster::CmdResult ConvertT(MCU_OWM & owm, int res);
OneWireMaster::CmdResult ReadDeviceData(MCU_OWM & owm, uint8_t *array, RomId romId);
uint32_t getValue(uint8_t *array, int firstIdx, int lastIdx, uint8_t handle);
OneWireMaster::CmdResult writeScratchpad(MCU_OWM & owm, uint8_t *array, RomId romId);
