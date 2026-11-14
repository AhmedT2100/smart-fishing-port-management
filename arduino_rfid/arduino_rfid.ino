// =============================================
// Projet Arduino - Lecteur RFID Employé
// =============================================

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  10
#define RST_PIN  9

MFRC522 rfid(SS_PIN, RST_PIN);


String dernierUID = "";

void setup() {
    Serial.begin(9600);
    SPI.begin();
    rfid.PCD_Init();
    Serial.println("READY");
}

void loop() {
    // Si Qt envoie "GET" → renvoyer le dernier UID lu
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "GET" && dernierUID != "") {
            Serial.println("CODE:" + dernierUID);
        }
    }

    // Lire un nouveau badge
    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial())   return;

    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) uid += "0";
        uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    dernierUID = uid;
    Serial.println("CODE:" + uid);

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    delay(1500);
}
