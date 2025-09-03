// Max 30102 on Arduino MKR WiFi 1010
// Works along with the MATLAB helper function heartbeat_BLE

#include <Wire.h>
#include "MAX30105.h"
#include <ArduinoBLE.h>

// === VARIABILI GLOBALI =======================
bool performingMeasure = false; // flag per comando (n!=0) da MATLAB
bool sendTemp = false;
bool isConnected = false; // flag connessione
//volatile bool newData = false; // flag per interrupt hardware
BLEDevice central; // central rappresenta un dispositivo Bluetooth centrale che si connette al tuo dispositivo Arduino.

const int numSec = 60;  // DDURATA desiderata in secondi

// BUFFER PER PACCHETTI DI DATI da 10 campioni
uint8_t dataPacket[100];    // 10 campioni * 10 byte
int sampleIndex = 0;        // indice campione attuale (0–9)

// === BUFFER TEMPERATURA (PER INVIO POST-MISURA) ===

float temperatureBuffer[numSec];      
uint32_t timeBuffer[numSec];          
int tempIndex = 0;  // n dati nel buffer 
int tempSendIndex = 0;
int packetCounter = 0;  // quanti pacchetti bleData inviati (ogni 10 leggi T°)
uint32_t lastPacketTimestamp = 0;

// ============ BLE =========================== 

// creo un bluetooth Service dedicato al mio sensore
BLEService bleService("8ee10201-ce06-438a-9e59-549e3a39ba35"); 

// Unica caratteristica BLE per pacchetto IR + RED + Time (10 byte)
BLECharacteristic bleData("33303cf6-3aa4-44ad-8e3c-21084d9b08db", BLERead | BLENotify, 100); // 3+3+4 byte
// Canale di comando (bidirezionale), numero !=0 --> inizo misurazione
BLEByteCharacteristic bleCommand("bf789fb6-f22d-43b5-bf9e-d5a166a86afa", BLERead | BLEWrite | BLENotify);   
//caratteristica BLE per la temperatura (float 4 byte)
BLECharacteristic bleTemp("44404cf7-4bb5-55be-9f60-32195a8c09ec", BLERead | BLENotify, 80); 

MAX30105 particleSensor;    // creo oggetto sensore


// === FUNZIONI HELPER ===

// FUNZIONE READREGISTER - input: registro da leggere & output: byte
byte readRegister(byte reg) { 
  Wire.beginTransmission(0x57); // MAX30102 address
  Wire.write(reg); // segnala il registro da leggere
  Wire.endTransmission(false); // termina scrittura ma lascia la connessione I2C
  
  uint8_t bytesRequested = 1;
  uint8_t bytesReceived = Wire.requestFrom(0x57, bytesRequested);

  if (bytesReceived != bytesRequested) {
    Serial.print("Errore: ricevuti ");
    Serial.print(bytesReceived);
    Serial.println(" byte invece di 1.");
    return 0x00;
  }

  if (Wire.available()) {
    return Wire.read();
  } else {
    Serial.println("Errore: nessun dato disponibile!");
    return 0x00;
  }
}

// --- FUNZIONE READTEMPERATURE: leggere la temperatura del sensore ---
// Usa i registri 0x1F (parte intera) e 0x20 (frazione) con readRegister
// Output: temperatura float in °C (es. 25.25)
float readTemperature() {
  // Inizia una conversione di temperatura scrivendo 1 sul bit TEMP_EN (registro 0x21)
  Wire.beginTransmission(0x57);
  Wire.write(0x21);        // Registro di controllo temperatura
  Wire.write(0x01);        // Setta TEMP_EN = 1 (avvia conversione)
  Wire.endTransmission();


  // Legge la parte intera della temperatura (registro 0x1F) usando readRegister
  int8_t tempInt = (int8_t)readRegister(0x1F);

  // Legge la parte frazionaria della temperatura (registro 0x20)
  uint8_t tempFrac = readRegister(0x20) & 0x0F; // I 4 bit meno significativi rappresentano la frazione

  // Ritorna temperatura come somma di intero + frazione*0.0625°C
  return tempInt + (tempFrac * 0.0625f);
}

// === FUNZIONE SEND T°BUFFER: invio finale temperature ===
void sendTemperatureBufferBLE() {
  const int maxPerPacket = 10; // 10 temperature per pacchetto: 10 x 8 = 80 byte
  uint8_t tempPacket[80]; // buffer BLE da 80 Byte

  //Serial.println("=== Invio dati temperatura via BLE ===");
  //Serial.print("Totale misure da inviare: ");
  //Serial.println(tempIndex);

  while (tempSendIndex < tempIndex) {    
    int entries = min(maxPerPacket, tempIndex - tempSendIndex); // ultimo pacchetto può avere meno di 10 entries
  
    for (int j = 0; j < entries; j++) {
      memcpy(&tempPacket[j * 8], &temperatureBuffer[tempSendIndex + j], 4); // Copia 4 byte del float nella posizione j*8 del pacchetto
      memcpy(&tempPacket[j * 8 + 4], &timeBuffer[tempSendIndex + j], 4); // Copia 4 byte del timestamp nella posizione j*8 + 4
    }
    bleTemp.writeValue(tempPacket, entries * 8);

      /*/ Debug dettagliato
      Serial.print("Pacchetto inviato con ");
      Serial.print(entries);
      Serial.println(" letture:");
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
  delay(2000); //  delay per stabilità BLE  
  }
  //Serial.println("Fine invio buffer temperatura.");
  
  // Reset buffer e indici
  tempSendIndex = 0;
  tempIndex = 0;
  sendTemp = false;
}


// === FUNZIONE STOREONESAMPLE: in realtà buffer 10 campioni poi invia ===
void storeOneSample() {
  uint32_t irValue = particleSensor.getIR();
  uint32_t redValue = particleSensor.getRed();
  uint32_t timeMicros = micros();

  int offset = sampleIndex * 10; // capisce dove scrivere il sample nel buffer

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

  sampleIndex++;

  // Se ho raccolto 10 campioni, invio il pacchetto BLE, se fallisce lo notifica
  if (sampleIndex >= 10) {
    bool result = bleData.writeValue(dataPacket, 100);
    //if (!result) {
    //   Serial.println("BLE write failed!");
  // }
    sampleIndex = 0;

    packetCounter++;

    // === Quando Invio misuro una temperatura
    if (packetCounter >= 10 && tempIndex < numSec) {
      // === Quando invio aggiorno LastPacketTimeStamp
      lastPacketTimestamp = millis();
      //Serial.println("lastPacketTimestamp =  ");
      //Serial.println(lastPacketTimestamp);

      float temp = readTemperature();
      temperatureBuffer[tempIndex] = temp;
      /*Serial.println("T° letta: ");
      Serial.println(temp);
      Serial.println("all'istante: ");
      Serial.println(lastPacketTimestamp); */
      timeBuffer[tempIndex] = lastPacketTimestamp;
      tempIndex++;
      //Serial.println("tempIndex: ");
      //Serial.println(tempIndex);
      packetCounter = 0;
      }
  }
}


