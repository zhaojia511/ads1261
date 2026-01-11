Using ESP32C6 and TI's ADS1261 ADC to measure four loadcells.

## Loadcell Design
- Use ADS1261's differential mode to connect loadcells to differential inputs
- Use excitation voltage as external reference voltage (ratiometric measurement)
- Configure PGA gain to 128 with high output data rate

## References
- TI's ADS1261 datasheet
- TI Precision Lab's GitHub repositories
- ADS1261 sample code

## Hardware and Toolchain

* ESP32C6
* TI ADS1261 ADC
* TI Precision Lab GitHub repositories
* ESP-IDF
* C
* CMake
* GCC

## Setup Instructions

### 1. Configure VSCode to use ESP-IDF
- Install the ESP-IDF extension for VSCode
- Run `ESP-IDF: Configure ESP-IDF extension` from the command palette
- Select the ESP-IDF version and installation method

### 2. Configure Toolchain
- Install ESP-IDF: `git clone --recursive https://github.com/espressif/esp-idf.git`
- Run the install script: `./install.sh esp32c6`
- Set up environment: `. ./export.sh`

### 3. Sample Code for ADS1261
- Initialize SPI communication with ESP32C6
- Configure ADS1261 registers:
  - Set input multiplexer to differential mode
  - Configure PGA gain to 128x
  - Set output data rate to high speed
- Read ADC values from the four loadcell channels
- Apply calibration and scaling factors

## Project Status
- [x] VSCode and ESP-IDF configuration
- [x] Toolchain setup
- [x] ADS1261 sample code provided
