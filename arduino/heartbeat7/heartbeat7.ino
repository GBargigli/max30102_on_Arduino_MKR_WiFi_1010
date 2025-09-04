// Max 30102 on Arduino MKR WiFi 1010

// Necessary libraries
#include <Wire.h>
#include "MAX30105.h"
#include <ArduinoBLE.h>

// ========== GLOBAL VARIABLES =======================
bool performingMeasure = false; // turns TRUE when MATLAB sends the start input for measuring (n!=0) 
bool sendTemp = false; // turns true when BLE transmission of RED and IR data ends, starts BLE transmission of Temperature data
bool isConnected = false; // turns TRUE when  a BLE connection is established

BLEDevice central; // represents a central Bluetooth device that connects to your Arduino device.

// DESIRED DURATION for measuring (sec) 
const int numSec = 60;  // 

// BUFFER FOR DATA PACKETS (Red and IR) of 10 samples
uint8_t dataPacket[100];    // 10 samples * 10 byte = 100 byte data packets
int sampleIndex = 0;        // current sample index (0-9)

// =========== TEMPERATURE BUFFER (FOR POST-MEASUREMENT DISPATCH, n° samples = numSec) ===================================

float temperatureBuffer[numSec];      
uint32_t timeBuffer[numSec];          
int tempIndex = 0;  // number of data already in the buffer 
int tempSendIndex = 0; // number of sent packages
int packetCounter = 0;  // nummber of bleData packets sent (one every 10 read for T°)
uint32_t lastPacketTimestamp = 0;

// ============ BLE CONNECTION INITIALIZATION =========================== 

// Creates a Bluetooth Service dedicated to the sensor
BLEService bleService("8ee10201-ce06-438a-9e59-549e3a39ba35"); 

// Unique BLE characteristic for the signal data (IR + RED + Time = 3+3+4 byte = 10 bytes)
BLECharacteristic bleData("33303cf6-3aa4-44ad-8e3c-21084d9b08db", BLERead | BLENotify, 100); 
// Control channel (bi-directional), when MATLAB sends on the channel a number !=0 --> measurement starts
BLEByteCharacteristic bleCommand("bf789fb6-f22d-43b5-bf9e-d5a166a86afa", BLERead | BLEWrite | BLENotify);   
//BLE characteristic for temperature data (float 4 bytes + time data 4 bytes = 8 bytes)
BLECharacteristic bleTemp("44404cf7-4bb5-55be-9f60-32195a8c09ec", BLERead | BLENotify, 80); 

MAX30105 particleSensor;    // creates sensor object


// ============ HELPER FUNCTIONS ==================================================

/* READREGISTER FUNCTION
Reads a register from the max30102 sensor 
Prints an error message if the number of received bytes is different from the expected number of bytes.
Returns, if present, the output of the reading

input: register to be read 
output: read bytes */

byte readRegister(byte reg) { 
  Wire.beginTransmission(0x57); // MAX30102 address
  Wire.write(reg); // sends the register that needs to be read to the sensor.
  Wire.endTransmission(false); // terminates writing but leaves the I2C connection active
  
  uint8_t bytesRequested = 1; // expected number of bytes
  uint8_t bytesReceived = Wire.requestFrom(0x57, bytesRequested); // bytes of the reading

  if (bytesReceived != bytesRequested) { 
    Serial.print("Error: ");
    Serial.print(bytesReceived);
    Serial.println(" byte received instead of 1.");
    return 0x00;
  }

  if (Wire.available()) {
    return Wire.read();
  } else {
    Serial.println("Errore: nessun dato disponibile!");
    return 0x00;
  }
}

/* READTEMPERATURE FUNCTION
reads the temperature of the sensor every second
stores temperature and time data in the temperature buffer and timebuffer
Uses registers 0x1F (integer part) and 0x20 (fraction) with readRegister,
As the sensor stores Temperature data decomposed in those two parts. 

Output: floating value of temperature in °C (e.g. 25.25) */

float readTemperature() {
  // Starts a temperature conversion by writing 1 to the TEMP_EN bit (register 0x21 of the max30102 sensor)
  Wire.beginTransmission(0x57);
  Wire.write(0x21);        // Temperature control register
  Wire.write(0x01);        // sets TEMP_EN = 1 (starts conversion)
  Wire.endTransmission();


  // Reads the integer part of the temperature (0x1F register) using readRegister
  int8_t tempInt = (int8_t)readRegister(0x1F);

  // Reads the fractional part of the temperature (register 0x20)
  uint8_t tempFrac = readRegister(0x20) & 0x0F; // I 4 bit meno significativi rappresentano la frazione

  // Returns temperature as sum of integer + fraction * 0.0625°C 
  return tempInt + (tempFrac * 0.0625f);
}

/* SEND TEMPERATURE BUFFER FUNCTION
Until there are no more data to sent in the Temperature buffer and Time buffer:
- Constructs packets of 80 bytes for 10 Temperature datapoints.
- writes the packet on the bleTemp Characteristic.
*/ 