//FUNZIONE ISR (Interrupt Service Routine) - interrupt hardware 
// void onSensorDataReady()                
//{
//  newData = true;                       // Imposta la variabile flag newData a true quando si verifica l'interruzione
//  Serial.println("Nuovo dato nell'interrupt!"); // Stampa un messaggio di debug sul monitor seriale
//}

void setup()
{
  Serial.begin(115200); // Inizializzazione del monitor seriale
  // Arduino inizializza il sensore con ruolo di master I2C
  Wire.begin();                         // Serve per usare il sensore come master
  delay(100);                           // Breve pausa
  
  // Tentativi multipli per inizializzare il sensore 
  bool sensorFound = false;
  for (int attempts = 0; attempts < 25; attempts++) {
    if (particleSensor.begin(Wire, I2C_SPEED_FAST)) {
      Serial.println("Prova2: MAX30105  found.");
      sensorFound = true;
      break;
    }
    Serial.println("MAX30105 not found. Retrying...");
    delay(500);
  }
  if (!sensorFound) {
    Serial.println("MAX30105 was not found after multiple attempts.");
    while (1);  // blocca tutto se sensore non trovato
  }

  // Configura il sensore con parametri scelti
  byte ledBrightness = 0xFF;  // 
  byte sampleAverage = 8;     // quindi l'output è a 100 Hz
  byte ledMode = 2;           // Rosso + IR
  int sampleRate = 1600;        // Hz
  int pulseWidth = 69;        // μs
  int adcRange = 16384;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  // Abilita interrupt PPG_RDY scrivendo nel registro 0x02
  Wire.beginTransmission(0x57);
  Wire.write(0x02);              // Interrupt Enable 1
  Wire.write(0b01000000);        // PPG_RDY_EN = 1
  Wire.endTransmission();

  // Leggi il registro di stato per pulire eventuali interrupt pendenti
  Wire.beginTransmission(0x57);
  Wire.write(0x00); // Registro Interrupt Status 1
  Wire.endTransmission(false); // Non mandare stop, preparati a leggere
  Wire.requestFrom(0x57, 1);
  byte dummy = Wire.read(); // Dato letto e ignorato, serve solo per pulire

  // DEBUG // Serial.println("Interrupt PPG_RDY configurato correttamente!"); 

  // Configura l'interruttore per la gestione del nuovo dato
  //pinMode(3, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(3), onSensorDataReady, FALLING);
  
  // Controlla se il modulo BLE è stato inizializzato correttamente
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy failed!");
    while (1);
  }
  
  // Imposta il nome locale del dispositivo Bluetooth
  BLE.setLocalName("AA Pulse Oximeter");
  
  // Imposta il servizio Bluetooth da pubblicare
  BLE.setAdvertisedService(bleService);
 
  // Aggiungi caratteristiche al servizio BLE
  bleService.addCharacteristic(bleCommand); 
  bleService.addCharacteristic(bleData);    
  bleService.addCharacteristic(bleTemp);

  // Aggiungi il servizio Bluetooth
  BLE.addService(bleService);

  // Inizializza i valori delle caratteristiche
  uint8_t zeroData[100] = {0};
  bleData.writeValue(zeroData, 100);
  bleCommand.writeValue(2);

  // Pubblica servizio BLE
  BLE.advertise();
  Serial.println("Dispositivo Bluetooth attivo, in attesa di connessioni...");
}

void loop() {
  // Gestione connessione BLE
  if (!isConnected) {
    central = BLE.central();
  }

  if (central.connected()) {
    isConnected = true;

    if (bleCommand.written()) {
    uint8_t cmd = bleCommand.value();

    switch (cmd) {
        case 1:
            //Serial.println("CASE 1: meas T temp F");
            performingMeasure = true;
            sendTemp = false;
            tempIndex=0;
            break;
        case 0:
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
            //Serial.println("DEFAULT meas F temp F");
            performingMeasure = false;
            sendTemp = false;
            break;
    }
    }

    // Esegui readRegister(0x00) solo se stai effettivamente misurando
    if (performingMeasure) {
      byte intStatus = readRegister(0x00);

      if (intStatus & 0x40) { // Bit PPG_RDY = 1
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
