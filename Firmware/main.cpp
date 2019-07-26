/*******************************************************************************
* Copyright (C) Maxim Integrated Products, Inc., All rights Reserved.
*
* This software is protected by copyright laws of the United States and
* of foreign countries. This material may also be protected by patent laws
* and technology transfer regulations of the United States and of foreign
* countries. This software is furnished under a license agreement and/or a
* nondisclosure agreement and may only be used or reproduced in accordance
* with the terms of those agreements. Dissemination of this information to
* any party or parties not specified in the license agreement and/or
* nondisclosure agreement is expressly prohibited.
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/



/* Included Libraries */
#include "mbed.h"
#include "OneWire.h"
#include "max32630fthr.h"
#include "USBSerial.h"
#include "string"
#include "sstream"
#include "vector"


/* Define Constants */
#define BUF_SIZE_BYTES   1024

#define STRING_DATA_BEGIN   3
#define BYTES_IN_ROM_ID     8
#define SCRATCHPAD_WRITE_BYTES  3

#define ALARM_SEARCH_CMD    0xEC

/* Define the Flags */
#define USB_RECEIVED_FLAG   0x00000002

using namespace OneWire;
using namespace RomCommands;
using namespace std;


/****************** Global Variables ******************/

float VersionNumber = 1.01;
/* Set up button for measurement */
//InterruptIn Int_button(SW1);

/* Buffer for what is read in from Serial */
string InitComp = "INIT";
string ReadBuffer = "";
bool StringReceived = false;
bool OutputRomID;
int CmdData;


/* Flag variable used for interrupts */
uint32_t int_flag;
uint32_t cmd_flag;

/* Set up and configure lights */
DigitalOut rLED(LED1, LED_OFF);
DigitalOut gLED(LED2, LED_OFF);
DigitalOut bLED(LED3, LED_OFF);

/* Set up virtual Serial Port */
USBSerial microUSB;

/* One Wire Master pointer and CmdResult */
MCU_OWM* owm_ptr;
vector<RomId> RomIDs;
int DeviceCount;
int mostRecentIdx;
SearchState search_state;

OneWireMaster::OWLevel NextLevel;
OneWireMaster::OWLevel CurrLevel;
OneWireMaster::OWSpeed NextSpeed;
OneWireMaster::OWSpeed CurrSpeed;


/* Error Messages */
string ROM_Error = "Not a recognized ROM command";
string Speed_Error = "Not a recognized Speed command";
string Pullup_Error = "Cannot configure Pullup";
string RW_Error = "Cannot parse R/W string";


/************** Function Definitions ***************/

/**
* @brief    Sets flag for main to process DataIn from USB Serial Port
* @version  1.0
* @post     Sets bit in int_flag
*****************************************************************************/
void USB_Received();

/**
* @brief    Processes the data recieved through the serial port and calls the corresponding One Wire functions
* @param[in] cmd: The string containing the data read from the serial port
* @param[in] owm: Pointer to the OneWireMaster object that preforms the operations
* @version  1.0
* @notes    Requires 'string' and 'OneWire.h'
* @pre      owm must be initialized and cmd must not be 'null'
* @post     Depending on specific contents of cmd, could change:
                 OutputRomID, RomIDs, search_state, NextLevel, CurrLevel, NextSpeed, CurrSpeed
*****************************************************************************/
void Parse_and_Execute(string cmd, MCU_OWM* owm);

/**
* @brief    Converts a string to a uint8_t
* @version  1.0
* @param[in] _string: String containing characters '0'-'9' to be converted
* @param[out] The equivalent value of the string
*****************************************************************************/
uint8_t StringtoDecimal(string _string);

/**
* @brief    Initializes the owm to be used, cycles red LED if not able to initialize
* @version  1.0
* @param[in] owm: OneWireMaster structure to be initialized
*****************************************************************************/
void OWM_Initialize(MCU_OWM owm);

/**
* @brief    Searches the bus for any one wire devices on the bus
* @version  1.0
* @param[in/out] owm: Reference to owm to conduct the search
* @param[in/out] search_state: SearchState structure to use to conduct the search
* @param[out] Returns the result of the command
* @post     Populates the RomIDs vector with the RomID of every device on the bus
* @notes    Requires 'OneWire.h'
*****************************************************************************/
OneWireMaster::CmdResult FindAllRomIDs(MCU_OWM & owm, SearchState &search_state);

