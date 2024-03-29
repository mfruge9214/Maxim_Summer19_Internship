Raw 1-Wire Interface:

Required Components:
	- MAX32630FTHR + USB Cable + MAX32630PICO (to load the Firmware)
	- Access to the GUI and Firmware
	- Any number of 1-Wire devices
	- Hardware to create a 1-Wire device bus

Setup:
	- The MAX32630FTHR (FTHR) must be attached to your PC via Serial Port for the communication pathway to be established between the GUI and the FTHR 
	- The 1-Wire devices can be either Parasite Powered or supplied with 3.3-5V
	- A logic analyzer or oscilloscope is helpful to connect to the bus and see the waveforms being generated by the One Wire Master and Slaves
	- The FTHR must be loaded with the distributed firmware and the GUI must be installed on your PC
	- When the GUI process begins, the Maxim Splash Screen will appear. This is something everyone else experiences and is a normal part of growing up and running programs
	- After 3 seconds, the Maxim Splash Screen will disapppear and the 'Connect to Serial Port' screen will appear. This screen is where you will connect your serial port to the FTHR
		- Press the 'Search' button to populate the list of COM ports. Select the COM port that the FTHR is connected to, and press the 'Connect' button
	- Once you are connected to a Serial port, the Main Program will appear and specify the COM port and Baud rate, as well as the firmware version

Usage:
	- On the Left side of the Screen are all of the ROM layer commands
		- Standard Search: Populates the dropdown list with ROM ID's of devices on the bus
		- Alarm Search: Populates the dropdown with ROM ID's of actively alarming devices
		- When a device is selected in the dropdown, that device's ROM ID will be used in any ROM Command that must specify the device (Match ROM and OD Match ROM)
		- To issue a ROM Command, simply select the command and press 'Send ROM Command' 
			- Note: Not all ROM commands will work at all times (Read ROM on a multi-device bus). Ensure that the command you are trying to issue is within the 1-Wire specs
			
	- Once a ROM command is issued and a specific device is selected, then any of the device's Function Commands can be issued by entering the command's Hex values in the Text box and pressing 'Send Data + Pullup Option'
		- If the Function Command requires more than 1 byte, then the string can be issued all at once (1 press of the send button) or separately (More than 1 press of the send button)
		- If the Function Command requres a strong pull up on the bus, then write the command to the text box but before pressing send, change the selection in the box to Strong Pull-up After Writing Bit/Byte.
			- To get back into normal power mode, just change the selection back to normal and press 'Send Data + Pull-up Option' again
			
	- Output from the bus and the firmware is displayed in the Text Box on the right side of the screen
			
			
			
Example:
	- Following along with the DS18B20 datasheet on page 18, this will demonstrate how to use this program to mirror the operations
	--------------------------------------
	DS18B20 Operation Example 1:
		1)
			- Standard Search (to obtain ROM ID's)
			- Select a ROM ID from the dropdown
			- Select 'Reset + Match ROM'
			- 'Send ROM Command'
		2) 
			- Enter '44' into 'Data to Send'
			- Change the selection from Normal to Strong Pull-Up after Writing Byte
			- 'Send Data + Pull-Up Option'
			- Change selection back to 'Normal'
			- 'Send Data + Pull-Up Option'
		1)
			- Select the same ROM ID from the list
			- 'Reset + Match ROM'
			- 'Send ROM Command'
		2)
			- Enter 'BE' into 'Data to Send'
			- Select 9 in the 'Number of Bytes to Read' +/- box
			- 'Read Bytes'
			
		Output)
			- 9 byte scratchpad, 1st byte is LSB of Temperature; 9th byte is CRC
			* SHould not be a sequence of F's, otherwise something was messed up

Decoding:
	- This program relies on an arbitrary code that was created by the developer and is hidden from the user. However, if only using the firmware and not the GUI, then you will need these codes to keep the firmware functional by
sending them through the serial port to the FTHR via Terminal

		- ROM Commands:
			- 'RMT': Match ROM
			- 'ROM': Overdrive Match ROM
			- 'ROS': Overdrive Skip ROM
			- 'RRI': Read ROM
			- 'RRS': Resume
			- 'RRI': Standard Bus Search
			- 'RSK': Skip ROM
			- 'RAS': Alarm Search
		
		- Speed Commands:
			- 'SNL': Normal Speed
			- 'SOD': Overdrive Speed
		
		- Read/Write Commands
			- 'WWI' + '1/0': Write Bit (1 | 0)
			- 'WRI': Read Bit
			- 'WWY' + data: Write Bytes and the data to write
			- 'WRY' + numBytes: Read Bytes and the number of bytes to read

		- Pullup Commands
			- 'PNO': Normal Pullup
			- 'PWI' + '1/0': Strong Pullup after Writing Bit (1 | 0)
			- 'PWY' + byte: Strong Pullup after Writing Byte and the byte to write