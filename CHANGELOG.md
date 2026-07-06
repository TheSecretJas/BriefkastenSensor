# Changelog

All notable changes to this project will be documented in this file.

## [1.0.2] - 2026-06-08
### Added
* **Documentation:** Added clear logging and info regarding the automatic hysteresis logic (clarifying that a manual reset of the mail flag is not required).

### Changed
* **Power Management:** Optimized the deep sleep sequence to prevent leakage currents. I2C pins are now set to floating, the LoRa module is explicitly put into sleep mode, and the Vext power rail is disabled before entering deep sleep.

### Fixed
* **I2C Communication:** Enabled internal pull-up resistors (`machine.Pin.PULL_UP`) for SDA (Pin 41) and SCL (Pin 42) during initialization to prevent the VL53L0X sensor from freezing and locking the system state.

## [1.0.1] - 2026-06-06
### Added
* **CryptoEngine:** Implemented AES-CBC encryption with PKCS7 padding and a randomized IV. The AES key is now securely retrieved from Non-Volatile Storage (NVS).
* **State Persistence:** Added mailbox and battery state persistence utilizing the RTC memory to retain data across deep sleep cycles.

### Changed
* **TransmitterNode:** Refactored the main node logic to significantly improve overall power management, battery monitoring, and sensor checks.
* **Battery Monitoring:** ADC control is now specifically toggled during voltage reads to conserve power. Raw ADC values are properly converted to actual physical voltage.
* **Hysteresis Logic:** Introduced battery hysteresis thresholds (low/high) to reduce false state flips. 
* **Power Management:** Increased deep-sleep duration to 1 hour to reduce wakeups and extend battery life.
* **Code Quality:** Renamed and documented distance thresholds, applied structural cleanups, and enhanced logging to strengthen system reliability and message confidentiality.

## [1.0.0] - 2026-03-15
### Added
* **Initial Release:** Initial commit of the project.
* **Documentation:** Created project structure and added the initial README file.
