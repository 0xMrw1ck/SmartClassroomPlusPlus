# Smart Classroom — ESP32-S3 + OLED + modern dashboard

## Physical button (GPIO3)

Connect a normally open momentary button between **GPIO3 and low-voltage GND**. The input uses an internal pull-up. Do not connect it to 5 V. For four-terminal tactile buttons, use contacts from opposite electrical sides.

- Single short press: cycle OLED live status, electricity and network views.
- Double short press: return both channels to AUTO. Start the second press within 400 ms of the first release.
- Hold for five seconds: force both loads OFF, save the energy checkpoint and enter STANDBY. The OLED shows a countdown; release early to cancel. Holds longer than 700 ms do not count as clicks.
- Press in standby: resume AUTO after releasing the previous hold. No extra half-second arming wait is required. The resume press cannot also change pages or trigger standby.

Standby keeps Wi-Fi, the dashboard, sensor readings and energy recording running. It does not cut electrical supply power. The website shows RUNNING/STANDBY, button countdown and Activity events. Relay controls are disabled, and the control API returns HTTP 409 during standby. Resume using a fresh physical-button press or the authenticated website Resume AUTO button. Clock and energy-save controls remain available.

Standby does not survive a restart: startup remains both outputs OFF in AUTO, and a radar detection may switch them on. Bench-test button actions and both relay indicators with mains disconnected, including cancelled holds, holding after standby, separate-press resume and web command rejection.

This version adds your confirmed **SSD1306 128×64 I²C OLED** and implements the generated dashboard concept as a responsive local webpage using live ESP32 data. Radar UART2 on GPIO16/17, PZEM UART1 on GPIO4/5, relay control, energy ledger and Serial Monitor reporting are retained.

**Extract the entire ZIP. Open SmartClassroomS3OLED.ino inside its matching folder. Keep both ClassroomFirmware.h and DashboardAssets.h beside it. All three code files are required.** Configuration is at the top of ClassroomFirmware.h. Web assets are embedded in DashboardAssets.h; no external CDN, server, SD card, website builder or internet connection is needed to view the local dashboard.

## OLED connections

| OLED pin | ESP32-S3 connection |
| --- | --- |
| VCC / VDD | 3V3, for a module rated to operate at 3.3 V |
| GND | GND |
| SDA | GPIO8 |
| SCL / SCK (I²C clock label) | GPIO9 |

Identify terminals by their labels, not their physical order. This code is for a four-pin I²C SSD1306 128×64 module; screen size alone does not identify the interface. SPI and SH1106 modules need different setup. Use I²C pull-ups to **3.3 V**, never 5 V at the ESP32 GPIO. Many OLED breakout boards already include pull-ups. Do not power a board with pull-ups to 5 V directly from 5 V while connecting it to ESP32 signals.

In Arduino IDE Library Manager, install **Adafruit SSD1306** and **Adafruit GFX Library**, accepting required dependencies such as **Adafruit BusIO**. Wire is supplied with the ESP32 core. The firmware probes 0x3C and 0x3D, uses custom GPIO8/9 and continues operating if the OLED is absent. It tolerates isolated I2C errors, marks the display offline after three consecutive failed frame transfers, and retries approximately every five seconds with an I2C bus restart. Serial Monitor reports the I2C error code. The OLED address and any failure appear in Serial Monitor.

### OLED information

The OLED displays one live screen immediately after initialization and refreshes every second:

- Date and time in Philippines time (UTC+8), including seconds.
- Room occupancy from in-zone radar detections, or RADAR OFFLINE when unavailable. Detected targets are not a guaranteed count of everyone in the room.
- Large measured watts from the PZEM, whose measurements are polled every two seconds. Unavailable/stale meter data displays W: --.
- Commanded lights and fan ON/OFF states (L/F), not physical contact feedback.

If the clock is unset, the screen shows TIME NOT SET and Set time on dashboard. Connect to internet-enabled station Wi-Fi for NTP or use the dashboard's Set ESP32 time from this device action. The clock is not battery backed.

## Dashboard and PWA behavior

