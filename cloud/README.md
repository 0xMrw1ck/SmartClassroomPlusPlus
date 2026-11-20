# SmartClassroom++ online monitoring

The ESP32 sends measured telemetry to Firebase over verified HTTPS. The online dashboard requires verified Google sign-in. Any signed-in Google account can add a device using its Device ID, then view and control it. Keep that ID private: knowledge of the ID grants access. Adding an account preserves the original enrollment and other viewers. Per-account membership lists permit direct device reads without allowing shared viewers to list all devices. Firmware 1.2.0 adds AUTO, Manual ON and Manual OFF for both outputs. Commands expire after 60 seconds and are applied in the main firmware loop, with acknowledgements sent back to the website. Offline, unsupported and standby controls are disabled; cloud commands never resume standby. Keep local credentials private and do not expose the local HTTP server through port forwarding.

## Provisioning

1. Create a Firebase Spark project and register a web app.
2. Enable Google and Anonymous authentication. Anonymous accounts identify devices but receive no data permissions until enrolled.
3. Create a Realtime Database in locked mode. Copy `public/config.example.json` to `public/config.json` and fill in its exact HTTPS URL and the web app's Firebase configuration. `config.json` stays out of Git.
4. Copy `database.rules.example.json` to `database.rules.json`. The actual rules file stays out of Git.
5. Sign in to the official Firebase CLI and run `firebase deploy --only hosting,database` from this folder.
6. Upload the cloud-enabled firmware. In its local Settings → Online monitoring, enable publishing and copy the Firebase API key and database URL displayed on the online dashboard. Save and restart.
7. Sign in to the online dashboard with a Google account, then add the device ID displayed in the ESP32's local cloud status. Repeat for other Google accounts using the same ID.

The default publish interval is 30 seconds. After 90 seconds without a fresh packet, the site reports the device offline and hides live watts and occupancy. History and output states are labelled as last reported. Internet loss does not stop local automation.

No payment method, custom domain, service account key, or router port forwarding is needed. Spark service quotas still apply. Keep Firebase CLI credentials and device refresh tokens private. Firebase web configuration is public; database rules enforce access.

## Energy history (firmware 1.3.0)

The ESP32 PATCHes one row per minute to `history/<device>/minutes/<YYYY-MM-DD>/<HHMM>`, hourly totals to `hours/<YYYY-MM-DD>/<HH>` and daily totals to `days/<YYYY-MM-DD>` (local time). The dashboard's **Energy report (Excel)** card downloads one `.xlsx` file built in the browser (`public/xlsx.js`, also embedded in the ESP32 dashboard; run `node tools/sync-xlsx.cjs` after editing it): Summary, Daily (all dates), Hourly (7 days ending on the chosen date) and Minute (the chosen date) sheets with filter arrows and filtered averages/min/max/totals. **Reset energy data** on the ESP32 deletes this history online as well. 1-minute rows older than 31 days are deleted by the device; hourly and daily totals are kept (about 1.5 MB per classroom per year in total).

These paths need the updated rules: copy `database.rules.example.json` to `database.rules.json` and run `firebase deploy --only hosting,database`. Until the rules are deployed, the device logs `Cloud history upload failed (401)`, live monitoring keeps working, and rows wait on the device (up to 24 h).

## Registration troubleshooting

`Firebase sign-in failed (200)` in older firmware can mean the HTTPS request
succeeded but its response was discarded because it had no `Content-Length`
header. Update the firmware with the bounded response-reader fix; changing the
Wi-Fi password or pairing a nonexistent device ID does not resolve this bug.
The corrected reader supports unknown-length and chunked bodies while limiting
responses to 16 KiB. JSON failures are reported separately from HTTP/transport
errors, without logging authentication tokens. After registration succeeds,
approve the displayed device ID on the online dashboard.

## Verification

Run `node --test tests/*.test.*` here, plus the root dashboard checks. Sharing commands retain the enrollment UID in `requesterUid` for compatibility with firmware 1.2.0; rules validate `actorUid` against the actual signed-in account and its membership. Older owner-only clients remain compatible. No firmware update is required for this access change. The firmware must also compile for `esp32:esp32:esp32s3usbotg:PartitionScheme=min_spiffs` with the libraries pinned in the release workflow. Confirm sensor readings and device enrollment on hardware before treating the site as live.
