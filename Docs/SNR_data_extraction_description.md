### SNR_data_extraction:
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
This MATLAB script processes and analyzes data acquired from an Arduino-based system using the MAX30105 sensor.
It collects photoplethysmographic (PPG) signals—specifically Red and Infrared (IR)—and temperature data
at five different LED intensities (10–50 mA), with five repeated measurements per intensity.
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

FOLDER SETUP ----------------------------------------------------------------------------------------------------------------

* Creates subfolders for saving exported figures and tables:

  * images/signal: PPG signal plots
  * images/temperature: Temperature plots
  * images/PSD: Power Spectral Density plots
  * tables: CSV tables with raw and statistical SNR data

DATA EXTRACTION ------------------------------------------------------------------------------------------------------------

* Iterates over each LED intensity (10mA, 20mA, ..., 50mA)
* For each intensity:

  * Loads 5 .mat files containing:

    * SNR\_ir\_dB: Signal-to-noise ratio for IR channel
    * SNR\_red\_dB: Signal-to-noise ratio for Red channel
    * figSignals, figTemp, figPSD: MATLAB figures

  * Stores SNR values in a structured variable
  * Exports figures as .png images
  * Closes figures to free memory

STATISTICAL ANALYSIS --------------------------------------------------------------------------------------------------------

* Computes:

  * Raw SNR values for each repetition
  * Mean ± Standard Deviation
  * Median

* Separately for IR and Red channels

TABLE GENERATION ------------------------------------------------------------------------------------------------------------

* Creates two types of tables:

  * Raw SNR values: 5 repetitions per intensity
  * Statistical summary: Mean ± SD and Median

* Saves all tables as .csv files in the tables folder:

  * SNR\_IR\_raw.csv
  * SNR\_Red\_raw.csv
  * SNR\_IR\_stats.csv
  * SNR\_Red\_stats.csv

OUTPUT ----------------------------------------------------------------------------------------------------------------------

* PNG images of signal, temperature, and PSD plots
* CSV tables with raw and statistical SNR values
* Console message confirming successful extraction

INTEGRATION ------------------------------------------------------------------------------------------------------------------
This script is designed to work in conjunction with:

* Arduino acquisition system using BLE
* MATLAB scripts for BLE data reception (e.g., heartbeat\_BLE, SNR)
* .mat files generated during acquisition
