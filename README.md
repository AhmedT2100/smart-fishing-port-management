# Smart Fishing Port Management

A C++17 / Qt Widgets desktop prototype for managing fishing-port operations, with Oracle database access and Arduino hardware experiments. The interface and much of the source commentary are in French.

## Features represented in the source

- Boat records: create, edit, delete, search, sort, and status statistics.
- Berth/location and dock management, availability views, charts, and document exports.
- Departure records and employee management, including leave and attendance workflows.
- Password and RFID login flows, password-reset SMS through Twilio, and email verification through EmailJS.
- Serial hardware panels for RFID badges, PIR motion detection, and dock barrier control.
- Optional boat-data assistant requests to a local Ollama `mistral` model.

These are existing code paths, not a guarantee that every integration is operational. See limitations and the [validation report](docs/VALIDATION.md).

## Technologies and architecture

| Layer | Files / technology |
| --- | --- |
| Entry point and connection | `main.cpp`, `connexion.cpp/.h`; startup requires a successful QODBC connection |
| Desktop presentation | `fishingport.cpp/.h/.ui`; Qt Widgets, Designer, Charts, PrintSupport |
| Business and persistence | `bateau`, `emplacement`, `quai`, `sortie`, `employe` C++ modules; SQL queries against Oracle |
| Hardware bridge | `arduino.cpp/.h`; Qt SerialPort at 9600 baud |
| Remote services | Qt Network; optional Twilio, EmailJS, and local Ollama |
| Assets | `resources.qrc`, `icons/`, `styles/theme.qss` |
| Firmware | `arduino_pir/`, `arduino_rfid/`, `inchirah+yasmine.ino/sketch_may4a/` |
| Build | qmake project `gestion_bateaux.pro`; C++17 and Qt 6 |

The main window coordinates the domain modules, SQL models, network requests, and serial communication. Arduino sketches run separately on their boards; they are not compiled into the desktop application.

## Prerequisites

- Qt **6** SDK with Core, Gui, Widgets, SQL, Network, PrintSupport, Charts, and SerialPort; install SVG runtime/image plugins for the supplied icons.
- A compatible C++17 compiler and build tool (for example GCC + Make or MSVC + nmake), plus qmake from the same Qt kit. Qt Creator is optional.
- Qt's `QODBC` SQL plugin, an Oracle ODBC driver matching the application architecture, and an accessible Oracle database.
- An existing schema matching the queries. Tables referenced include `BATEAU`, `EMPLOYE`, `QUAIS`, `AVOIR`, and additional berth, departure, leave, and attendance tables. **No schema creation or seed scripts are supplied.** Obtain the project's schema before running; this list is not a database specification.
- For hardware: Arduino IDE or CLI, a compatible board, serial access, and the libraries described below.

## Configuration

Use [.env.example](.env.example) as a reference. **The application does not load `.env` files.** Set variables in your shell or Qt Creator's Run Environment. Keep real values local; `.env` files are ignored.

| Variable | Purpose |
| --- | --- |
| `FISHING_PORT_ODBC_CONNECTION` | Required full ODBC connection string; driver, database, username, and password must match your installation |
| `TWILIO_ACCOUNT_SID`, `TWILIO_AUTH_TOKEN`, `TWILIO_FROM_NUMBER` | Optional password-reset SMS configuration; requires an authorized sender and recipient |
| `EMAILJS_SERVICE_ID`, `EMAILJS_TEMPLATE_ID`, `EMAILJS_PUBLIC_KEY` | Optional email verification service; template parameters are `email`, `passcode`, and `time` |
| `FISHING_PORT_ALERT_EMAIL` | Recipient for the existing hardware email-verification flow; use an address you control |
| `FISHING_PORT_AUTHORIZED_RFID` | UID of the demo authorized badge, hex without separators; an empty value disables this badge shortcut |

For example, in a POSIX shell (replace the placeholders locally):

```sh
export FISHING_PORT_ODBC_CONNECTION='Driver={YOUR_ORACLE_DRIVER};Dbq=YOUR_DATABASE;Uid=YOUR_USER;Pwd=YOUR_PASSWORD;'
```

