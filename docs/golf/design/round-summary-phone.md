# Round summary export to iPhone

Status: Implemented locally; device acceptance pending · Updated 2026-09-05 · Repository baseline: `6c717d5`

## Product decision

Use a temporary **X4 Wi-Fi hotspot**. The iPhone joins it and opens a report served by the X4. Both devices can complete viewing and downloading without internet, including first use.

The hotspot approach is approved for the first release. Optimize it for **download one readable file, then attach it to the existing Discord/OpenClaw conversation**. ChatGPT is an alternative destination using the same data. No account integration or agent credentials belong on the X4. Implementation details and acceptance targets below remain subject to validation on hardware.

```text
X4: Send to phone → temporary hotspot
iPhone: join → open in Safari → view report → download file
X4: Done → hotspot off
iPhone: cellular / normal Wi-Fi → Files → share attachment → Discord / ChatGPT
```

Transfer from X4 to phone is offline. Sending to Discord or ChatGPT requires the phone to have internet again. A downloaded file survives disconnection; an X4-local webpage address cannot be opened by a remote agent.

QR codes help the phone join Wi-Fi and open the local report. Hosted viewers, compressed round payloads in QR codes, and cloud upload are outside the first release.

## First release and future iPhone app

The first release uses Safari and Files on the iPhone, with a complete TXT report as the primary download and CSV/JSON as secondary exports. It requires no native app installation. Viewing and saving work offline; the user reconnects to the internet before sending the file to an agent.

A native iPhone app for the course-map app is a possible future direction, not a dependency or deliverable for this release. It could later provide a dedicated round library and import experience. Preserve the versioned JSON export and keep report data separate from HTML presentation so a future app can consume the same recorded information. App features, automatic synchronization, device pairing, and any additional API remain future design decisions.

## Evidence and integration points