- **Dashboard:** occupancy, live combined watts, today's kWh and radar status; separate lights/fan segmented AUTO/ON/OFF controls with vacancy timers; daily energy bar chart; top/perspective radar; recent events; electrical telemetry.
- **Energy history:** up to 31 recorded dates, unallocated energy and CSV download. The chart uses the last seven recorded dates, not fabricated empty calendar days.
- **Activity:** last 40 real device events, with text download.
- **Settings:** set time, save energy checkpoint, view IPs, vacancy delays, OLED status and sensor faults. Pin assignments and delays remain firmware configuration, not live editable controls.
- On phones the cards stack and navigation moves to the bottom. All assets are local; figures come from the API rather than the generated image's illustrative values.
- Controls begin disabled and enable only after a live API response. Connection loss disables controls, marks old data stale and clears active radar dots. The app does not queue commands or pretend a failed control request succeeded. After an uncertain response, check actual device state before retrying.
- Manifest, 192/512-pixel PNG app icons, standalone app metadata and a service worker are included. **The ESP32 still serves ordinary HTTP. A responsive dashboard is not the same as a fully installable HTTPS PWA.** At http://192.168.4.1 or a LAN IP, use your browser's Add to Home Screen / shortcut feature if offered; the exact menu and app behavior depend on the browser.
- Standard service-worker caching and manifest-based PWA installation require an HTTPS origin (or localhost development). The dashboard therefore registers its service worker only in a secure context. It does not claim offline caching is available on the HTTP hotspot.
- For full PWA operation later, place the ESP32 behind a local HTTPS reverse proxy with a certificate trusted by your phone. Proxy the full same-origin paths, including the APIs, manifest, icons and service worker. This package does **not** set up that proxy or a certificate.
- When served securely and cached successfully, only the static interface is available offline. Live measurements and controls always require the device. APIs, relay commands, energy exports and events are never cached by the service worker; no background control synchronization is implemented.
- Authentication remains as in your working version: **admin / ChangeThisPassword** by default. Change credentials. If you want no sign-in for a local prototype, set **REQUIRE_WEB_LOGIN = false**. Anyone who can reach the ESP32 can then use its controls.

## Preserve your existing energy data

The Preferences namespace, ledger key, structure and version marker are unchanged from the last S3 version. A normal sketch upload should keep that stored ledger. **Do not enable Erase All Flash Before Sketch Upload**, erase NVS, or reset the PZEM if you want to retain your records. Download a CSV before upgrading. The page and OLED do not create another database or expand the 31-date retention period.

Assumptions: ESP32-S3 DevKit with GPIO4, 5, 16, 17, 18 and 21 exposed, HLK-LD2450, PZEM-004T-100A **V3.0 TTL UART**, and a two-channel relay module with inputs explicitly compatible with 3.3 V ESP32 logic. Older PZEM versions, RS485 variants and boards with other pin layouts require changes. Verify your module markings before connecting power.

## Low-voltage pinouts

| Device pin | Connection |
| --- | --- |
| OLED SDA / SCL | GPIO8 / GPIO9 |
| OLED VCC / GND | 3V3 / GND for a 3.3 V compatible module |
| LD2450 VCC | Regulated 5 V |
| LD2450 GND | Low-voltage common GND |
| LD2450 TX | ESP32 GPIO16 (UART2 RX) |
| LD2450 RX | ESP32 GPIO17 (UART2 TX) |
| PZEM TTL 5V | Regulated 5 V; powers its communication optocouplers |
| PZEM TTL GND | Low-voltage common GND |
| PZEM TTL TX | 5 V to 3.3 V UART level translator → GPIO4 (UART1 RX) |
| PZEM TTL RX | GPIO5 (UART1 TX) → 3.3 V to 5 V UART level translator |
| Relay IN1 | GPIO18, through compatible input interface; lights |
| Relay IN2 | GPIO21, through compatible input interface; fan |
| Relay VCC/JD-VCC | Supply required by the relay module, commonly 5 V; follow its schematic |
| Relay logic GND | Low-voltage common GND for the specified nonisolated interface |
| ESP32 | USB power or correctly regulated board 5V/VIN supply |

TX connects to RX. Identify pins by printed labels, not connector position. LD2450 has 3.3 V UART logic despite a 5 V supply. Do not connect a 5 V UART output directly to ESP32 GPIO. Use a UART-rated translator for the PZEM in both directions; connect its low side to 3.3 V and high side to 5 V as specified by its manufacturer. A passive bidirectional I2C board is not automatically suitable for UART.

Use a regulated supply sized for the radar, ESP32 Wi-Fi peaks and both relay coils. Do not power coils from ESP32 3V3 or GPIO. Do not tie two separate 5 V supplies together through USB/VIN unless your board supports it. Share only the low-voltage signal reference where required. PZEM TTL GND is not mains neutral or earth.

The default is active-low relay control. Use `RELAY_ACTIVE_LOW = false` for active-high inputs or an interface that inverts the signal. A generic 5 V relay board may pull IN pins to 5 V; do not assume it is safe for direct ESP32 connection. For such a board use a suitably designed transistor/opto interface, and match the sketch polarity to the effective interface.

