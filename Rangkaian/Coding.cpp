#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SERVO_PIN 9
#define LED_PIN 3
#define PH_SENSOR_PIN A0
#define BUTTON_PIN 7
#define BUZZER_PIN 8

enum FeedState { WAITING, FEEDING, POST_FEED };
FeedState currentState = WAITING;

Servo feederServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

const unsigned long feedInterval = 10000;
const unsigned long servoOnDuration = 1000;
const unsigned long postFeedDelay = 500;

unsigned long lastAutoFeedMillis = 0;
unsigned long stateStartMillis = 0;

bool manualFeedTriggered = false;

void setup() {
  Serial.begin(9600);
  feederServo.attach(SERVO_PIN);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  feederServo.write(0);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Aquasmart ready");
}

void loop() {
  unsigned long currentMillis = millis();

  // === BACA PH ===
  int phRaw = analogRead(PH_SENSOR_PIN);
  float voltage = phRaw * (5.0 / 1023.0);
  float phValue = 7 + ((2.5 - voltage) * 3.0);

  // === LCD TAMPILKAN PH ===
  lcd.setCursor(0, 1);
  lcd.print("pH: ");
  lcd.print(phValue, 2);
  lcd.print("       ");

  // === TAMPILKAN WARNING PH > 7 ===
  if (phValue > 7.0) {
    lcd.setCursor(10, 1);
    lcd.print("!HIGH");
  }

  // === BUZZER JIKA PH > 6 ===
  if (phValue > 6.0) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

if (phValue > 7.0) {
    tone(BUZZER_PIN, 1000);  // Buzzer berbunyi melengking pada frekuensi 1000Hz
  } else {
    noTone(BUZZER_PIN);   // Mematikan buzzer jika pH <= 7
  }

  // === MANUAL BUTTON ===
  if (digitalRead(BUTTON_PIN) == LOW && currentState == WAITING) {
    manualFeedTriggered = true;
    currentState = FEEDING;
    stateStartMillis = currentMillis;

    Serial.println("MANUAL FEED START");
    feederServo.write(90);
    digitalWrite(LED_PIN, HIGH);
  }

  // === AUTO FEED ===
  if ((currentMillis - lastAutoFeedMillis >= feedInterval) && currentState == WAITING) {
    manualFeedTriggered = false;
    currentState = FEEDING;
    stateStartMillis = currentMillis;
    lastAutoFeedMillis = currentMillis;

    Serial.println("AUTO FEED START");
    feederServo.write(90);
    digitalWrite(LED_PIN, HIGH);
  }

  // === STATE MACHINE ===
  switch (currentState) {
    case FEEDING:
      if (currentMillis - stateStartMillis >= servoOnDuration) {
        Serial.println("FEED END");
        feederServo.write(0);
        digitalWrite(LED_PIN, LOW);
        stateStartMillis = currentMillis;
        currentState = POST_FEED;
      }
      break;

    case POST_FEED:
      if (currentMillis - stateStartMillis >= postFeedDelay) {
        currentState = WAITING;
      }
      break;

    case WAITING:
      break;
  }

  delay(100);
}