| Repository evidence | Design consequence |
| --- | --- |
| The network activity already starts AP mode, gets its local IP, starts DNS, and starts a web server ([CrossPointWebServerActivity.cpp, lines 214–264](../../../src/activities/network/CrossPointWebServerActivity.cpp#L214)). | The platform supports the essential offline connection. Reuse its demonstrated network APIs in a golf-owned activity. |
| Existing AP configuration uses a fixed name and no password ([same file, lines 23–28](../../../src/activities/network/CrossPointWebServerActivity.cpp#L23)). | The golf session needs a distinguishable name and temporary credentials. |
| The general web server exposes uploads and filesystem operations ([CrossPointWebServer.cpp, lines 143–158](../../../src/network/CrossPointWebServer.cpp#L143)) and allocates a 4 KiB upload buffer ([CrossPointWebServer.h, lines 41–49](../../../src/network/CrossPointWebServer.h#L41)). | Use a small read-only export server with routes for the selected round. General file management is unnecessary for this workflow. |
| Exiting the current network activity restarts the device ([CrossPointWebServerActivity.cpp, lines 100–118](../../../src/activities/network/CrossPointWebServerActivity.cpp#L100)). | Returning directly to round review without reboot is a new lifecycle requirement and must be verified. |
| A full four-player round is fixed at 906 bytes ([GolfRound.h, lines 6–45 and 60–63](../../../src/golf/GolfRound.h#L6)). | Retain one immutable round snapshot per export session, then stream outputs. |
| Archived review already owns the full round and player slot ([GolfHistoryRoundMenuActivity.cpp, lines 30–41](../../../src/activities/golf/GolfHistoryRoundMenuActivity.cpp#L30)). | Add Send to phone here and default to the reviewed player. |
| Finish retains a totals-only summary ([GolfRoundMenuActivity.cpp, lines 26–47 and 108–135](../../../src/activities/golf/GolfRoundMenuActivity.cpp#L108)); archiving stamps a validated copy and clears active state ([RoundArchive.cpp, lines 806–810 and 865–877](../../../src/golf/RoundArchive.cpp#L806)). | Finished export must load the exact committed archive, not reconstruct detail from totals or reference cleared state. |
| History falls back to summary when the detailed file cannot load ([GolfHistoryActivity.cpp, lines 142–172](../../../src/activities/golf/GolfHistoryActivity.cpp#L142)). | Preserve a clearly labeled summary-only export. |

Implementation stays within golf directories and the integration surfaces in [ARCHITECTURE.md, lines 39–51](../ARCHITECTURE.md#L39). The evidence table describes the pre-implementation baseline; the delivery notes below describe the implemented behavior.

## X4 and iPhone interaction

1. Add **Send to phone** to the finished-round summary, archived round menu, and active round menu. Active exports are explicitly marked **In progress**. Finishing remains an archive operation independent of transfer success.
2. Open the selected player's session directly. The X4 shows **Join Scorecard-XXXX**, a Wi-Fi join QR, and readable credentials as fallback. Use a unique device suffix and fresh session password; no password entry in the normal flow.
3. When a phone associates, show **Open report** with a short local HTTP URL QR. Also allow Confirm to switch between join/open screens in case automatic association detection is delayed. Association means connected to Wi-Fi, not report received.
4. If a captive portal opens, it provides an optional welcome screen. The dependable download path is full Safari: scan the report URL or enter the displayed address there. Do not promise automatic opening or depend on captive-browser download support.
5. The Safari page immediately shows the round, with **Download for your agent** as the primary action. Supporting copy says **Saves a text file. Find it in Files → Downloads.**
6. After downloading, the page explains **Open the file to check it, then press Done on your X4. Reconnect to the internet to send it.** Done closes the hotspot and returns to review.
7. On the iPhone, open the file in Files and attach it to the existing Discord/OpenClaw conversation or a ChatGPT chat.

One Wi-Fi QR cannot both join a network and open an arbitrary webpage. Budget for two scans; captive-portal launch is only an optional shortcut. Include manual SSID/password/address recovery. Derive the report URL from the actual AP address; do not hardcode an assumed IP.

Show **Ready**, **Phone connected**, **Report opened**, and **Download served** only for events the X4 can observe. A completed HTTP response does not prove that iOS saved the file or that an agent received it. Do not shut down the session automatically after a download, because the user may want another format or retry.

Back/Done closes the session. Use a five-minute idle timeout, refreshed by meaningful report requests and downloads, not DNS probes. Bound stalled connections so they cannot prevent cleanup. Keep the device awake only for this bounded session. No countdown animation or continuously refreshed QR is needed on e-ink.

Device copy uses `tr()` and source translation keys. Use mapped buttons, UITheme/FreeInkUI, oriented dimensions, and bezel-safe layout. Freeze the exported player and round for the session; returning to the menu allows choosing another player. All enabled slots are supported. A group export is a later explicit option so other players' names are not included unexpectedly.

## Where the file appears on an iPhone

Safari downloads are accessible in **Files → Browse → Downloads**, and Safari's download list provides quick access. The storage location depends on Safari settings: iCloud Drive, On My iPhone, or a chosen folder. Explicit **Save to Files** lets the user choose the location. [Apple download guide](https://support.apple.com/en-gb/guide/iphone/iphc9cd7266c/ios), [Safari download settings](https://support.apple.com/en-gb/guide/iphone/iphb3100d149/26/ios/26)

For an offline test, choose **On My iPhone** and verify that the file opens after leaving the hotspot. A locally completed download does not need iCloud sync to be sent later. The X4 cannot choose the user's Files folder.

The implementation uses a sanitized course and stable player slot, for example `scorecard-2026-09-05-moganshan-p1.txt`; it uses `undated` when the recorded date is unavailable. The full player name is inside the report. Safari handles repeated download filenames. An archive identifier in the download filename remains a follow-up; these filenames are not unique round IDs, and must not be used for automatic deduplication.

For sharing, Apple supports touching and holding a file and choosing **Share**. App destinations depend on installed share extensions; the app's attachment picker is the fallback. [Apple Files sharing guide](https://support.apple.com/guide/iphone/send-files-from-the-files-app-iphf2746307f/ios)

## Files and agent handoff

| Action | Output | Purpose |
| --- | --- | --- |
| View report | Local HTML page | Comfortable immediate review in Safari |
| Download for your agent | UTF-8 `.txt` | Primary portable file, readable by a person and an agent |
| Download table | UTF-8 `.csv` | Spreadsheet analysis and comparison across rounds |
| Download data | Versioned `.json` | Lossless structured ingestion and future automation |
| Save formatted report | Self-contained `.html`, secondary | Formatted archive with no external assets; verify iOS preview behavior |

The default TXT includes a readable overview, all recorded hole details, penalty events, units, availability flags, and a brief data dictionary. It contains enough data for detailed analysis by itself. It is not a screenshot, compressed archive, or summary with missing holes. JSON/CSV are secondary choices rather than mandatory additional downloads.

The HTML export contains pre-rendered data and inline styles so opening it does not require scripts or connectivity. PDF is optional through Safari's own export/print facilities; do not introduce an X4 PDF generator in the first release.

### Discord and OpenClaw: recommended destination

Use the existing bot DM or authorized channel. From Files, share the TXT to Discord if offered; otherwise use Discord's file attachment flow. Mention the bot if that channel's configuration requires it. Discord supports document attachments, while OpenClaw documents inbound document extraction into model-visible content. Exact handling depends on the installed OpenClaw version, channel configuration, runtime, and model. Verify one real attachment before calling the integration supported. [Discord attachments](https://support.discord.com/hc/en-us/articles/25444343291031-File-Attachments-FAQ), [OpenClaw document handling](https://docs.openclaw.ai/nodes/media-understanding), [OpenClaw Discord configuration](https://docs.openclaw.ai/channels/discord)

Recommended accompanying message:

> Analyze this golf round. Check the totals, identify my three biggest opportunities, and suggest two practice priorities. Compare with previous rounds only if you have their data.

Recommend OpenClaw first because it is the user's existing workflow. Prior conversation history is useful only when available to that session. Do not assume the agent automatically stores or indexes attachments permanently. A future explicit import workflow could archive originals and detect duplicates; no bot configuration or message sending is part of this firmware feature.

The first end-to-end test asks the agent to state player, course, holes entered, gross strokes, and penalties. Confirm those match the file before relying on coaching analysis. If extraction fails, copy the report text into a message or fix attachment ingestion; sending the private X4 URL cannot substitute for the attachment.

### ChatGPT: alternate destination

Attach the TXT or CSV through the ChatGPT app's available attachment menu and choose the downloaded file, or paste report text into the chat. Menu labels and attachment availability must be verified on the user's app/account. ChatGPT supports analysis of attached documents and data exports; CSV analysis is described in official guidance. [ChatGPT file guidance](https://learn.chatgpt.com/docs/use-chatgpt#attach-files), [CSV analysis example](https://learn.chatgpt.com/use-cases/clean-messy-data)

Use ChatGPT for an independent analysis or its preferred presentation. OpenClaw history does not automatically follow the file into ChatGPT: attach any earlier rounds needed for comparison. Do not require the ChatGPT app to appear in the iOS share sheet.

There is no direct **Send to Discord/OpenClaw/ChatGPT** promise on the local webpage. HTTP pages cannot rely on the secure-context Web Share API, and neither third-party service can fetch a private hotspot address. Use normal file downloads and the iPhone's app/Files sharing tools. A selectable text report is the fallback when clipboard APIs are unavailable. [Web Share specification](https://www.w3.org/TR/web-share/)

## Detailed data and correctness

The report includes player name and stable slot; course; tee; recorded date or unavailable; active/archived status; hole count; entered state; per-hole par, SI, yards, putts, inside/outside-100 strokes, gross strokes, and complete penalty events. Overview and statistics include front/back totals, played par, to-par when available, short-game strokes excluding putts, one-putt/three-or-more-putt holes, and worst holes.

Reuse firmware scoring helpers for every rendered/downloaded format ([GolfStats.cpp, lines 13–15, 30–49 and 91–102](../../../src/golf/GolfStats.cpp#L30)):

- Entered means `in100 + out100 != 0`; unentered holes show a dash.
- Gross is `in100 + out100 + penaltyStrokes`. Putts are already included in in100.
- Short-game strokes excluding putts are `in100 - putts`.
- Round totals and played par include entered holes only.
- Hazard adds one penalty stroke; OB adds two. Do not replay penalty mutations during export: the stored counters already reflect them ([GolfPenalty.cpp, lines 47–68](../../../src/golf/GolfPenalty.cpp#L47)).
- Missing par, SI, yards, dates, or historical penalty tracking remain unavailable. They are not recorded zero values.

TXT explains these meanings to prevent agents double-counting putts or interpreting absent fields as zero. It includes per-hole penalty field and kind, not just counts. No inferred GPS, shot locations, FIR/GIR, handicaps, or notes are produced.

Define `schema: "scorecard.round-export"`, `version: 1`, scoring-rules version, source status, selected slot, data availability, and complete hole objects in JSON. Name this as an export schema, not the full device archive format. CSV has one row per actual hole, explicit availability/entered columns, raw counters and derived values, and lossless event data. Quote values and neutralize spreadsheet formulas in user-controlled text. Validate and escape all user text in HTML, headers, and filenames.

Keep existing archive JSON unchanged. Add optional caller-owned output storage for the exact committed archive filename to the golf archive API ([RoundArchive.h, line 24](../../../src/golf/RoundArchive.h#L24)). Populate it for complete and cleanup-pending commits, including recovery. The finished summary retains this identifier; export reloads the committed record. Never assume the last index row identifies the right round.

Carry loader provenance where needed to distinguish legacy/unrecorded and repaired data; current loading can repair a round ([GolfRoundFile.cpp, lines 21–46](../../../src/golf/GolfRoundFile.cpp#L21)). If detail is absent, offer **Download summary** with a visible limitation. Export cannot undo a committed round or silently fabricate missing detail.

## Firmware and local web server

Implemented components:

| Component | Responsibility |
| --- | --- |
| `src/activities/golf/GolfRoundExportActivity.{h,cpp}` | Session state, immutable snapshot, QR display, timeout and teardown |
| `src/golf/GolfExportServer.{h,cpp}` | Bounded read-only HTTP routes and report/download counters; association belongs to the activity |
| `src/golf/GolfRoundExport.{h,cpp}` | Shared streaming formatters for HTML/TXT/CSV/JSON |
| `src/golf/GolfQrCode.{h,cpp}` | Fixed version-5/ECC-M encoder with activity-owned workspace |
| `src/golf/GolfExportStrings.cpp` and English translation YAML | Translated device/report copy; inline HTML/CSS resides in the formatter |
| Golf test suites | Export fixtures, escaping, errors and lifecycle regression checks |

The session serves only the chosen snapshot:

| Route | Behavior |
| --- | --- |
| `GET /` | Local report and download/help links |
| `GET /round.txt` | Primary agent report attachment |
| `GET /round.csv` | Table attachment |
| `GET /round.json` | Structured attachment |
| `GET /round.html` | Self-contained formatted report |
| Other routes, including captive probes | 404; no captive DNS service in this implementation |

Use correct MIME types, UTF-8, sanitized `Content-Disposition: attachment`, and `Cache-Control: no-store` for session responses. Stream with an exact counted content length where practical; a counting sink and output sink use the same immutable snapshot. No user filesystem paths are accepted. Omit general upload, deletion, settings, WebSocket, and directory-listing routes.

The implementation allows one associated station and one HTTP client at a time. A nonblocking socket is polled from the activity loop, with a 15-second absolute client deadline, 1 KiB request buffer, 2 KiB output block, and 64 KiB maximum response. A counting pass rejects formatter overflow before publishing successful headers. There is no new task or general WebServer instance. Back and timeout handling continue between polls; device responsiveness still needs measurement.

Use a temporary password-protected AP with a distinguishable SSID. Generate credentials from an SDK-verified entropy source during implementation, not an assumed unavailable API or a predictable clock seed. Serve HTTP locally; a self-signed HTTPS certificate would add browser setup. All report assets and translations must be local; no analytics, CDN, fonts, or remote API calls.

The QR dependency already exists ([platformio.ini, line 99](../../../platformio.ini#L99)), but the current helper truncates oversized text and uses fallible allocation without the required nothrow wrapper ([QrUtils.cpp, lines 18–40](../../../src/util/QrUtils.cpp#L18)). Add a bounded golf helper for the short Wi-Fi and HTTP payloads. Preserve a four-module quiet zone and integer scaling. The pinned [QRCode v0.0.1 source](https://github.com/ricmoo/QRCode/blob/v0.0.1/src/qrcode.c#L628) uses stack scratch arrays, so audit the selected maximum version and move oversized workspace to checked activity-owned storage. A short QR reduces required symbol size but does not excuse unsafe library stack usage.

All storage uses HalStorage/HalFile. Preparation and SD reads occur outside RenderLock; publish display state under the lock. Large constant assets use constexpr flash storage. Device and served-page copy comes from translation sources; code examples here are conceptual labels.

## RAM, persistence, and cleanup

The C3 has a 380 KiB usable-RAM ceiling and one 48,000-byte framebuffer. This design does not reserve another framebuffer or generate PDF/image reports.

| Incremental storage | Implemented allocation |
| --- | --- |
| Immutable round | One 906-byte member of the checked heap-allocated activity |
| Output chunk | One reusable 2 KiB member; too large for the local-stack policy |
| QR output and workspace | Two 172-byte QR bitmaps plus 442-byte workspace, 786 bytes total |
| Session metadata | Fixed SSID/password/URL/filename fields and status; target ≤1 KiB |
| HTTP request/state | One 1 KiB request member plus fixed cursor/socket state; no DNS instance |
| Wi-Fi driver/runtime | Platform-managed allocation; measure on X4, do not guess from static sizes |
| Archive decode | Existing transient JSON/staging allocations, completed before radio startup where possible |

Target application-owned buffers below 8 KiB, excluding network internals, radio, parent activities, allocator overhead, UI host state, and fonts. This is a budget, not a measured peak heap total. The checked activity allocation owns the snapshot, summary fallback, server buffers, QR workspace and metadata. Fixed members keep these buffers off small task stacks and reuse them across requests. No full formatted document, vector of rounds, or JSON DOM is allocated during downloads. Existing archive decoding uses checked staging before radio startup.

Existing network startup releases rebuildable font caches under memory pressure ([CrossPointWebServerActivity.cpp, lines 64–76](../../../src/activities/network/CrossPointWebServerActivity.cpp#L64)). Apply the same principle if measured headroom requires it. Verify CJK UI rendering after cache release.

Teardown stops accepting requests, closes/drains active connections within a deadline, stops HTTP and DNS, closes member file handles, disconnects/deinitializes AP/Wi-Fi using verified APIs, and releases owned state before activity destruction. Avoid global server pointers/callbacks retaining a deleted activity. If the platform retains a radio allocation after first use, measure and document that steady-state baseline; repeated sessions must not accumulate it. Return without reboot is a release gate, given current upstream behavior.

Downloads do not create SD export files or mutate round state. Hash the archive and index before/after export to verify this. Do not auto-delete or mark a round as shared. Once the user sends an attachment to Discord/ChatGPT, it is a copy outside the X4; later device deletion does not remove it.

## Verification and delivery

1. Prove hotspot → Safari → TXT download → Files on the user's iPhone with both devices offline. Use actual X4-hosted assets. Test the captive portal and explicit Safari fallback.
2. Implement shared streaming exporters and fixtures, then integrate archived review, committed Finish, and active snapshots.
3. Validate one attachment in the user's Discord/OpenClaw setup and one optional ChatGPT upload. The user performs sends; implementation does not send messages or configure agents automatically.
4. Run the relevant host suites, formatting via `./bin/clang-format-fix -g`, and one final `pio run -e golf` after the last code change. No clean build is required by default.

Acceptance targets, not results already achieved:

- No SSID/password typing in the normal flow; at most two scans. After joining, one tap downloads the default report.
- With cellular disabled and no upstream Wi-Fi, all page assets load and every download completes. The TXT opens from Files after hotspot shutdown.
- The Files location and sharing instructions match the user's actual iOS/Safari settings. Test On My iPhone, iCloud download location, filename collisions, and interrupted downloads.
- Agent ingestion returns exact player/course, entered-hole count, gross and penalty totals. No local URL is presented as remotely accessible. If the installed agent cannot extract the attachment, document the failure and validate the plain-text fallback.
- TXT, CSV, JSON and HTML agree for 9/18 holes, every enabled slot, maximum counters/events, UTF-8 names, missing metadata, repaired/legacy files, unfinished rounds and summary-only history.
- Verify safe escaping and bounded responses for malformed requests; only selected snapshot routes are exposed. Browser refresh and retries do not mutate data.
- Test all four X4 orientations, remapped buttons, Back, timeout, sleep, disconnect/reconnect, low heap, absent SD, corrupt archive and archive cleanup-pending status.
- Record free heap before decode, before/after radio startup, during response/QR generation, and after exit. Require >50 KiB at peak and no accumulating loss over 100 sessions. Inspect task high-water marks and compiler stack-usage output against the <256-byte local-frame policy.
- Verify Back/timeout remains responsive with stalled clients and teardown does not reboot, leak handles, or leave callbacks into freed memory.
- Hash `/golf/state.json`, the selected archive, and `/golf/index.csv` before/after export of an already archived round. Test Finish writes separately.
- Once the download is complete, restoring phone internet allows the user to send it. Sending later works without the X4 present.

## Local delivery and device handoff

Implemented: active/archive/finished-summary entry points; exact committed archive reload; summary-only fallback; password-protected AP using RAM-only configuration; separate join/report QR codes; read-only TXT/CSV/JSON/HTML downloads; meaningful activity timeout; explicit network teardown without a reboot call. The optional captive portal is not implemented: use the second QR or the displayed address in Safari. New copy is in English translation YAML, with normal English fallback for other languages.

Host validation: all 423 tests passed, including 15 new exporter/HTTP/QR checks. These cover all player slots, maximum events/counters, legacy unknowns, summary fallback, escaping, immutable snapshots, bounded headers, stalled clients, exact Content-Length, and independent JSON/CSV parsing. QR module hashes match the pinned ricmoo v0.0.1 encoder for both join and URL payloads. RISC-V `-fstack-usage` checks report maximum individual frames of 128 bytes for the formatter, 160 for the encoder, 128 for the server and 176 for the export activity. These are individual frames, not complete call-chain high-water marks. QR drawing has a separate non-inlined function to keep its temporaries out of the text-layout frame.

The stock-reader `default` build also passed (one existing WebSockets deprecation warning). Static analysis passed for `golf` and `default` with no high/medium findings; low-severity style findings remain, including in the adapted QR code. Formatting was applied through the repository wrapper, including new files, without staging changes.

The final `golf` firmware build passed: static RAM 57,140 bytes; flash 5,574,869 / 6,553,600 bytes. Static linker RAM is not runtime free heap and does not establish the >50 KiB peak-headroom gate.

First device test:

1. Flash the locally built `golf` firmware using the normal device workflow. Open an archived round and choose **Send to phone**.
2. Disable iPhone cellular data. Scan the join QR, accept staying on the network without internet, then scan the report QR and open Safari.
3. Download TXT; open it in Files and compare player, gross score and penalties with the X4. Download each optional format too.
4. Press **Done**, confirm the X4 returns without reboot and the downloaded TXT still opens. Restore phone internet and attach the file in the existing Discord/OpenClaw conversation.
5. Repeat from an active round and immediately after Finish. Test a summary-only history entry, Back during an incomplete request, five-minute idle expiry, every orientation and repeated sessions.
6. Monitor `GOLFEXP` debug logs for heap before export, with AP/server, and after network teardown. Verify heap recovery after the activity is deleted; also inspect loop/render task high-water marks. Hash the source archive/index around read-only exports.

Not verified here: physical QR scanning, iOS saving/share extensions, OpenClaw/ChatGPT ingestion, low-heap behavior, peak runtime memory, power consumption, and repeated teardown on an X4. These remain release gates, not assumed successes.
