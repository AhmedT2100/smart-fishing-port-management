/*
 * ============================================================
 *  Port de Pêche — Atelier Arduino
 *  Capteur PIR + LED + Buzzer
 * ============================================================
 */

// ── Broches ─────────────────────────────────────────────────
const int LED_PIN    = 2;
const int PIR_PIN    = 8;
const int BUZZER_PIN = 7;

// ── Temps ───────────────────────────────────────────────────
const unsigned long WAIT_TIMEOUT_MS = 8000;
const unsigned long PIR_COOLDOWN_MS = 4000;

// ── Variables ───────────────────────────────────────────────
String mode = "IDLE";

unsigned long stateStartTime = 0;
unsigned long waitStartTime  = 0;
unsigned long lastMotionTime = 0;

String incoming = "";

// Sirène
unsigned long lastSirenChange = 0;
bool sirenHigh = false;

// ── Setup ───────────────────────────────────────────────────
void setup() {

    Serial.begin(9600);

    pinMode(PIR_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    delay(3000); // Stabilisation PIR

    Serial.println("[Arduino] Pret.");
}

// ── Loop ────────────────────────────────────────────────────
void loop() {

    unsigned long now = millis();

    int pirValue = digitalRead(PIR_PIN);

    // ========================================================
    // 1. Détection PIR
    // ========================================================
    if (mode == "IDLE") {

        // Détection mouvement avec cooldown
        if (pirValue == HIGH &&
            (now - lastMotionTime > PIR_COOLDOWN_MS)) {

            Serial.println("MOTION");

            lastMotionTime = now;

            mode = "WAITING";

            waitStartTime = now;
        }
    }

    // ========================================================
    // 2. Lecture série Qt
    // ========================================================
    while (Serial.available() > 0) {

        char c = (char)Serial.read();

        if (c == '\r')
            continue;

        if (c == '\n') {

            incoming.trim();

            String cmd = incoming;

            cmd.toUpperCase();

            // ------------------------------------------------
            // OK
            // ------------------------------------------------
            if (cmd == "OK") {

                mode = "OK";

                stateStartTime = now;

                digitalWrite(LED_PIN, HIGH);

                noTone(BUZZER_PIN);
            }

            // ------------------------------------------------
            // ALERT
            // ------------------------------------------------
            else if (cmd == "ALERT") {

                mode = "ALERT";

                stateStartTime = now;

                lastSirenChange = now;

                sirenHigh = true;

                digitalWrite(LED_PIN, HIGH);

                tone(BUZZER_PIN, 2000);
            }

            // ------------------------------------------------
            // RESET / ERR
            // ------------------------------------------------
            else if (cmd == "RESET" || cmd == "ERR") {

                mode = "IDLE";

                digitalWrite(LED_PIN, LOW);

                noTone(BUZZER_PIN);

                digitalWrite(BUZZER_PIN, LOW);
            }

            incoming = "";
        }
        else {

            incoming += c;
        }
    }

    // ========================================================
    // 3. Timeout attente Qt
    // ========================================================
    if (mode == "WAITING") {

        if (now - waitStartTime >= WAIT_TIMEOUT_MS) {

            mode = "IDLE";

            digitalWrite(LED_PIN, LOW);

            noTone(BUZZER_PIN);

            digitalWrite(BUZZER_PIN, LOW);

            Serial.println("TIMEOUT");
        }
    }

    // ========================================================
    // 4. Mode ALERT
    // ========================================================
    if (mode == "ALERT") {

        // Fin après 5 sec
        if (now - stateStartTime >= 5000) {

            mode = "IDLE";

            noTone(BUZZER_PIN);

            digitalWrite(BUZZER_PIN, LOW);

            digitalWrite(LED_PIN, LOW);
        }
        else {

            // Sirène alternée
            if (now - lastSirenChange >= 150) {

                lastSirenChange = now;

                sirenHigh = !sirenHigh;

                if (sirenHigh) {

                    tone(BUZZER_PIN, 2500);
                }
                else {

                    tone(BUZZER_PIN, 1200);
                }
            }
        }
    }

    // ========================================================
    // 5. Mode OK
    // ========================================================
    else if (mode == "OK") {

        if (now - stateStartTime >= 3000) {

            mode = "IDLE";

            digitalWrite(LED_PIN, LOW);
        }
    }
}