/**
* @brief    Conducts an AlarmSearch on the devices on the bus
* @version  1.0
* @param[in/out] owm: Reference to owm to conduct the search
* @param[in/out] search_state: SearchState structure to use to conduct the search
* @param[out] Returns the result of the command
* @notes    Requires  'OneWire.h'
*****************************************************************************/
OneWireMaster::CmdResult AlarmSearchBus(MCU_OWM &owm, SearchState &search_state);



int main()
{
    /* Initialize the Board */
    MAX32630FTHR feather;
    feather.init(MAX32630FTHR::VIO_3V3);

    /* Initialize OWM and other OWM related variables */
    MCU_OWM owm(false, true);
    OWM_Initialize(owm);
    NextLevel = OneWireMaster::NormalLevel;
    NextSpeed = OneWireMaster::StandardSpeed;
    owm.OWSetLevel(NextLevel);
    owm.OWSetSpeed(NextSpeed);
    CurrLevel = NextLevel;
    CurrSpeed = NextSpeed;
    search_state.reset();
//    owm_ptr = &owm;
    DeviceCount = 0;

    bool InitReceived = false;

//    /* Initialize Button Interrupt */
//    Int_button.rise(&Button_ISR);

    /* Initialize Serial Interface */
    microUSB.attach(&USB_Received);

    /* Temporary Variable Storage */
    volatile char BufIn;
//  char BufOut;

//  static int count = 0;

    while(1) {
        if(!int_flag) {
            // Go to sleep
        }

        /* Get from USB into Buffer */
        if(int_flag & USB_RECEIVED_FLAG) {
            int_flag &= !(USB_RECEIVED_FLAG);

            while(microUSB.readable()) {
                BufIn = microUSB.getc();
                if(BufIn == '\r' || BufIn == '\0' || BufIn == '\n') {
                    StringReceived = true;
                    gLED = !gLED;
                    /* Detects the GUI initialization, resets the Firmware to keep it in sync */
                    if(ReadBuffer == InitComp){
                        microUSB.printf("Version %f\r\n", VersionNumber);
                        InitReceived = true;
                        owm.OWSetLevel(OneWireMaster::NormalLevel);
                        owm.OWSetSpeed(OneWireMaster::StandardSpeed);
                        search_state.reset();
                        RomIDs.clear();
                        gLED = LED_ON;
                        rLED = LED_OFF;
                        bLED = LED_OFF;
                    }
                }
                if(!StringReceived) {
                    ReadBuffer += BufIn;
                }
            }

            /* String is now in Buffer */
            StringReceived = false;
//            microUSB.printf("Command Received: '%s' \n\r", ReadBuffer);
            if(!InitReceived){
                Parse_and_Execute(ReadBuffer, &owm);
            }
            ReadBuffer ="";
            bLED = !bLED;
            InitReceived = false;
        }
    }

    return 0;
}


/***************** Functions ********************/

void USB_Received()
{
    int_flag |= USB_RECEIVED_FLAG;
}