Software cannot prevent relay pulses before setup runs. The hardware interface must hold inputs in the OFF state during power-up, reset and ESP32 disconnection. For verified direct 3.3 V active-low inputs, use appropriate external pull-ups to 3.3 V; for another interface follow its circuit. Verify with mains disconnected.

## AC measurement and switching

**Mains voltage can kill. Have a qualified electrician wire and enclose the AC portion. First test the ESP32 outputs using only the relay indicators or a safe low-voltage load.** A breadboard is for the low-voltage signals only. Disconnect the mains before touching the relay terminals, meter voltage inputs or CT wiring.

The following is the functional arrangement; terminal order and ratings must come from your exact modules:

1. A protected live feeder supplies the two relay COM contacts, using separately appropriate branch protection if needed.
2. Relay channel 1 NO feeds the lights' live conductor. Relay channel 2 NO feeds the fan's live conductor. NC is unused. NO means a de-energized relay disconnects its branch.
3. Each load's neutral returns to the neutral bus. Protective earth remains continuous to equipment requiring it; neither earth nor neutral is switched by these prototype relays.
4. The PZEM voltage measurement L/N connects across the protected supply **before the relay switches**, keeping the meter powered when lights and fan are off. Its mains circuit requires AC power; the TTL 5 V input alone does not run the measurement chip.
5. The external 100 A CT goes around **one insulated live conductor feeding both measured branches**, before the live splits. Do not clamp around live and neutral together. Connect the CT using its designated meter terminals, with power disconnected; do not disconnect the CT secondary while energized.
6. Ensure no other load passes through this CT if the result should represent lights plus fan only. This single meter gives their **combined consumption**, not separate per-channel readings.

The CT's 100 A range is not the relay's switching capacity. Choose relays/contactors for actual voltage, LED-driver inrush and fan motor inductive current. A relay's printed resistive rating does not prove it is suitable for a classroom circuit. Use appropriate fuses/breakers, terminal covers, strain relief, separation and an enclosure.

## Install and upload

1. Install the Espressif ESP32 board package in Arduino IDE. Select **ESP32S3 Dev Module** (or the exact supported S3 board) and your USB port. Use the same flash size, PSRAM and USB settings as your working sketch. If using native USB for Serial Monitor, enable **USB CDC On Boot**; if using a USB-to-UART bridge connector, Serial Monitor normally uses UART0 with USB CDC On Boot disabled. Do not assign relay signals to GPIO19/20: they are native USB D-/D+.
2. Open the SmartClassroomS3OLED folder's SmartClassroomS3OLED.ino, leaving ClassroomFirmware.h and DashboardAssets.h in that folder. Install the Adafruit SSD1306, Adafruit GFX and Adafruit BusIO libraries first. WiFi, WebServer, Preferences, Wire and system time come with the ESP32 Arduino core. PZEM communication is implemented directly using V3.0 Modbus, with CRC checking and a nonblocking receive state machine; no PZEM library is required.
3. Set HOME_WIFI_SSID and HOME_WIFI_PASSWORD. Set AP_WIFI_PASSWORD and WEB_PASSWORD to your own values. Your original Wi-Fi password has not been copied into this deliverable.
4. Adjust relay polarity, `VACANCY_MS`, timezone and X/Y zone boundaries. Defaults: lights 30 seconds, fan 15 seconds, UTC+8.
5. Upload. Open Serial Monitor at **115200 baud**.
6. Connect your phone/computer to your home LAN and open the IP shown by Serial Monitor, or connect to hotspot **ESP32-LD2450**, default password **ESP32Radar123**, then open **http://192.168.4.1**.
7. Sign in with **admin / ChangeThisPassword** unless changed. Browser basic authentication over local HTTP is not encrypted; use a trusted LAN, change passwords and do not expose this server to the internet.
8. Internet-connected station Wi-Fi enables NTP clock synchronization. Hotspot-only operation needs **Set ESP32 time from this device**, using a phone/computer with the correct clock, after each reboot. A battery-backed RTC is needed if correct dated logging must work autonomously after an offline power failure.

## Automation and manual controls

