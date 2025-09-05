### SNR (BLE PPG Signal to Noise Ratio Analysis Live Script):
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
`SNR` is a MATLAB Live Script designed to acquire and analyze photoplethysmographic (PPG) signals—specifically **Infrared (IR)** and **Red**—along with **temperature data** via Bluetooth Low Energy (BLE). It uses the `heartbeat_BLE` function to interface with an Arduino MKR WiFi 1010 running custom firmware.
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

## Workflow
DATA ACQUISITION ----------------------------------------------------------------------------------------------------------------
   - Calls `heartbeat_BLE(duration)` to start BLE acquisition for a specified duration (default: 40 seconds).
   - Receives IR, Red, and temperature data from the MAX30105 sensor.

SIGNAL EXTRACTION ----------------------------------------------------------------------------------------------------------------
   - Extracts and aligns timestamps.
   - Filters out invalid temperature readings (e.g., initial zero values).

VISUALIZATION ----------------------------------------------------------------------------------------------------------------
   - Plots IR and Red signals over time.
   - Displays temperature trend and computes average temperature.

SPECTRAL ANALYSIS ----------------------------------------------------------------------------------------------------------------
   - Computes Power Spectral Density (PSD) using the `periodogram` method.
   - Focuses on the heart rate frequency band (40–180 bpm).

SNR CALCULATION ----------------------------------------------------------------------------------------------------------------
   - Calculates signal power inside and outside the heart rate band.
   - Computes Signal-to-Noise Ratio (SNR) and converts it to decibels (dB).

SAVING RESULTS ----------------------------------------------------------------------------------------------------------------
   - Prompts the user for a filename.
   - Saves the workspace to a `.mat` file.
   - Includes overwrite protection if the file already exists.

## Key Features
- Time-aligned signal and temperature plotting
- PSD and SNR computation for IR and Red signals
- Configurable acquisition duration
- User-controlled data saving

## Dependencies
- `heartbeat_BLE.m`: MATLAB function for BLE data acquisition
- Arduino firmware (`heartbeat7`): handles sensor data transmission via BLE

## Notes
- To change the save location, manually update the `fixedPath` variable in the script.
- Ensure the Arduino device is powered and broadcasting BLE before running the script.