void Parse_and_Execute(string cmd, MCU_OWM* _owm)
{
    /* Variable Declarations */
    string::iterator i;
    string temp = "";
    string c;
    char ch;
    string RomIDstr;
    string CmdResponse;
    uint8_t num;
    uint32_t word;
    char* strend;
    long int hex_val;
    int j;
    int k;
    bool Read_Ret = false;
    bool RomID_Ret = false;
    uint8_t Scratchpad_Data [SCRATCHPAD_WRITE_BYTES];
    uint8_t ReadBytes[BUF_SIZE_BYTES];
    uint8_t ReadCount = 0;
    RomId romId;
    bool AddROM = false;
    bool AlarmSearch = false;
    size_t strpos = 0;
    vector<bool> RomIDMatches(RomIDs.size(), true);

    MCU_OWM* owm = _owm;
    OneWireMaster::CmdResult result;

    /* Initialize result to Fail */
    result = OneWireMaster::OperationFailure;

    i=cmd.begin();
/* Decode the commands based on 'Family Code' and parse the data */
    switch(*i)
    {
        /* ROM commands */
        case 'R':
            //microUSB.printf("1: ROM command \r\n");
            i++;
            switch(*i)
            {
                case 'M':
//                    microUSB.printf("2: Match ROM command \r\n");
                    i++;
                    if( *i == 'T')
                    {
                        RomIDstr = cmd.substr(3);       // Obtains the string after the 3 CMD characters (The ROM ID we are looking for)
                        if(RomIDstr == ""){
                            microUSB.printf("Please select a Rom ID from the Devices on the Bus\r\n");
                            break;
                        }

                        for(j=0; j<BYTES_IN_ROM_ID; j++){
                            strpos = 2*j;
                            temp = RomIDstr.substr(strpos, 2);
                            num = strtol(temp.c_str(), &strend, 16);
                            for(k=0; k<RomIDs.size(); k++){
                                // First, check to make sure that the RomID we are checking still matches
                                if(!RomIDMatches[k]){
                                    break;
                                }
                                // If the byte in num does not match the byte in the RomID, set the corresponding index in 'matches' to false
                                if(num != RomIDs[k].buffer[(BYTES_IN_ROM_ID - 1) - j]){
                                    RomIDMatches[k] = false;
                                    break;
                                }
                            }
                        }
                        j=0;    // Reset the loop variables
                        k=0;
                        // Now, there should only be 1 or 0 'true' values in RomIDMatches, so use that index for the MatchROM
                        while(!RomIDMatches[j] && j<RomIDs.size()){
                            j++;
                        }
                        // Now we have determined that the jth index of the RomIDs vector contains the RomID given by the user,
                        romId = RomIDs[j];
                        result = OWMatchRom(*owm, romId);
                        CmdResponse = "Match ROM";
                        RomID_Ret = true;
                        ReadCount = BYTES_IN_ROM_ID;
                    }
                    break;

                case 'O':
                    //its an OVERDRIVE command
                    i++;
                    switch(*i)
                    {
                        case 'M':
                             RomIDstr = cmd.substr(3);       // Obtains the string after the 3 CMD characters (The ROM ID we are looking for)
                            if(RomIDstr == ""){
                                microUSB.printf("Please select a Rom ID from the Devices on the Bus\r\n");
                                break;
                            }
                            for(j=0; j<BYTES_IN_ROM_ID; j++){
                                strpos = 2*j;
                                temp = RomIDstr.substr(strpos, 2);
                                num = strtol(temp.c_str(), &strend, 16);
                                for(k=0; k<RomIDs.size(); k++){
                                    // First, check to make sure that the RomID we are checking still matches
                                    if(!RomIDMatches[k]){
                                        break;
                                    }
                                    // If the byte in num does not match the byte in the RomID, set the corresponding index in 'matches' to false
                                    if(num != RomIDs[k].buffer[(BYTES_IN_ROM_ID - 1) - j]){
                                        RomIDMatches[k] = false;
                                        break;
                                    }
                                }
                            }
                            j=0;    // Reset the loop variables
                            k=0;
                            // Now, there should only be 1 or 0 'true' values in RomIDMatches, so use that index for the MatchROM
                            while(!RomIDMatches[j] && j<RomIDs.size()){
                                j++;
                            }
                            // Now we have determined that the jth index of the RomIDs vector contains the RomID given by the user,
                            romId = RomIDs[j];
                            result = OWOverdriveMatchRom(*owm, romId);
                            CmdResponse = "Overdrive Match ROM";
                            RomID_Ret = true;
                            ReadCount = BYTES_IN_ROM_ID;
                            break;

                        case 'S':
//                            microUSB.printf("3: Its an OD Skip ROM\r\n");
                            result = OWOverdriveSkipRom(*owm);
                            CmdResponse = "Overdrive Skip ROM";
                            break;

                        default:
                            microUSB.printf("%s \r\n", ROM_Error.c_str());
                            break;
                    }
                    break;

                case 'R':
                    // It's a Resume OR ReadROM
//                    microUSB.printf("2: Res or Read ROM command \r\n");
                    i++;
                    switch(*i) {
                        case 'D':
//                            microUSB.printf("3: Its a Read ROM\r\n");
                            result = OWReadRom(*owm, romId);
                            RomID_Ret = true;
                            ReadCount = 8;
                            CmdResponse = "Read ROM";
                            break;
                        case 'S':
//                            microUSB.printf("3: Its a Resume\r\n");
                            result = OWResume(*owm);
                            CmdResponse = "Resume";
                            break;
                        case 'I':
                        /* Preforms a search finidng all devices on the bus */
                            result = FindAllRomIDs(*owm, search_state);
                            CmdResponse = "Standard Search";
                            break;
                        default:
                            microUSB.printf("%s \r\n", ROM_Error.c_str());
                            break;
                    }
                    break;

                case 'S':
                    // It's a Search rom (f or n) OR a SkipROM
//                    microUSB.printf("2: Search or skip ROM command \r\n");
                    i++;
                    switch(*i) {
                        case 'K':
//                            microUSB.printf("3: Skip-ROM issued\r\n");
                            result = OWSkipRom(*owm);
                            CmdResponse = "Skip ROM";
                            break;

                        default:
                            microUSB.printf("%s \r\n", ROM_Error.c_str());
                            break;
                    }
                    break;

                case 'A':
                    // It's an alarm search
                    i++;
                    if(*i == 'S'){
                        result = AlarmSearchBus(*owm, search_state);
                        CmdResponse = "Alarm Search";
                    }
                    else{
                        microUSB.printf(" %s \r\n", ROM_Error.c_str());
                    }
                break;

                default:
                microUSB.printf("%s \r\n", ROM_Error.c_str());
                break;
            }
            break;

        case 'S':
//            microUSB.printf("1: Speed command \r\n");
            // it is a speed command
            i++;
            switch(*i) {
                case 'N':
                    NextSpeed = OneWireMaster::StandardSpeed;
//                    microUSB.printf("2: Standard Speed command issued\r\n");
                    if(CurrSpeed == NextSpeed) {
                        microUSB.printf("Already in Standard Speed Mode \r\n");
                        result = OneWireMaster::Success;
                    } else {
                        result = owm->OWSetSpeed(NextSpeed);
                        CurrSpeed = NextSpeed;
                        CmdResponse = "Normal Speed";
                    }
                    break;
                case 'O':
                    NextSpeed = OneWireMaster::OverdriveSpeed;
//                    microUSB.printf("2: Overdrive speed command issued\r\n");
                    if(CurrSpeed == NextSpeed) {
                        microUSB.printf("Already in Overdrive Speed Mode\r\n");
                        result = OneWireMaster::Success;
                    } else {
                        result = owm->OWSetSpeed(NextSpeed);
                        CurrSpeed = NextSpeed;
                        CmdResponse = "Overdrive Speed";
                      }
                      break;
                default:
                    microUSB.printf("%s \r\n", Speed_Error.c_str());
                    break;
                }
            break;

        case 'W':
            // it is a R/W cmd
//            microUSB.printf("1: Read/Write command \r\n");
            i++;
            switch(*i) {

                case 'R':
//                    microUSB.printf("2: Read command \r\n");
                    // Its a read cmd
                    i++;
                    switch(*i) {
                        case 'I':
//                            microUSB.printf("3: Read bit command \r\n");
                            // Read single bit command
//                            microUSB.printf("Read bit command issued\r\n");
                            // When to make read bit 1 or 0
                            result = owm->OWTouchBitSetLevel(*ReadBytes, CurrLevel);
                            Read_Ret = true;
                            ReadCount = 1;
                            CmdResponse = "Read Bit";
                            break;

                        case 'Y':
                            // Read X Bytes
//                            microUSB.printf("3: Read byte command \r\n");
                            /* Extract the data from the CMD string */
                            temp = cmd.substr(3);
                            /* Convert the string to the corresponding int */
                            ReadCount = StringtoDecimal(temp);
                            result = owm->OWReadBlock(ReadBytes, (size_t)ReadCount);
                            Read_Ret = true;
                            CmdResponse = "Read " + temp + " Bytes";
                            break;

                        default:
                            microUSB.printf("%s \r\n", RW_Error.c_str());
                            break;
                    }
                    break;

                case 'W':
                    // Its a write command
//                    microUSB.printf("2: Write Command\r\n");
                    i++;
                    switch(*i) {
                        case 'I':
//                            microUSB.printf("3:Write bit command \r\n");
                            //Write a single bit
                            i++;
                            c = *i;  // Get the 1/0 needed as a string
                            //microUSB.printf("The string obtained is: '%s' and it has 0x%02X characters\r\n", c, c.length());
                            num = StringtoDecimal(c); //convert to decimal
//                            microUSB.printf("Issuing a Write %d command\r\n", num);
                            result = owm->OWTouchBitSetLevel(num, CurrLevel);
                            CmdResponse = "Write Bit";
                            break;

                        case 'Y':
                            // Write multiple bytes
//                            microUSB.printf("3:Write Byte command \r\n");
                            temp = cmd.substr(3);

                            if((temp.length()%2) != 0){
                                microUSB.printf("Please enter a Byte Alligned Value\r\n");
                                return;
                            }

                            /* Throws an error and returns if input is not formatted correctly */
                            for(j=0; j<temp.length(); j++){
                                ch= temp[j];

                                if((ch < '0' || ch > '9') && (ch < 'A' || ch > 'F')){
                                    microUSB.printf("Please ensure that you enter hex values\r\n");
                                    return;
                                }
                            }

                            for(j=0; j<(temp.length()+1)/2; j++){
                                strpos = 2*j;
                                c = temp.substr(strpos, 2);
                                hex_val = strtol(c.c_str(), &strend, 16);
                                num = (uint8_t) hex_val;
                                Scratchpad_Data[j] = num;
                            }
                            microUSB.printf("Wrote '0x%s'\r\n", temp);
                            result = owm->OWWriteBlock(Scratchpad_Data, (temp.length()+1)/2);
                            CmdResponse = "Write";
                            break;
                        default:
                            microUSB.printf("Unrecognized Command\r\n");
                            break;
                    }
                    break;
            }
            break;

        case 'P':
//            microUSB.printf("1: Power command \r\n");
            i++;
            switch(*i) {
                case 'N':
//                    microUSB.printf("2: Normal PWR command \r\n");
                    i++;
                    if(*i == 'O') {
//                        microUSB.printf("3: Further Normal PWR command \r\n");
                        NextLevel = OneWireMaster::NormalLevel;
                        if(NextLevel == CurrLevel) {
                            microUSB.printf("Normal Power\r\n");
                        } else {
                            microUSB.printf("Normal Power\r\n");
                            result = owm->OWSetLevel(NextLevel);
                            CurrLevel = NextLevel;
                            CmdResponse = "Normal Power";
                        }
                    }

                    else microUSB.printf("Unrecognizable\r\n");
                    break;

                case 'W':
                    // Strong pullup after writing...
//                    microUSB.printf("2: Power Write command \r\n");
                    i++;
                    if(NextLevel == OneWireMaster::StrongLevel) {
                        microUSB.printf("Already in Strong Pull-Up Mode\r\n");
                        result = OneWireMaster::Success;
                        break;
                    }
                    NextLevel = OneWireMaster::StrongLevel;
                    switch(*i) {
                        case 'I':
//                            microUSB.printf("3: Power Write bit command \r\n");
                            temp = cmd.substr(3);
                            num = atoi(temp.c_str());
                            if(temp == "1" || temp == "0") {
                                //microUSB.printf("Issuing strong Pullup after writing %d \r\n", num);
                                result = owm->OWWriteBitPower(num);
                                microUSB.printf("Switching to Strong Pull-Up Mode\r\n");
                                CurrLevel = NextLevel;
                                CmdResponse = "Strong Pullup After Writing Bit";
                                microUSB.printf("Wrote '0x%s'\r\n", temp);
                            }
                            else {
                                microUSB.printf("Incorrect formatting error. Decoded data as 0x%02X, expecting 1 or 0 \r\n", num);
                            }
                            break;

                        case 'Y':

//                            microUSB.printf("3: Issuing Strong Pullup after writing Byte\r\n");
                            temp = cmd.substr(3);
                            hex_val = strtol(temp.c_str(), &strend, 16);

                            result = owm->OWWriteByteSetLevel(hex_val, NextLevel);
                            microUSB.printf("Switching to Strong Power Mode\r\n");
                            CurrLevel = NextLevel;
                            CmdResponse = "Strong Pullup After Writing Byte";
                            microUSB.printf("Wrote '%s'\r\n", temp);
                            break;

                        default:
                            microUSB.printf("Could not identify that command\r\n");
                            break;
                    }
                    break;
                default:
                    microUSB.printf("%s\r\n", Pullup_Error.c_str());
                }
                break;
        case 'Z':
            result = owm->OWReset();
            CmdResponse = "Reset";
            break;
        } // End of switch

        /* For loop to print out the 8 byte Rom ID and add it to the 'RomIDs' vector*/
        if(RomID_Ret && result == OneWireMaster::Success) {
          /* Flag to add to vector or not */
          AddROM = true;

          /* Printing */
          microUSB.printf("RomID: ");
          for(j=ReadCount-1; j>=0; j--) {
              microUSB.printf("%02X ",romId.buffer[j]);
          }
          microUSB.printf("\r\n");

          /* Add the Rom ID to the vector if not in there already, and update the most recent RomID index used */
          for(j=0; j<RomIDs.size(); j++){
              if(RomIDs[j] == romId){
                  AddROM = false;
                  mostRecentIdx = j;
                  break;
              }
          }

          if(AddROM){
             /* If we need to add the RomID to the vector, that means it was the most recent one that was used/found, so update the mostRecentIdx */
             RomIDs.push_back(romId);
             mostRecentIdx = RomIDs.size() - 1;
             microUSB.printf("Added RomID to array\r\n");
          }

          if(search_state.last_device_flag && AddROM && !AlarmSearch){
                microUSB.printf("Obtained RomID's for all devices on the bus\r\n");
          }
      }

        /*Return bytes that were read from the device */
      if(Read_Ret && result == OneWireMaster::Success) {
          microUSB.printf("Result: ");
          for(j=0; j<ReadCount; j++) {
              microUSB.printf("%02X ",ReadBytes[j]);
          }
          microUSB.printf("\r\n");
      }

      if(result == OneWireMaster::Success) {
          microUSB.printf("The %s was successful\r\n", CmdResponse);
      } else if (result == OneWireMaster::CommunicationWriteError){
          microUSB.printf("Command not issued, Communication Write Error\r\n");
      } else if (result == OneWireMaster::CommunicationReadError){
          microUSB.printf("Command not issued, Communication Read Error\r\n");
      } else if (result == OneWireMaster::TimeoutError) {
          microUSB.printf("The device timed out before the command could be issued\r\n");
      } else {
          microUSB.printf("Operation Failed\r\n");
      }
}