- Startup: both outputs OFF, both modes AUTO. Manual modes do not survive a reboot.
- AUTO: any detected target inside the configured classroom zone turns each channel ON. Every new detection refreshes the vacancy timer. Lights turn OFF after 30 seconds without in-zone targets; fan after 15 seconds. They can have different delays by changing VACANCY_MS.
- ON: forces that channel ON until AUTO, OFF or reboot. It **overrides vacancy shutdown**.
- OFF: forces that channel OFF until AUTO, ON or reboot.
- Returning to AUTO applies the current presence/timer state immediately; an existing ON output may stay ON for the remaining hold interval; an OFF output stays OFF until a new target is detected.
- Radar UART silent for 3 seconds: AUTO holds existing relay states and reports a fault. Communication loss is not considered proof of an empty room. Manual controls remain available. Outputs remain OFF if the radar has never been available after boot. Once valid radar frames return, normal automation resumes; an already-expired vacancy timer can shut outputs OFF immediately.
- The webpage shows commanded relay states, not physical confirmation. There is no welded-contact detection or individual-load feedback.

### Radar limitations

LD2450 is a motion target tracking module, with up to three reported targets. It is not a dependable count of all students or a guarantee that a quiet room is empty. Its nominal range is about 6 m, and walls, installation angle, obstructions and movement outside the classroom affect results. A fan itself may produce unwanted detections. Keep the radar away from the fan's moving blades and test the configured zone.

For a real classroom, validate detection with people seated quietly in every part of the room for longer than both vacancy delays. If the sensor loses them, add suitable stationary-presence sensing and/or another occupancy source; merely increasing the timeout does not eliminate false vacancy detection. This code only uses the LD2450 supplied in your working sketch.

The initial sketch decoded bit 15 as negative. Hi-Link's LD2450 protocol uses bit 15 set for positive magnitude; this sketch corrects that and uses real X/Y coordinates without replacing negative Y with distance.

## Energy recording

- Meter poll: every 2 seconds. Voltage, current, power, frequency, power factor and cumulative energy appear on the page and Serial Monitor. CRC failures/timeouts are logged. Stale values are explicitly marked offline; they are not converted to zero.
- Energy comes from successive PZEM cumulative Wh readings, not estimated from instantaneous watts. Meter totals are never reset by the sketch.
- First installation establishes a baseline; pre-existing meter consumption is excluded from newly recorded daily totals.
- The ledger keeps up to **31 dates with measured intervals**. Older dated entries are replaced. Dates with no recorded interval need not appear, so this is not necessarily 31 consecutive calendar days.
- The ledger is saved in ESP32 Preferences/NVS about every 5 minutes when meter samples arrive. The **Save energy checkpoint** button saves immediately. Frequent flash writes are avoided.
- On reboot, the difference from the persisted PZEM baseline normally recovers same-date energy consumed since the checkpoint. If a measurement gap spans dates, has no valid clock, or goes backward in time, its energy goes to **unallocated**, because precise per-day attribution cannot be recovered from a cumulative meter alone. Unallocated is a lifetime bucket, separate from the rolling daily records.
- A normal midnight interval of at most 15 seconds is assigned to the ending date. Thus per-day boundaries have up to one short sampling interval of attribution error. Reboots/gaps across dates may create unallocated energy instead.
- A meter reset/decrease creates a new baseline and logs a warning; consumption lost by resetting the meter cannot be reconstructed.
- The clock is not battery backed. Setting the date late cannot reconstruct the exact prior daily distribution. Ensure time is correct before measurement, and do not erase flash or reset the PZEM during measurement.
- Download daily totals through **Download daily CSV**. Watt readings/target tracks are live telemetry, not a persistent time series. Events are the last 40 messages in RAM, downloadable as text, and are cleared on reboot. Save Serial Monitor output on your computer for a longer experiment record.

### 1-minute, hourly and daily log (firmware 1.3.0)

- Every minute the 2-second meter readings are averaged into one row: `timestamp, device_id, voltage_V, current_A, power_avg_W, power_peak_W, pf, frequency_Hz, energy_Wh, meter_total_Wh, lights, fan, mode, occupied, samples, meter_ok`. Energy is integrated from measured watts; minutes with no meter reading have blank electrical columns, never zeros.
- Hourly rows (`hour_start, energy_kWh, avg_W, peak_W, min_W, avg_V, avg_pf, occupied_min, lights_on_min, fan_on_min, minutes_recorded`) and daily rows (`date, energy_kWh, avg_W, peak_W, peak_time, occupied_h, lights_on_h, fan_on_h, minutes_recorded, est_cost`) are built from the minute rows. `minutes_recorded` below 60/1440 means part of the period had no data.
- On the device: last 24 h of minutes (`/minute.csv`), 7 days of hours (`/hourly.csv`), 31 days of days (`/daily.csv`), stored with LittleFS in the existing 128 KB `spiffs` partition. Rows are written to flash every 10 minutes and before standby, settings saves and updates, so a power cut loses at most 10 minutes. The legacy `/energy.csv` ledger, settings and cloud pairing are unchanged by the OTA update.
- Set the electricity rate on the History page to fill `est_cost`. It is stored separately from the main settings.
- With online monitoring enabled, every minute row plus the hourly/daily totals are uploaded to Firebase under `history/<device>/`. Rows recorded while offline are uploaded when the connection returns (up to the 24 h the device keeps). 1-minute rows older than 31 days are removed online; hourly/daily totals stay.

