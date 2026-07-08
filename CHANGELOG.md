# Changelog
All notable changes to this project will be documented in this file.

## [2.1.1] - 2026-07-08
### CHanged
* **CHanged:** Changed thresholds for mail detection


## [2.1.0] - 2026-07-08
### Added
* **TLS Security:** Embedded the Google Trust Services GTS Root R1 certificate and switched the SMTP client from `setInsecure()` to `setCACert()`, so the Gmail server certificate is now properly validated instead of trusted blindly.
* **Time Sync:** Added an NTP sync step on the gateway after WiFi connect, required for correct certificate validity checking.

## [2.0.0] - 2026-07-06
### Added
* **Full C++ Port:** Rewrote the entire project from MicroPython to C++ (PlatformIO, Arduino framework) to significantly reduce RAM usage. Three build environments (`sender`, `gateway`, `provision`) replace the previous single-script MicroPython layout.
* **LAN Web Portal:** New gateway web portal served via mDNS at `http://briefkastensensor.local/`, protected by HTTP Basic Auth (`PORTAL_PASS`). Displays live sender status (mail/battery flags, voltage, distance, last contact) and gateway room climate, and provides a JSON status API polled every 5 seconds.
* **Runtime Configuration:** Web portal now allows changing WLAN, SMTP, and recipient settings after initial provisioning, without reflashing. Password fields are write-only (blank = unchanged); the AES key remains provisioning-only for security.
* **Remote Flag Reset:** Added a Class-A-style downlink mechanism. The sender now transmits a `SYNC` packet (flags, battery voltage, distance) on every hourly wake cycle and opens a 2-second RX window; the gateway queues a `CMD RESET FLAGS` command from the portal and delivers it in that window, with the sender confirming via a second SYNC.
* **Provisioning via .env:** Replaced `setup_NVS.py` with a PlatformIO pre-build script (`scripts/load_env.py`) that injects `.env` values as compile-time defines for a one-time provisioning firmware, so credentials never live in a flashed file system.
### Changed
* **Crypto:** Ported the AES-CBC encryption/decryption to C++ using mbedtls, keeping the existing packet format (16-byte random IV + PKCS7-padded ciphertext) and NVS layout (`sys_sec` namespace, same blob keys) for full compatibility with already-provisioned chips.
* **State Persistence:** Replaced `rtc.memory()` string parsing with native `RTC_DATA_ATTR` variables; a power cycle now resets both flags automatically, making a separate reset script unnecessary.
* **Email Dispatch:** Rebuilt the SMTP client with watchdog-safe retry logic (up to five attempts) and deferred dispatch (~800 ms) so a long-running email send cannot block the sender's short LoRa receive window.
* **Reliability:** Added a task watchdog on the gateway main loop, fed both during normal operation and during SMTP transmission.
### Fixed
* **Portal Security:** Ensured stored secrets (WLAN/SMTP passwords) are never echoed back by the config API, and that the AES key cannot be modified via the web interface under any circumstances.

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