Missing optional service variables prevent the corresponding request. Configure the RFID UID in the desktop process; the RFID sketch only transmits badge UIDs and needs no embedded credential. RFID UIDs are cloneable and must not be treated as strong authentication.

## Build and run

Clone this private repository with an account that has access, then use a Qt 6 kit:

```sh
git clone https://github.com/AhmedT2100/smart-fishing-port-management.git
cd smart-fishing-port-management
mkdir -p build
cd build
/path/to/Qt/6.x/gcc_64/bin/qmake ../gestion_bateaux.pro
make -j2
./gestion_bateaux
```

Adjust the qmake path for your installation. On Windows, open `gestion_bateaux.pro` in Qt Creator, select a Qt 6 kit, configure its Run Environment, and build/run; the command-line build tool depends on the kit. If the database cannot connect, the application exits before opening the main window. Check the driver, process environment, database reachability, and schema locally.

The optional assistant uses `http://localhost:11434/api/generate` with model `mistral`. Install and run Ollama with that model if you want to exercise it. Boat data is included in requests to that endpoint.

## Arduino setup

Flash each sketch separately to its intended board; review its pin definitions and wiring first. All supplied sketches use **9600 baud**. Select or enter the connected port in the desktop UI; some defaults are Windows `COM3`, while Linux ports usually resemble `/dev/ttyACM0` or `/dev/ttyUSB0`. The current user needs serial-device access.

| Sketch | Hardware / libraries | Protocol |
| --- | --- | --- |
| `arduino_rfid/arduino_rfid.ino` | MFRC522 reader; SPI and MFRC522 library; SS 10, RST 9 | Emits `CODE:<UID>`; `GET` requests the last UID |
| `arduino_pir/arduino_pir.ino` | PIR on 8, LED on 2, buzzer on 7 | Emits `MOTION` / `TIMEOUT`; accepts `OK`, `ALERT`, `RESET`, `ERR` |
| `inchirah+yasmine.ino/sketch_may4a/sketch_may4a.ino` | Wire, LiquidCrystal_I2C, Servo; LCD 0x27, LEDs 2/3, servo 9 | `1` opens the barrier; `0` closes it |

Do not open the same serial port in Arduino Serial Monitor and the application simultaneously. Firmware compilation and physical wiring must be verified on the target boards.

## Screenshots

No screenshots were supplied in the archive. The original Designer form, stylesheet, and SVG assets are included; screenshots should be added after a verified run with non-sensitive demonstration data.

## Validation and known limitations

See [docs/VALIDATION.md](docs/VALIDATION.md) for checks performed on this import. A successful application build is **not claimed**.

- The available environment lacks a working Qt SDK/qmake and Arduino CLI. Compilation, GUI behavior, database operations, live messaging, Ollama, and physical hardware have not been tested.
- No database schema, migrations, seed data, or automated functional tests were supplied.
- Existing authentication compares/stores passwords without a modern password-hashing design. Demo badge shortcuts, reset-code handling, and authorization need review before real deployment.
- Some network operations block the UI or lack explicit timeouts; provider failures need further testing.
- Existing text contains encoding artifacts in some French labels/comments; these are preserved in this import to avoid broad unrelated changes.
- Serial defaults and device discovery depend on the operating system and board. A single configuration is not a guarantee of compatibility with all sketches.

## Team credits

This is a team project. The supplied README credits **Ahmed Saadaoui** with the boat-management module. Other contributor identities and module ownership have not been verified; the team can add agreed credits. Names are not inferred from directory names, test addresses, or GitHub usernames.

## Security and licensing

Database, SMS, email-service, and authorized-badge configuration belong in the local process environment. Message payloads and reset codes are not deliberately logged in the cleaned paths. This source review is not a security audit; review remaining diagnostics and access controls before using real data. The supplied README reports earlier credential exposure; any previously exposed credentials should be revoked or rotated by their owners.

No license file was supplied. Copyright remains with the respective owners; no open-source license is implied. This repository remains private, and this import does not publish a release.