void sendTemperatureBufferBLE() {
  const int maxPerPacket = 10; // sets 10 as number of temperature datapoints per packet: 10 x 8 = 80 bytes
  uint8_t tempPacket[80]; // 80 byte BLE packet

  /* SERIAL DEBUG
  Serial.println("=== Sending temperature data via BLE ===");
  Serial.print("Total measures to be sent: ");
  Serial.println(tempIndex); */

  while (tempSendIndex < tempIndex) {   // while n° of sent T° datapoints < n° of stored T° datapoints 
    int entries = min(maxPerPacket, tempIndex - tempSendIndex); // number of datapoints per package, last package may have less than 10 entries.
    // for each entry:
    for (int j = 0; j < entries; j++) {
      memcpy(&tempPacket[j * 8], &temperatureBuffer[tempSendIndex + j], 4); // Copies 4 bytes of the float to position j*8 of the packet
      memcpy(&tempPacket[j * 8 + 4], &timeBuffer[tempSendIndex + j], 4); // Copies 4 bytes of the timestamp to position j*8 + 4
    }
    bleTemp.writeValue(tempPacket, entries * 8); // writes entries * 8 bytes on the bleTemp characteristic

      /* SERIAL DEBUG
      Serial.print("Package sent with ");
      Serial.print(entries);
      Serial.println(" datapoints:");
      for (int j = 0; j < entries; j++) {
        float temp;
        uint32_t ts;
        memcpy(&temp, &tempPacket[j * 8], 4);
        memcpy(&ts, &tempPacket[j * 8 + 4], 4);
        Serial.print("  #");
        Serial.print(tempSendIndex + j);
        Serial.print(" -> Temp: ");
        Serial.print(temp, 2);
        Serial.print(" °C | Time: ");
        Serial.println(ts);
      } */

  tempSendIndex += entries; 
  BLE.poll(); 
  delay(2000); //  delay for BLE stability  
  }

  //Serial.println("End of temperature buffer transmission.");
  
  // Resets indices and flags
  tempSendIndex = 0;
  tempIndex = 0;
  sendTemp = false;
}


/* STOREONESAMPLE FUNCTION 
reads Red and IR values from the max30102 sensor, stores timestamps in Microseconds
*/
void storeOneSample() {
  uint32_t irValue = particleSensor.getIR();
  uint32_t redValue = particleSensor.getRed();
  uint32_t timeMicros = micros();

  int offset = sampleIndex * 10; //  where to write the sample in the buffer

  // Creates an unique string of bytes with Red, IR and Time data and writes it in the buffer
  // IR - Big Endian
  dataPacket[offset + 0] = (uint8_t)(irValue >> 16);
  dataPacket[offset + 1] = (uint8_t)(irValue >> 8);
  dataPacket[offset + 2] = (uint8_t)(irValue);

  // RED - Big Endian
  dataPacket[offset + 3] = (uint8_t)(redValue >> 16);
  dataPacket[offset + 4] = (uint8_t)(redValue >> 8);
  dataPacket[offset + 5] = (uint8_t)(redValue);

  // TIME - Little Endian
  dataPacket[offset + 6] = (uint8_t)(timeMicros & 0xFF);
  dataPacket[offset + 7] = (uint8_t)((timeMicros >> 8) & 0xFF);
  dataPacket[offset + 8] = (uint8_t)((timeMicros >> 16) & 0xFF);
  dataPacket[offset + 9] = (uint8_t)((timeMicros >> 24) & 0xFF);

  sampleIndex++; // Signals that one datapoint has been written in the buffer

  // If I have collected 10 samples, I send the BLE packet, 
  if (sampleIndex >= 10) {
    bool result = bleData.writeValue(dataPacket, 100);

  // SERIAL DEBUG - if it fails, notifies it
    //if (!result) {
    //   Serial.println("BLE write failed!");
  // }

    sampleIndex = 0; 
    packetCounter++;

    /* EVERY TIME 10 DATA PACKET ARE SENT (about 1 Hz)
    A TEMPERATURE VALUE AND ITS TIMESTAMP IS STORED IN THE TEMPERATURE BUFFER
    */

    if (packetCounter >= 10 && tempIndex < numSec) { 
      // === When I send I update LastPacketTimeStamp
      lastPacketTimestamp = millis();
      //Serial.println("lastPacketTimestamp =  ");
      //Serial.println(lastPacketTimestamp);

      float temp = readTemperature();
      temperatureBuffer[tempIndex] = temp;
      /*Serial.println("T° read: ");
      Serial.println(temp);
      Serial.println("at time: ");
      Serial.println(lastPacketTimestamp); */
      timeBuffer[tempIndex] = lastPacketTimestamp;
      tempIndex++;
      //Serial.println("tempIndex: ");
      //Serial.println(tempIndex);
      packetCounter = 0;
      }
  }
}

// ========================== SETUP =================================================================

