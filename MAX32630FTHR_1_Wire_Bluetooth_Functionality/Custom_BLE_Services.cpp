#include <events/mbed_events.h>
#include <mbed.h>
#include <math.h>
#include "stdlib.h"
//#include "time.h"
#include "ble/BLE.h"
//#include "ble/Gap.h"
#include "USBSerial.h"
#include "OnewireBT.h"
#include "string"
#include "sstream"
//#include "Custom_Service.h"


#define TEMP_OUTPUT_FLAG    0x01
#define HIA_OUTPUT_FLAG     0x02
#define LOA_OUTPUT_FLAG     0x04
#define ALARMING_FLAG       0x08
#define CLK_OUTPUT_FLAG     0x10

#define BUFSIZE             70



/* Globals */

/* Serial Port */

USBSerial microUSB;
/* Service UUID's */
uint16_t TempServiceUUID = 0xDEAD;
uint16_t TimeServiceUUID = 0xBEEF;
uint16_t OutputUUID = 0xDEAF;

/* Characteristic UUID's */
uint16_t readTempUUID = 0x1600;
uint16_t rwHiAlarmUUID = 0x1601;
uint16_t rwLowAlarmUUID = 0x1602;
uint16_t getAlarmingStateUUID = 0x1603;

uint16_t readCurrTimeUUID = 0x1634;

uint16_t outStringUUID = 0x1678;

/* Device name and profile UUID */
const static char     DEVICE_NAME[] = "FTHR_BT_Demo";
static const uint16_t uuid16_list[] = {TempServiceUUID, TimeServiceUUID, OutputUUID};


/* Temperature Characteristics and Service */
static uint16_t readTemp = 0;
ReadOnlyGattCharacteristic<uint16_t> getTemp(readTempUUID, &readTemp);

static uint8_t HiAlarm = 100;
ReadWriteGattCharacteristic<uint8_t> accessHiAlarm(rwHiAlarmUUID, &HiAlarm);

static uint8_t LowAlarm = 0;
ReadWriteGattCharacteristic<uint8_t> accessLowAlarm(rwLowAlarmUUID, &LowAlarm);

static bool Alarming;
ReadOnlyGattCharacteristic<bool> currAlarmState(getAlarmingStateUUID, &Alarming);

GattCharacteristic *chars[] = {&getTemp, &accessHiAlarm, &accessLowAlarm, &currAlarmState};
static GattService TempService(TempServiceUUID, chars, 4);



/* Clock Service */
static uint32_t ClockVal = 0;
ReadOnlyGattCharacteristic<uint32_t> getCurrTime(readCurrTimeUUID, &ClockVal);

GattCharacteristic *timeChars[] = {&getCurrTime};
static GattService TimeService(TimeServiceUUID, timeChars, 1);



/* Output Service */
static char Output[BUFSIZE] = {0};
ReadOnlyArrayGattCharacteristic<char, BUFSIZE> showOutput(outStringUUID, Output);

GattCharacteristic *outChars[] = {&showOutput};
static GattService OutputService(OutputUUID, outChars, 1);

/* Data written flags */
uint8_t WriteData = 0;

/* Array for Scratchpad Data */
uint8_t ScratchData[9];

uint8_t output_flag = 0;


bool showConnectMessage = true;

static EventQueue eventQueue(/* event count */ 16 * EVENTS_EVENT_SIZE);

OneWire::array<uint8_t, 8> Temp_Rom_ID = {0x28, 0x07, 0xE9, 0x77, 0x0B, 0x00, 0x00, 0x5D};
OneWire::array<uint8_t, 8> Clock_Rom_ID = {0x24, 0x0A, 0x1D, 0x31, 0x00, 0x00, 0x00, 0x83};

DigitalOut rLED(LED1, LED_OFF);
DigitalOut gLED(LED2, LED_OFF);
DigitalOut bLED(LED3, LED_OFF);

MCU_OWM owm(false, true);

/* Functions */

template <typename T> string toString(const T& t) {
   std::ostringstream os;
   os<<t;
   return os.str();
}

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
    BLE::Instance().gap().startAdvertising(); // restart advertising
    microUSB.printf("Disconnected\r\n");
    showConnectMessage = true;
}

void updateTemp(){

}

void periodicCallback(void)
{
    rLED = !rLED; /* Do blinky on LED1 while we're waiting for BLE events */

    if (BLE::Instance().getGapState().connected && showConnectMessage) {
        //eventQueue.call(updateTemp);
        microUSB.printf("Connected... waiting for action\r\n");
        showConnectMessage = false;
    }
}

