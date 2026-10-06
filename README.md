# SmartClassroom++
ESP32-S3 classroom automation with LD2450 radar, PZEM-004T v3, SSD1306 OLED, one button, a local dashboard and GitHub OTA.

## First USB upload
Use Arduino ESP32 core 3.3.12 and board **ESP32-S3-USB-OTG**. Select **Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)**, port COM4, and Erase All Flash **Disabled**. Keep the whole SmartClassroomS3OLED folder together. Install ArduinoJson 7.4.2, Adafruit SSD1306 2.5.17 and Adafruit GFX 1.12.6. Upload SmartClassroomS3OLED.ino once over USB to install the OTA-enabled firmware and partition table.

Default pins: radar RX16/TX17, PZEM RX4/TX5, OLED SDA8/SCL9, lights relay10, fan relay11, button3 to GND. Lights vacancy:30 seconds; fan:15 seconds; button hold:5 seconds.

## Web settings
Connect to ESP32-LD2450 (default password ESP32Radar123), open http://192.168.4.1 and sign in (public-build defaults: admin / ChangeThisPassword). Your local USB build retains your previous login and Wi-Fi defaults through a private LocalProvisioning.h. That file must never be published.
Settings edits are validated and saved separately from the energy ledger. Saving turns outputs OFF and restarts to apply all changes consistently. Blank password fields retain current values; the open-network checkbox explicitly clears a home Wi-Fi password. Change timers, radar bounds, clock offset, polling, OLED refresh, button hold, network/login values, GPIO assignments and OTA repository. GPIO assignments must be distinct and exclude USB/flash pins. Do not change wiring settings without matching physical wiring.

## OTA releases
On boot, outputs stay OFF while the device waits up to20 seconds for Wi-Fi and a valid clock. A missing release/offline network continues with existing firmware. A newer semantic version with matching board, fitting OTA slot and verified SHA-256 is installed over verified HTTPS. Same/older version is skipped. Saved configuration and energy ledger remain in NVS. Manual checks enter standby; press Resume AUTO afterward if no reboot occurs.

To publish an update, change FIRMWARE_VERSION in GithubOTA.h, commit, and push matching tag v1.0.1 (example). GitHub Actions builds a public firmware.bin with no private provisioned credentials and publishes manifest.json. Only the application binary is used for OTA. First release tag must match the current version. Workflow dispatch on a branch intentionally fails the tag check.

The access point provides local control; it does not give your phone internet. On home Wi-Fi open the ESP32 LAN IP shown by OLED/Serial, not its access-point IP.

Hardware behavior and wireless OTA require verification on the actual board after the USB upload.
