#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servoQuai;

const int PIN_LED_ROUGE = 2;
const int PIN_LED_VERTE = 3;
const int PIN_SERVO     = 9;

void setup() {
  Serial.begin(9600);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("En attente...");

  // LEDs + Servo
  pinMode(PIN_LED_ROUGE, OUTPUT);
  pinMode(PIN_LED_VERTE, OUTPUT);
  servoQuai.attach(PIN_SERVO);

  // État initial
  digitalWrite(PIN_LED_ROUGE, HIGH);
  digitalWrite(PIN_LED_VERTE, LOW);
  servoQuai.write(0);
}

void loop() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    lcd.clear();

    if (data == "1") {
      // Quai libre
      lcd.setCursor(0, 0);
      lcd.print("Quai libre");
      lcd.setCursor(0, 1);
      lcd.print("Barriere ouverte");

      digitalWrite(PIN_LED_VERTE, HIGH);
      digitalWrite(PIN_LED_ROUGE, LOW);
      servoQuai.write(90);
    }
    else if (data == "0") {
      // Quai occupé
      lcd.setCursor(0, 0);
      lcd.print("Quai occupe");
      lcd.setCursor(0, 1);
      lcd.print("Barriere fermee");

      digitalWrite(PIN_LED_VERTE, LOW);
      digitalWrite(PIN_LED_ROUGE, HIGH);
      servoQuai.write(0);
    }
  }
}
