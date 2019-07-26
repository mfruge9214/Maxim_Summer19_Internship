#include "OnewireBT.h"
#include "OneWire.h"



using namespace OneWire;
using namespace RomCommands;


void OWM_Initialize(MCU_OWM owm)
{
    rLED = LED_ON;
    OneWireMaster::CmdResult result = owm.OWInitMaster();
    while(result != OneWireMaster::Success) {
        printf("Failed to init OWM...\r\n\r\n");
        result = owm.OWInitMaster();
        wait(0.5);
        rLED = !rLED;
    }
    rLED = LED_OFF;
    owm.OWSetLevel(OneWireMaster::NormalLevel);
    owm.OWSetSpeed(OneWireMaster::StandardSpeed);
}


OneWireMaster::CmdResult ConvertT(MCU_OWM & owm, int res)
{
    printf("Converting Temperature\r\n");
    RomId romId;
    int i;
    OneWireMaster::CmdResult result = OneWireMaster::OperationFailure;
    romId.buffer = Temp_Rom_ID;

    /* Find Sensor on bus */
    result = RomCommands::OWMatchRom(owm, romId);

    if(result != OneWireMaster::Success){
        printf("Match ROM Failed\r\n");
        return result;
    }

    /* Issue Strong Pullup and Convert T cmd */
    result = owm.OWWriteByteSetLevel(OW_CONVERT_T_CMD, OneWireMaster::StrongLevel);

    if(result != OneWireMaster::Success){
        printf("Convert CMD Failed\r\n");
        return result;
    }

    /* Pause execution for Converting */
    switch(res){
        case 9:
            wait(.1);
            break;
        case 10:
            wait(.2);
            break;
        case 11:
            wait(.4);
            break;
        case 12:
            wait(.8);
            break;
        default:
            printf("Improper Resolution\r\n");
            break;
    }

    /* Remove Strong PU */
    result = owm.OWSetLevel(OneWireMaster::NormalLevel);
    if(result != OneWireMaster::Success){
        printf("Strong PU Removal Failed\r\n");
        return result;
    }
    else{
        printf("Conversion Successful\r\n");
    }

    return result;
}


OneWireMaster::CmdResult ReadDeviceData(MCU_OWM & owm, uint8_t *array, RomId romId){
    OneWireMaster::CmdResult result = OneWireMaster::OperationFailure;
    int i = 0;
    int len;

    /* Determine command that needs to be issued */
    uint8_t cmd;
    if(romId.familyCode() == 0x28){
        cmd = OW_READ_SCRATCHPAD_CMD;
        len = 9;    // 8 for significant bytes in the scratchpad
    }
    else if(romId.familyCode() == 0x24){
        cmd = OW_READ_TIME_CMD;
        len = 5;    // 4 for significant clock bytes
    }

    /* Match Rom the Bus */
    result = RomCommands::OWMatchRom(owm, romId);
    if(result != OneWireMaster::Success){
        printf("Match ROM Failed\r\n");
        return result;
    }

    /* Issue cmd to the bus */
    result = owm.OWWriteByte(cmd);
    if(result != OneWireMaster::Success){
        printf("Read Data Command Failed\r\n");
        return result;
    }

    /* Read the data from the bus into array */
    for (i=0; i<len; i++){
        result = owm.OWReadByte(array[i]);
        if(result != OneWireMaster::Success){
            printf("Read Bus Failed\r\n");
            return result;
        }
    }
    return result;
}

uint32_t getValue(uint8_t *array, int firstIdx, int lastIdx, uint8_t handle){
    uint8_t len;
    int i;
    uint32_t byte;
    uint32_t value = 0;

    if(firstIdx == lastIdx){
        value = array[firstIdx];
        return value;
    }

    if(handle == 0x22){
        for(i=firstIdx; i<=lastIdx; i++){
            byte = array[i];
            value += (byte<<(8*i));
        }
    }
    else if(handle == 0x32){
        for(i=firstIdx; i<=lastIdx; i++){
            byte = array[i];
            value += (byte<<(8*i-1));
        }
    }
    return value;
}

OneWireMaster::CmdResult writeScratchpad(MCU_OWM & owm, uint8_t *array, RomId romId){
    OneWireMaster::CmdResult result = OneWireMaster::OperationFailure;
    int i;

    result = OWMatchRom(owm, romId);

    if(result != OneWireMaster::Success){
        printf("Match ROM Failed\r\n");
        return result;
    }

    result = owm.OWWriteByte(OW_WRITE_SCRATCHPAD_CMD);

    if(result != OneWireMaster::Success){
        printf("Command to Write Scratchpad Failed\r\n");
        return result;
    }

    for(i=0; i<3; i++){
        result = owm.OWWriteByte(array[i]);
        if(result != OneWireMaster::Success){
            printf("Writing Scratchpad Failed\r\n");
            return result;
        }
    }
    return result;
}

//float ReadTempScratchpad(MCU_OWM & owm)
//{
//    int i;
//    int len = 2;
//    uint8_t retArray [len];
//    uint8_t num;
//    float Temp = 0;
//    float holder = 0;
//    RomId romId;
//    OneWireMaster::CmdResult result = OneWireMaster::OperationFailure;
//
//    /* Get Rom Id */
//    for(i=0; i<ROM_ID_LEN; i++){
//        romId.buffer[i] = Temp_Rom_ID[(ROM_ID_LEN - 1) - i];
//    }
//
//    /* Search bus for RomId */
//    result = RomCommands::OWMatchRom(owm, romId);
//    if(result != OneWireMaster::Success){
//        printf("Match ROM Failed\r\n");
//        return result;
//    }
//
//    /* Issue the read Scratchpad Command */
//    result = owm.OWWriteByte(OW_READ_SCRATCHPAD_CMD);
//    if(result != OneWireMaster::Success){
//        printf("Issuing Read Scratchpad; Command Failed\r\n");
//        return result;
//    }
//
//    /* Read the data from the bus into an array */
//    for(i=0; i<len; i++){
//        result = owm.OWReadByte(retArray[i]);
//        if(result != OneWireMaster::Success){
//            printf("Read Bus Failed\r\n");
//            return result;
//        }
//    }
//
//    for(i=len-1; i>=0; i--){
//        num = retArray[i];
//        if(i==1 && num > 7){    // It is a negative number
//            /* Implement later */
//        }
//        else if(i==1 && num <=7){   // Its a positive number
//            holder = num * 16;
//        }
//        else if(i==0){
//            holder = retArray[i];
//            holder = holder/16;
//        }
//        Temp += holder;
//    }
//    return Temp;
//}
