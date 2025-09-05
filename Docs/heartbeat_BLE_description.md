### MATLAB Function Description: heartbeat_BLE(numSec)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
This MATLAB function manages the reception and storage of Red and Infrared data transmitted via Bluetooth Low Energy (BLE) from the MAX30105 sensor. 
It is designed to work in coordination with an Arduino-based (Arduino MKR WiFi 1010) acquisition system and is typically called by a main script (e.g., SNR2) with the desired acquisition duration in seconds as input. If no input is provided, the default duration is 40 seconds.
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

GLOBAL VARIABLES ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------

- dataArray: Preallocated array of size [numSec*100, 3] to store IR, RED, and timestamp data.
- dataIndex: Index for tracing the number of incoming Red and IR signal samples written into the dataArray.
- tempdata: Preallocated array of size [numSec, 2] to store temperature and timestamp data.
- tempIndex: Index for tracing the number of incoming temperature signal samples written into the tempdata array.
- recFreq: Variable used to calculate the average acquisition frequency.
- numSec: Duration of acquisition in seconds (default = 40).

BLE SETUP AND CONNECTION -------------------------------------------------------------------------------------------------------------------------------------------------------------------
1. Device Identification
   - BLE device name: 'AA Pulse Oximeter'
   - UUIDs for service and characteristics:
     - serviceUUID: Main BLE service
     - cmdUUID: Command BLE characteristic (start/stop acquisition)
     - dataUUID: Signal data BLE characteristic
     - tempUUID: Temperature BLE data characteristic

2. Connection 
   - Disconnects any existing BLE connection.
   - Establishes a new connection to the specified device.
   - Waits until the connection is confirmed.

3. Characteristic Setup
   - Retrieves BLE characteristics for command, signal data, and temperature.
   - Assigns callback functions:
     - notifyCallbackHeartbeat: triggered on signal data reception.
     - tempCallbackHeartbeat: triggered on temperature data reception.
   - Subscribes to BLE notifications for both characteristics.

ACQUISITION --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
1. Start Acquisition
   - Sends command '1' to start signal acquisition.
   - Pauses execution for numSec seconds to allow data collection.
   - Sends command '0' to stop signal acquisition and trigger temperature transmission.

2. Temperature Reception
   - Waits 30 seconds to receive temperature data asynchronously.

CALLBACK FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
notifyCallbackHeartbeat(src, ~)
- Triggered when a 100-byte packet is received (10 samples of 10 bytes each).
- For each sample:
  - Extracts IR and RED values (3 bytes each, Big Endian).
  - Extracts timestamp (4 bytes, Little Endian).
  - Stores the sample in dataArray if within bounds.
  - Increments dataIndex.

tempCallbackHeartbeat(src, ~)
- Triggered when a temperature packet is received (multiple of 8 bytes).
- For each entry:
  - Extracts temperature (4-byte float).
  - Extracts timestamp (4-byte uint32, Little Endian).
  - Stores the entry in tempdata if within bounds.
  - Increments tempIndex.

POST-ACQUISITION STATISTICS ----------------------------------------------------------------------------------------------------------------------------------------------------------------
- Prints total number of signal samples received.
- Computes and prints average acquisition frequency (recFreq).
- Prints total number of temperature samples received.
