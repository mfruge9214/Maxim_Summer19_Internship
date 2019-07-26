#include <events/mbed_events.h>
#include <mbed.h>
#include <math.h>
#include "ble/BLE.h"
//#include "ble/Gap.h"
#include "OnewireBT.h"
//#include "Custom_Service.h"



/* Globals */

/* Service UUID's */
uint16_t TempServiceUUID = 0xDEAD;
uint16_t TimeServiceUUID = 0xBEEF;

/* Characteristic UUID's */
uint16_t readTempUUID = 0x1600;
uint16_t rwHiAlarmUUID = 0x1601;
uint16_t rwLowAlarmUUID = 0x1602;
uint16_t getAlarmingStateUUID = 0x1603;

uint16_t readCurrTimeUUID = 0x1634;

/* Device name and profile UUID */
const static char     DEVICE_NAME[] = "MyProfile";
static const uint16_t uuid16_list[] = {TempServiceUUID, TimeServiceUUID};


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

/* Array for Scratchpad Data */
uint8_t ScratchData[9];

/* Clock Service */
static uint32_t ClockVal = 0;
ReadOnlyGattCharacteristic<uint32_t> getCurrTime(readCurrTimeUUID, &ClockVal);

GattCharacteristic *timeChars[] = {&getCurrTime};
static GattService TimeService(TimeServiceUUID, timeChars, 1);

/* Data written flags */
uint8_t WriteData = 0;

bool showConnectMessage = true;

static EventQueue eventQueue(/* event count */ 16 * EVENTS_EVENT_SIZE);

OneWire::array<uint8_t, 8> Temp_Rom_ID = {0x28, 0x07, 0xE9, 0x77, 0x0B, 0x00, 0x00, 0x5D};
OneWire::array<uint8_t, 8> Clock_Rom_ID = {0x24, 0x0A, 0x1D, 0x31, 0x00, 0x00, 0x00, 0x83};

DigitalOut rLED(LED1, LED_OFF);
DigitalOut gLED(LED2, LED_OFF);
DigitalOut bLED(LED3, LED_OFF);

MCU_OWM owm(false, true);

/* Functions */

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
    BLE::Instance().gap().startAdvertising(); // restart advertising
    printf("Disconnected\r\n");
    showConnectMessage = true;
}

void updateTemp(){
//    currTemp = currTemp + 1;
//
//    if(currTemp >100){
//        printf("Oh Jeez Rick, it's really hot in here!\r\n");
//        currTemp = 0;
//    }

//    envServicePtr->updateTemperature(gTemp);
//    printf("%f\r\n", gTemp);
}

