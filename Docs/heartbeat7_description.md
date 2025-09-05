### heartbeat7:
---
This code implements a Bluetooth Low Energy (BLE) data acquisition system using the MAX30105 sensor. 
It is designed to collect photoplethysmographic (PPG) signals—specifically Red and Infrared (IR)—and temperature data, 
and transmit them via BLE to a central device. 
The system operates in two modes: continuous acquisition of Red/IR signals, and batch transmission of temperature data. 
Data is buffered and sent in structured packets, with timestamps included for synchronization. 
The code handles sensor initialization, BLE service setup, data formatting, and communication logic;  
Its structure makes it easy to interface with a MATLAB script that
receives, decodes, and analyzes the incoming data for further processing or visualization.
---

### GLOBAL VARIABLES 
---
- performingMeasure: Flag that initiates BLE acquisition and transmission (notify) of Red and IR signals.
- sendTemp: Flag that initiates acquisition and BLE transmission (notify) of temperature data.
- isConnected: Flag indicating an active Bluetooth connection.

- numSec: Desired duration of measuring in seconds.

- dataPacket[100]: Preallocated uint8_t array, buffer for sending BLE Red and IR data.
- sampleIndex: Index for writing 10 samples (10 Byte/Sample) into the buffer.

- temperatureBuffer[numSec]: Preallocated float array, buffer for temperature data.
- timeBuffer[numSec]: Preallocated uint32_t array, buffer for timestamps associated with temperature data.
- tempIndex: Index updated each time a new temperature data point is written.
- tempSendIndex: Index tracking the number of temperature data points already sent.
- packetCounter: Counter for Red/IR packets (increments by 1 after each BLE data transmission).

When packetCounter = 10 a temperature data point is stored in temperatureBuffer, so is its timestamp.

- lastPacketTimestamp: Timestamps for Temperature data points.

- BLEService: Bluetooth Low Energy service.
- bleData: BLE characteristic for Red and IR data with timestamps (IR + RED + Time = 3 + 3 + 4 = 10 bytes).
- bleCommand: BLE characteristic used as control channel.
- bleTemp: BLE characteristic for temperature data (Temperature + Time = 4 + 4 = 8 bytes).

- MAX30105 particleSensor: Sensor object.



 ### HELPER FUNCTIONS 
 ---

## 1.  byte readRegister(byte reg)

READS A BYTE FROM A MAX30102 REGISTER 

- Writes the register address via I2C.
- Requests to read a byte.
- Checks that exactly one byte is received.
- Returns the byte if available, otherwise returns 0.

---------------------------------------------------------------------------------------

## 2. float readTemperature()

RETURNS A TEMPERATURE DATAPOINT

- Writes 1 to the temperature configuration register of the sensor (TEMP_EN = 1 --> conversion starts).
- Reads the integer part of the Temperature register.
- Reads the integer and fractional parts of the temperature register.
- Returns the temperature as: integer + fraction * 0.0625°C.

---------------------------------------------------------------------------------------

## 3. void sendTemperatureBufferBLE()

SENDS TEMPERATURE DATA  VIA A SERIES OF BLE PACKETS  

maxPerPacket: Number of (Temperature, Time) entries per BLE packet.
tempPacket[80]: Preallocated array for the packet.

- While tempSendIndex < tempIndex (while it has not sent all the data).
- Calculates entries = min(maxPerPacket, tempIndex - tempSendIndex). 
	computes entries as this as the	last packet may have less than 10 entries.  
- For each entry j:
    - Copies 4 bytes of temperature from temperatureBuffer[tempSendIndex + j] to tempPacket[j*8].
    - Copies 4 bytes of timestamp from timeBuffer[tempSendIndex + j] to tempPacket[j*8 + 4].
- Sends the BLE buffer (writes tempPacket).
- Increases tempSendIndex by entries.
- Increments tempSendIndex by entries.
- Resets indices and sets sendTemp to false.

---------------------------------------------------------------------------------------

## 4. void storeOneSample()

STORES AND SENDS DATA BUFFERS OF 10 SAMPLES
IN ADDITION, EVERY 10 SENDS STORES A TEMPERATURE AND ITS TIMESTAMP IN THE RESPECTIVE BUFFERS (T°; t)

- Reads and assigns to variables red, ir and time values (irValue, redValue, timeMicros).
- Writes the values (3 Big Endian + 3 Big Endian + 4 Little Indian bits) into the buffer with an incremental offset.
- Increases sampleIndex each time a value is written.
- When sampleIndex reaches 10:
	- Sends BLE buffer (writes dataPacket on bleData characteristic). 
	- Increments packetCounter.
	
- When packetCounter reaches 10 (1 Hz cca) a Temperature datapoint, with its timestamp, is stored:
	- Assigns lastPacketTimeStamp and writes it into the timeBuffer.
	- Calls readTemperature and writes the return in the temperatureBuffer.
	- Increases tempIndex.
	- Resets packetCounter to 0.



 ### SETUP 
 ---
1. Initialises I2C connection with sensor (multiple attempts).
2. Configures sensor acquisition parameters.
3. Enables the interrupt for PPG_RDY and reads the register to clean it of residual interrupts.
4. Creates BLE features and service, adds service, initialises feature values, publishes BLE service.



 ### LOOP  
 ---

1. Continuously searches for a central BLE device until isConnected is true.
2. If a connection exists:
  2a. Sets isConnected to true.
	2b. If bleCommand is written:
     - Assigns value to cmd and interprets:
       - cmd == 1: Acquire Red and IR, do not send temperature (performingMeasure = true, sendTemp = false).
       - cmd == 0: Do not acquire Red and IR, send temperature (performingMeasure = false, sendTemp = true).
       - Otherwise: Do not acquire or send (performingMeasure = false, sendTemp = false).
	2c. If performingMeasure: 
			- polling of PPG_RDY register, if its value is 1 a datapoint is ready in the sensor FIFO and gets read (storeOneSample() is called).
	2d. If sendTemp:
			- Temperature and time buffers are sent via consecutive packets written on bleTemp (sendTemperatureBufferBLE() is called)
3. Else (if no connection exists): performs a reset in the event of a disconnection, by setting:
			isConnected = false; 
			performingMeasure = false; 
			sendTemp = false; 
			sampleIndex = 0; 
			packetCounter = 0; 
			tempIndex = 0.