### Clock

With internet, the clock syncs automatically from `ph.pool.ntp.org` / `time.google.com` as soon as Wi-Fi connects and then hourly. All dates and times use the Timezone setting (default 480 minutes = Philippines UTC+8) in 24-hour format. **Set ESP32 time from this device** is only needed without internet.

## Bench verification before AC deployment

1. Check relay OFF through boot, USB reconnection and reset with mains disconnected; then test ON/OFF independently from the webpage. Confirm labels match the physical channels.
2. For a quick test, temporarily set delays to 15000 and 10000 milliseconds. Enter the radar zone, then leave; confirm fan and lights shut off at different delays. Restore realistic delays afterward.
3. Test seated stillness, back corners, hall movement, fan motion and radar unplugging. On unplug, AUTO should hold its output states and flag a fault.
4. With the AC section professionally wired, compare PZEM readings against a known meter/load. Switch each branch independently and observe combined consumption changes.
5. Set time, collect energy, save checkpoint and reboot. Check same-date recovery, meter-disconnection behavior, CSV export, and correct unallocated treatment across unknown-date intervals.

Validation: C++17 syntax checked with lightweight Arduino and Adafruit API substitutes; dashboard DOM-logic checks passed for live metrics, chart/history, commands, navigation, faults and disconnected control rejection; service-worker checks confirmed dynamic requests bypass caching; manifest and PNG dimensions checked; embedded JavaScript syntax checked with Node; host tests passed for OLED initialization/page update and radar frame/sign decoding, Modbus CRC, separate vacancy cutoffs, manual modes, radar-fault hold, daily meter deltas, unknown-time allocation and meter reset. These substitutes do not validate the real ESP32 APIs or hardware. Visual browser rendering could not be checked because a usable browser download was unavailable. The ESP32 Arduino toolchain and physical hardware were unavailable, so this is not board-compiled or hardware-tested. Compile in Arduino IDE and complete the bench checks before wiring AC loads.

## Primary references

- Hi-Link LD2450 instruction manual: https://device.report/m/455891e75058d6eb3127cce6fb7b6a470e2f1e02443357cfced104985d55ccfd
- Hi-Link LD2450 protocol V1.03: https://device.report/m/4724663b9656799560c29bc06a1b7910f4eb185d4575e546e301da03c3d2a869
- PZEM V3.0 protocol/library implementation and AC/TTL power requirements: https://github.com/mandulaj/PZEM-004T-v30

ESP32-S3 USB pin and Serial references:
- https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html
- https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html

Additional primary references:
- https://github.com/adafruit/Adafruit_SSD1306
- https://developer.mozilla.org/en-US/docs/Web/Progressive_web_apps/Guides/Making_PWAs_installable
- https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API

OLED freeze diagnostics: Serial Monitor prints OLED frames, lastAttemptAge, I2Cerror and free heap every five seconds. Frame transfers use checked 16-byte I2C payloads; three consecutive failed transfers trigger bus recovery. PZEM serial processing has per-loop byte budgets so noisy traffic cannot monopolize the main loop. A successful transfer does not prove the panel physically updated. If the screen freezes, capture these diagnostics and note whether the web dashboard still updates.

The button uses ButtonInput.h plus compile-time ButtonInputChecks.h regression tests; keep both headers beside the sketch. Resuming resets the OLED to live status and reinitializes the display bus/controller. Standby keeps measured watts visible. A button held at startup must first be released. The state machine requires a debounced LOW press and HIGH release; if the electrical level never changes, check the GPIO3/GND button connections.

Firmware button-v4: AUTO vacancy timers are lights 30 seconds and fan 15 seconds from the last in-zone detection. Serial prints a hold countdown. A recorded hold of five seconds is recognized even if a slow loop only samples the subsequent release. If no PRESSED or countdown messages appear, inspect the GPIO3-to-GND button wiring; software cannot detect a press without a LOW input.