void periodicCallback(void)
{
    rLED = !rLED; /* Do blinky on LED1 while we're waiting for BLE events */

    if (BLE::Instance().getGapState().connected && showConnectMessage) {
        //eventQueue.call(updateTemp);
        printf("Connected... waiting for action\r\n");
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
    printf("DEVICE MAC ADDRESS: ");
    for (int i = 5; i >= 1; i--){
        printf("%02x:", address[i]);
    }
    printf("%02x\r\n", address[0]);
}

void connectCallback(Gap::ConnectionEventCallback_t params)
{
    printf("You are now connected\r\n");
}

/* Called anytime a characteristic is accessed */
void DataReceivedEvent(const GattReadCallbackParams *eventDataP){
    uint8_t attribute = eventDataP->handle;
    printf("Data Received\r\n");
    float Temp = 0;
    uint8_t alarmVal;
    OneWireMaster::CmdResult result;
    RomId romId;
    RomCommands::SearchState ss;

    switch(attribute){
        /* Get and read temp */
        case 0x22:
            romId.buffer = Temp_Rom_ID;
            result = ConvertT(owm, 12);
            if(result != OneWireMaster::Success){
                printf("Temp Conversion Failed\r\n");
                return;
            }
            result = ReadDeviceData(owm, ScratchData, romId);
            int i=0;
            printf("Read From ScratchPad: 0x");
            for(i=0; i<8; i++){
                printf(" %02X", ScratchData[i]);
            }
            printf("\r\n");
            readTemp = (uint16_t) getValue(ScratchData, 0, 1, attribute);
            printf("temp in hex is 0x%04X\r\n", readTemp);
            Temp = ((float)readTemp)/16;
            printf("The Temperature is %f degrees C\r\n", Temp);
            break;

        /* Read Th */
        case 0x24:
            romId.buffer = Temp_Rom_ID;
            result = ReadDeviceData(owm, ScratchData, romId);
            if(result != OneWireMaster::Success){
                printf("Reading DS18B20 Scratchpad Failed\r\n");
                return;
            }
            HiAlarm = (uint8_t) getValue(ScratchData, 2, 2, attribute);
            printf("The High Alarm is set at %d degrees C\r\n", HiAlarm);
            break;

        /* Read Tl */
        case 0x26:
            romId.buffer = Temp_Rom_ID;
            result = ReadDeviceData(owm, ScratchData, romId);
            if(result != OneWireMaster::Success){
                printf("Reading DS18B20 Scratchpad Failed\r\n");
                return;
            }
            LowAlarm = (uint8_t) getValue(ScratchData, 3, 3, attribute);
            printf("The Low Alarm is set at %d degrees C\r\n", LowAlarm);
            break;

        /*Get Alarming State */
        case 0X28:
            result = OWAlarmSearch(owm,ss);
            if(result == OneWireMaster::Success){
                Alarming = true;
                printf("There is an Alarm on the bus\r\n");
            }
            else{
                Alarming = false;
                printf("No alarm condition detected\r\n");
            }
            break;

        /* Get Time */
        case 0x32:
            romId.buffer = Clock_Rom_ID;
            result = ReadDeviceData(owm, ScratchData, romId);
            if(result != OneWireMaster::Success){
                printf("Reading DS1904 Scratchpad Failed\r\n");
                return;
            }
            ClockVal = getValue(ScratchData, 1, 5, attribute);
            printf("The Clock has a value of 0x%04X\r\n", ClockVal);
            break;
        default:
            printf("Data Received Error: Attribute val= 0x%02X\r\n", attribute);
            break;
    }
    printf("--------------------------------------------\r\n");
}

void DataWrittenEvent(const GattWriteCallbackParams *eventDataP){
    printf("Data Written Event\r\n");
    uint8_t attribute = eventDataP->handle;
    RomId romId;
    romId.buffer = Temp_Rom_ID;
    uint8_t toEnter[3];
    OneWireMaster::CmdResult result;
    int changeIdx;
    uint8_t dataWritten;
    int i;

    /* Get Scratchpad data */
    result = ReadDeviceData(owm, ScratchData, romId);
    if(result != OneWireMaster::Success){
        printf("Reading DS18B20 Scratchpad Failed\r\n");
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
            printf("Whatever just happened shouldn't have happened\r\n");
            break;
        }

    toEnter[changeIdx] = eventDataP->data[0];
    result = writeScratchpad(owm, toEnter, romId);
    switch(changeIdx){
        case 0:
            printf("High Alarm set to %d degrees C\r\n", eventDataP->data[0]);
            break;
        case 1:
            printf("Low Alarm set to %d degrees C\r\n", eventDataP->data[0]);
            break;
        case 2:
            break;
    }
    printf("--------------------------------------------\r\n");
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

    ble.gap().onDisconnection(disconnectionCallback);
    ble.gattServer().onDataRead(DataReceivedEvent);
    ble.gattServer().onDataWritten(DataWrittenEvent);

//    /* Setup primary service. */
//    envServicePtr = new EnvironmentalService(ble);
//
//    envServicePtr->updateHumidity( (EnvironmentalService::HumidityType_t) 50);
//    envServicePtr->updatePressure( (EnvironmentalService::PressureType_t) 1);

    /* Setup advertising. */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
//    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::GENERIC_THERMOMETER);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(1000); /* ms */

    ble.addService(TempService);
    ble.addService(TimeService);
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
//    OneWire::MCU_OWM owm;
    //owm(false, true);
    OWM_Initialize(owm);

    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(scheduleBleEventsProcessing);
    ble.init(bleInitComplete);

    eventQueue.dispatch_forever();
    return 0;
}