uint8_t StringtoDecimal(string _string) {
    uint8_t number;
    number = atoi(_string.c_str());
    return number;
}

void OWM_Initialize(MCU_OWM owm) {
    rLED = LED_ON;
    OneWireMaster::CmdResult result = owm.OWInitMaster();
    while(result != OneWireMaster::Success) {
        microUSB.printf("Failed to init OWM...\r\n\r\n");
        result = owm.OWInitMaster();
        wait(0.5);
        rLED = !rLED;
    }
    rLED = LED_OFF;
}


OneWireMaster::CmdResult AlarmSearchBus(MCU_OWM & owm, SearchState & search_state){
    int numAlarms = 0;
    RomId RetRom;
    OneWireMaster::CmdResult res;
    search_state.reset();
    while(!search_state.last_device_flag){
        res = OWAlarmSearch(owm, search_state);
        if(res == OneWireMaster::Success){
            numAlarms++;
            RetRom = search_state.romId;
            microUSB.printf("Active Alarms: ");
            for(int j=BYTES_IN_ROM_ID-1; j>=0; j--) {
              microUSB.printf("%02X ",RetRom.buffer[j]);
            }
            microUSB.printf("\r\n");
        }
    }
    return res;
}



OneWireMaster::CmdResult FindAllRomIDs(MCU_OWM & owm, SearchState &search_state){
    int numDevices = 0;
    RomId romId;
    OneWireMaster::CmdResult res;

    RomIDs.clear();
    search_state.reset();

    /* Identify next Device on the bus, and print out its ROM ID so GUI can parse it, and add it to the FW array */
    while(!search_state.last_device_flag){
        res = OWSearch(owm, search_state);
        if(res == OneWireMaster::Success){
            numDevices++;
            romId = search_state.romId;
            microUSB.printf("RomID: ");
            for(int j=BYTES_IN_ROM_ID-1; j>=0; j--) {
              microUSB.printf("%02X ",romId.buffer[j]);
            }
            microUSB.printf("\r\n");
            RomIDs.push_back(romId);
        }
    }
    return res;
}