void setup()
{
// ========================== I2C CONNECTION ========================================================
  Serial.begin(115200); // Serial monitor initialisation
  // Arduino initialises sensor with I2C master role
  Wire.begin();                         // Used to use the sensor as a master
  delay(100);                           
  
  // Multiple attempts to initialise the sensor 
  bool sensorFound = false;
  for (int attempts = 0; attempts < 25; attempts++) {
    if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
      Serial.println("MAX30105  found.");
      sensorFound = true;
      break;
    }
    Serial.println("MAX30105 not found. Retrying...");
    delay(500);
  }
  if (!sensorFound) {
    Serial.println("MAX30105 was not found after multiple attempts.");
    while (1);  // blocks everything in an infinite loop if sensor not found
  }

// ================== SENSOR CONFIGURATION ============================================================
  // Configures the sensor with chosen parameters
  byte ledBrightness = 0xFF;  // 
  byte sampleAverage = 8;     // quindi l'output è a 100 Hz
  byte ledMode = 2;           // Rosso + IR
  int sampleRate = 1600;        // Hz
  int pulseWidth = 69;        // μs
  int adcRange = 16384;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  // ================== PPG_RDY INTERRUPT CONFIGURATION ============================================================
  // Enable interrupt PPG_RDY by writing to register 0x02 (see max 30102 datasheet for PPG_RDY interrupt settings and functioning)
  Wire.beginTransmission(0x57);
  Wire.write(0x02);              // 0x02 = Interrupt Enable 1
  Wire.write(0b01000000);        // PPG_RDY_EN = 1
  Wire.endTransmission();

  // Reads the status register to clean up any pending interrupts
  Wire.beginTransmission(0x57);
  Wire.write(0x00); // 0x00 = Interrupt Status Register 1
  Wire.endTransmission(false); // Don't send stop, get ready to read
  Wire.requestFrom(0x57, 1);
  byte dummy = Wire.read(); // Information read and ignored, it only serves to clean

  // DEBUG 
  // Serial.println("Interrupt PPG_RDY correctly configured!"); 

// ============ BLE CONNECTION INITIALIZATION =========================================================
  // Checks whether the BLE module has been correctly initialised
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy failed!");
    while (1);
  }

  // Sets the local name of the Bluetooth device
  BLE.setLocalName("AA Pulse Oximeter");
  
  // Sets the Bluetooth service to be published
  BLE.setAdvertisedService(bleService);
 
  // Adds features to the BLE service
  bleService.addCharacteristic(bleCommand); 
  bleService.addCharacteristic(bleData);    
  bleService.addCharacteristic(bleTemp);

  // Adds Bluetooth service
  BLE.addService(bleService);

  // Initialises feature values
  uint8_t zeroData[100] = {0};
  bleData.writeValue(zeroData, 100);
  bleCommand.writeValue(2);

  // Publishes BLE service
  BLE.advertise();
  Serial.println("Dispositivo Bluetooth attivo, in attesa di connessioni...");
}

void loop() {
//========================== BLE connection management =================================================
  if (!isConnected) {
    central = BLE.central();
  }

  if (central.connected()) {
    isConnected = true;

    if (bleCommand.written()) {           
    uint8_t cmd = bleCommand.value();

    // bleCommand Char is written by MATLAB as:
    switch (cmd) { 
        // 1 - Performs Red and IR mearures, doesn't send Temperature buffer data
        case 1:
            //Serial.println("CASE 1: meas T temp F");
            performingMeasure = true;
            sendTemp = false;
            tempIndex=0;
            break;
        case 0:
        // 0 - Doesn't perform Red and IR mearures, sends Temperature buffer data
            //Serial.println("CASE 0: meas F temp T");
            performingMeasure = false;
            sendTemp = true;
            /*Serial.println("=== Dati memorizzati nel buffer (indice | temperatura | timestamp) ===");
            for (int i = 0; i < tempIndex; i++) {
              Serial.print(i);
              Serial.print(" | Temp: ");
              Serial.print(temperatureBuffer[i], 2); // 2 cifre decimali
              Serial.print(" | Time: ");
              Serial.println(timeBuffer[i]);
            }*/
            break;
        default:
        // any other value - Doesn't perform Red and IR mearures, doesn't send Temperature buffer data
            //Serial.println("DEFAULT meas F temp F");
            performingMeasure = false;
            sendTemp = false;
            break;
    }
    }

    // When perforing measures we poll the PPG_RDY register
    if (performingMeasure) {
      byte intStatus = readRegister(0x00);

      if (intStatus & 0x40) { // if Bit PPG_RDY = 1 a new read is ready in the FIFO of the sensor
        storeOneSample();
      }
    }

    // inviare dati quando sendTemp è true
      if (sendTemp) {
          sendTemperatureBufferBLE();  
      }

  } else {
    isConnected = false;
    performingMeasure = false;
    sendTemp = false;
    sampleIndex = 0; // reset in caso di disconnessione
    packetCounter = 0;
    tempIndex = 0;
  }

  BLE.poll(); // Sempre chiamato per gestire lo stack BLE
}