void onBleInitError(BLE &ble, ble_error_t error)
{
    (void)ble;
    (void)error;
   /* Initialization error handling should go here */
}

void printMacAddress()
{
    /* Print out device MAC address to the console*/
    Gap::AddressType_t addr_type;
    Gap::Address_t address;
    BLE::Instance().gap().getAddress(&addr_type, address);
    microUSB.printf("DEVICE MAC ADDRESS: ");
    for (int i = 5; i >= 1; i--){
        microUSB.printf("%02x:", address[i]);
    }
    microUSB.printf("%02x\r\n", address[0]);
}

void connectCallback(Gap::ConnectionEventCallback_t params)
{
    microUSB.printf("You are now connected\r\n");
}

/* Called anytime a characteristic is accessed */
void DataReceivedEvent(const GattReadCallbackParams *eventDataP){
    char charptr[10];
    uint8_t attribute = eventDataP->handle;
    microUSB.printf("Data Received\r\n");
    float Temp = 0;
    OneWireMaster::CmdResult result;
    RomId romId;
    RomCommands::SearchState ss;
    string holder;
    int i=0;
    int days, hours, minutes, seconds, temptime;
    string days_s, hours_s, minutes_s, seconds_s;

//    Output = charptr;
    memset(Output, 0, BUFSIZE);

    switch(attribute){
        /* Get and read temp */
        case 0x22:
            romId.buffer = Temp_Rom_ID;
            result = ConvertT(owm, 12);
            if(result != OneWireMaster::Success){
                microUSB.printf("Temp Conversion Failed\r\n");
                return;
            }
            result = ReadDeviceData(owm, ScratchData, romId);
            microUSB.printf("Read From ScratchPad: 0x");
            for(i=0; i<8; i++){
                microUSB.printf(" %02X", ScratchData[i]);
            }
            microUSB.printf("\r\n");
            readTemp = (uint16_t) getValue(ScratchData, 0, 1, attribute);
            microUSB.printf("temp in hex is 0x%04X\r\n", readTemp);
            output_flag = TEMP_OUTPUT_FLAG;
            break;

        /* Read Th */
        case 0x24:
            romId.buffer = Temp_Rom_ID;
            result = ReadDeviceData(owm, ScratchData, romId);
            if(result != OneWireMaster::Success){
                microUSB.printf("Reading DS18B20 Scratchpad Failed\r\n");
                return;
            }
            HiAlarm = (uint8_t) getValue(ScratchData, 2, 2, attribute);
            output_flag = HIA_OUTPUT_FLAG;
            microUSB.printf("The High Alarm is set at %d degrees C\r\n", HiAlarm);
            break;

        /* Read Tl */
        case 0x26:
            romId.buffer = Temp_Rom_ID;
            result = ReadDeviceData(owm, ScratchData, romId);
            if(result != OneWireMaster::Success){
                microUSB.printf("Reading DS18B20 Scratchpad Failed\r\n");
                return;
            }
            LowAlarm = (uint8_t) getValue(ScratchData, 3, 3, attribute);
            output_flag = LOA_OUTPUT_FLAG;
            microUSB.printf("The Low Alarm is set at %d degrees C\r\n", LowAlarm);
            break;

        /*Get Alarming State */
        case 0X28:
            result = OWAlarmSearch(owm,ss);
            if(result == OneWireMaster::Success){
                Alarming = true;
                microUSB.printf("There is an Alarm on the bus\r\n");

            }
            else{
                Alarming = false;
                microUSB.printf("No alarm condition detected\r\n");
            }
            strcpy(Output, holder.c_str());
            output_flag = ALARMING_FLAG;
            break;

        /* Get Time */
        case 0x32:
            romId.buffer = Clock_Rom_ID;
            result = ReadDeviceData(owm, ScratchData, romId);

            microUSB.printf("Read From ScratchPad: 0x");
            for(i=0; i<8; i++){
                microUSB.printf(" %02X", ScratchData[i]);
            }
            microUSB.printf("\r\n");

            if(result != OneWireMaster::Success){
                microUSB.printf("Reading DS1904 Scratchpad Failed\r\n");
                return;
            }
            ClockVal = getValue(ScratchData, 1, 5, attribute);
            microUSB.printf("Time from the Scratchpad: %d\r\n", ClockVal);
//            ClockVal = ClockVal/128;
            output_flag = CLK_OUTPUT_FLAG;
            break;

        case 0x42:
            switch(output_flag){
                case TEMP_OUTPUT_FLAG:
                    Temp = ((float)readTemp)/16;
                    microUSB.printf("The Temperature is %f degrees C\r\n", Temp);
                    holder = toString(Temp);
                    holder = "It is " + holder + " degrees C";
                    strcpy(Output, holder.c_str());
                    break;

                case HIA_OUTPUT_FLAG:
                    holder = toString((float) HiAlarm);
                    holder = "High alarm set to " + holder + " degrees C";
                    strcpy(Output, holder.c_str());
                    break;

                case LOA_OUTPUT_FLAG:
                    holder = toString( (float) LowAlarm);
                    holder = "Low Alarm set to " + holder + " degrees C";
                    strcpy(Output, holder.c_str());
                    break;

                case ALARMING_FLAG:
                    if(Alarming){
                        holder = "There is an active alarm on the bus";
                    }
                    else{
                        holder = "No active alarm detected";
                    }
                    strcpy(Output, holder.c_str());
                    break;

                case CLK_OUTPUT_FLAG:
                    temptime = ClockVal;
                    days = temptime/86400;
                    days_s = toString(days);
                    temptime = temptime % 86400;
                    hours = temptime / 3600;
                    hours_s = toString(hours);
                    temptime = temptime % 3600;
                    minutes = temptime / 60 ;
                    minutes_s = toString(minutes);
                    temptime = temptime % 60;
                    seconds = temptime;
                    seconds_s = toString(seconds);
                    holder = days_s + " days, " + hours_s + " hours, " +  minutes_s + " minutes, and " + seconds_s + " seconds since Jan 1, 1970";
                    strcpy(Output, holder.c_str());
                    break;
            }
            //microUSB.printf("The output string is: %s\r\n", Output);
            break;
        default:
            microUSB.printf("Data Received Error: Attribute val= 0x%02X\r\n", attribute);
            break;
    }
    microUSB.printf("--------------------------------------------\r\n");
}

