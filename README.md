### PPG and Temperature Signal Acquisition & Analysis via BLE
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
This repository provides firmware code for acquiring and transmitting photoplethysmographic (PPG) signals and temperature data using Arduino MKR WiFi 1010 and the MAX30102 sensor. Furthermore, an illustrative MATLAB pipeline for the collection and plotting of data, then focusing on signal quality assessment through Signal-to-Noise Ratio (SNR) computation, is provided.
While BLE communication is established between Arduino and MATLAB, Arduino and the MAX30102 sensor communicate via I2C transmission, hence a USB cable is required.
The pipeline has been tested for a maximum "stress" duration of ten minutes.

### Required Materials
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
**Hardware:**
- Arduino MKR WiFi 1010
- MAX30102 sensor (for Red/IR PPG and temperature)
- USB cable, Dupont cables 

**Software:**
- Arduino IDE (with BLE libraries)
- MATLAB (with BLE support)
- Git (optional, for version control)

### Measurement Protocol
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
The system performs repeated acquisitions of PPG and temperature signals at **five different LED intensities** (10, 20, 30, 40, 50 mA). 
For each intensity, **five repetitions** are recorded to assess signal quality and variability.

The analysis focuses on computing the **Signal-to-Noise Ratio (SNR)** for both Red and Infrared channels, 
using spectral methods that isolate the heart rate frequency band (typically 40–180 bpm). 
Temperature data is also collected to monitor sensor stability and environmental conditions.

### Workflow Overview
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

1. **Upload the Arduino firmware (`heartbeat7`)**
   - Initializes the MAX30102 sensor
   - Sets up BLE services and characteristics
   - Transmits Red/IR signals continuously and temperature data in batches

2. **Run the MATLAB Live Script (`SNR.mlx`)**
   - Connects to the Arduino via BLE using `heartbeat_BLE.m`
   - Acquires Red, IR, and temperature signals for a configurable duration
   - Computes Power Spectral Density (PSD) and SNR
   - Saves the results in `.mat` files

3. **Run the MATLAB Script (`SNR_data_extraction.m`)**
   - Loads `.mat` files from multiple repetitions and intensities
   - Extracts and saves figures (signal, temperature, PSD)
   - Computes raw SNR values, mean ± standard deviation, and median
   - Exports results as `.csv` tables and `.png` images

### Data Saving Guidelines
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
To ensure compatibility across scripts:

- Use `SNR.mlx` to save each acquisition as a `.mat` file with a clear naming convention (e.g., `SNR_10mA_rep1.mat`)
- Store all `.mat` files in a dedicated folder before running `SNR_data_extraction.m`
- The extraction script will automatically organize outputs into:
  - `images/signal`, `images/temperature`, `images/PSD`
  - `tables`: raw and statistical SNR data

### Integration Notes
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
All components are designed to work together seamlessly:

- `heartbeat7` (Arduino) → BLE transmission
- `heartbeat_BLE.m` (MATLAB) → BLE reception
- `SNR.mlx` → Acquisition and SNR computation
- `SNR_data_extraction.m` → Batch analysis and export

### Components
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
For detailed descriptions of each component, refer to the documentation in the `Docs/` folder.

## heartbeat7
Arduino firmware that initializes the MAX30102 sensor and handles BLE transmission of Red/IR signals and temperature data. 
It operates in two modes: continuous acquisition and batch temperature transmission. Data is sent in structured packets with timestamps.
📄 See: heartbeat7_description.md

## heartbeat_BLE
MATLAB function that connects to the BLE device, receives Red/IR and temperature data, and stores it in structured arrays. 
It uses callbacks for asynchronous data reception and supports configurable acquisition duration.
📄 See: heartbeat_BLE_description.md

## SNR (Live Script)
MATLAB Live Script that uses `heartbeat_BLE` to acquire data, compute Power Spectral Density (PSD), 
and calculate Signal-to-Noise Ratio (SNR) for IR and Red channels. It also visualizes signals and saves results to `.mat` files.
📄 See: SNR_description.md

## SNR_data_extraction
Processes `.mat` files from repeated measurements at different LED intensities. 
Extracts SNR values, generates plots (signal, temperature, PSD), and exports statistical tables.
📄 See: SNR_data_extraction_description.md

