# Smart Fishing Port Management

A desktop application prototype for managing operations at a fishing port, developed with **C++**, **Qt Widgets**, **Qt SQL**, and **Arduino** integrations.

## Overview

The project combines administrative workflows for fishing-port operations with serial-connected hardware experiments. The codebase includes modules for **boats**, **berths/locations (emplacements)**, **docks (quais)**, **departures (sorties)**, and **employees**, along with a desktop dashboard and Arduino sketches.

## Tech stack

- C++17 and Qt (Widgets, SQL, Network, PrintSupport, Charts, SerialPort)
- qmake (`gestion_bateaux.pro`) and Qt Designer (`fishingport.ui`)
- Oracle database through Qt's ODBC SQL driver (`QODBC`)
- Arduino sketches for PIR and RFID experiments
- Optional local LLM endpoints referenced by the application (`http://localhost:11434`)
- Optional Twilio SMS integration

## Repository structure

```text
.
├── gestion_bateaux.pro       # qmake project definition
├── main.cpp                  # application entry point
├── fishingport.cpp/.h/.ui    # desktop UI and orchestration
├── connexion.cpp/.h          # database connection
├── bateau.cpp/.h             # boats
├── emplacement.cpp/.h        # berth / location management
├── quai.cpp/.h               # docks
├── sortie.cpp/.h             # departures
├── employe.cpp/.h            # employee management
├── arduino.cpp/.h            # serial communication
├── arduino_pir/              # PIR sketch
├── arduino_rfid/             # RFID sketch
├── inchirah+yasmine.ino/     # additional Arduino sketch
├── icons/                    # SVG icons
└── styles/                   # QSS styles
```

## Getting started

1. Install **Qt 6** with the components referenced in `gestion_bateaux.pro`, a compatible C++17 compiler, and Qt Creator (or qmake). On Windows, use a compatible MinGW toolchain if desired.
2. Install/configure the **Oracle ODBC driver** and ensure your schema matches the SQL queries in the source code. **A database schema or seed data is not included in this repository.**
3. Configure `FISHING_PORT_ODBC_CONNECTION` in your operating system environment with the ODBC connection string for your own database. Do not commit credentials.
4. Open `gestion_bateaux.pro` in Qt Creator, configure your build kit, build, and run.
5. For Arduino features, flash the relevant `.ino` sketch to compatible hardware, select the appropriate serial port in the application (some defaults assume `COM3`), and use a matching serial configuration.
6. For SMS functionality, set `TWILIO_ACCOUNT_SID`, `TWILIO_AUTH_TOKEN`, and `TWILIO_FROM_NUMBER`. The app will report unavailable SMS functionality if these variables are not configured.

> The `.env.example` file is an **illustrative template only**. Qt does not automatically load `.env` files. Set the variables in the process environment through your OS or IDE configuration.

## Project status and limitations

**Academic/team prototype — not production-ready.** The uploaded project includes source code and UI assets but no database schema, deployment instructions, or test suite. The build and hardware integrations have not been validated in this environment. Authentication logic needs further security hardening, including strong password hashing and authorization checks before production use.

## Contributors

This is a **team project**. The supplied source archive and original README identify Ahmed Saadaoui as responsible for the boat management module. Other modules and integrations may have been developed collaboratively. Contributor credits should be completed with the team's agreement.

## Security

The original archive contained embedded database credentials and a third-party SMS API token. These have been **removed from this publishable copy** and replaced with environment variables. **Rotate/revoke the original credentials and token before making this project public**; removing them from this copy does not invalidate exposed credentials elsewhere.

## License

No license has been selected yet. All rights reserved by the respective copyright holders until the team chooses and adds an appropriate license.