void DataWrittenEvent(const GattWriteCallbackParams *eventDataP){
    microUSB.printf("Data Written Event\r\n");
    uint8_t attribute = eventDataP->handle;
    RomId romId;
    romId.buffer = Temp_Rom_ID;
    uint8_t toEnter[3];
    OneWireMaster::CmdResult result;
    int changeIdx;
    int i;

    /* Get Scratchpad data */
    result = ReadDeviceData(owm, ScratchData, romId);
    if(result != OneWireMaster::Success){
        microUSB.printf("Reading DS18B20 Scratchpad Failed\r\n");
        return;
    }

    /* Copy bytes of interest */
    for(i=0; i<3; i++){
        toEnter[i] = ScratchData[2+i];
    }

    switch(attribute){
        /* Th written */
        case 0x24:
            changeIdx = 0;
            break;
        /* Tl written */
        case 0x26:
            changeIdx = 1;
            break;
        case 0x28:
            changeIdx = 2;
        default:
            microUSB.printf("Whatever just happened shouldn't have happened\r\n");
            break;
        }

    toEnter[changeIdx] = eventDataP->data[0];
    result = writeScratchpad(owm, toEnter, romId);
    switch(changeIdx){
        case 0:
            microUSB.printf("High Alarm set to %d degrees C\r\n", eventDataP->data[0]);
            break;
        case 1:
            microUSB.printf("Low Alarm set to %d degrees C\r\n", eventDataP->data[0]);
            break;
        case 2:
            break;
    }
    microUSB.printf("--------------------------------------------\r\n");
}



void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE&        ble   = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        onBleInitError(ble, error);
        return;
    }

    if (ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        return;
    }

    /*Set up behavior for GATT */

    ble.gap().onDisconnection(disconnectionCallback);
    ble.gattServer().onDataRead(DataReceivedEvent);
    ble.gattServer().onDataWritten(DataWrittenEvent);

    /* Setup advertising. */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(1000); /* ms */

    ble.addService(TempService);
    ble.addService(TimeService);
    ble.addService(OutputService);
    ble.gap().startAdvertising();

    printMacAddress();
}



/* Called anytime the BLE stack has pending work */
void scheduleBleEventsProcessing(BLE::OnEventsToProcessCallbackContext* context) {
    BLE &ble = BLE::Instance();
    eventQueue.call(Callback<void()>(&ble, &BLE::processEvents));
}

int main()
{
    eventQueue.call_every(2000, periodicCallback);
    /* Initialize the Board */
    MAX32630FTHR feather;
    feather.init(MAX32630FTHR::VIO_3V3);

    /* Initialize OWM and other OWM related variables */
    OWM_Initialize(owm);

    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(scheduleBleEventsProcessing);
    ble.init(bleInitComplete);

    eventQueue.dispatch_forever();
    return 0;
}
