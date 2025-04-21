#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

#define PH_SENSOR_PIN A0
#define BUTTON_PIN 7
#define LED_PIN 3
#define SERVO_PIN 9
#define BUZZER_PIN 8

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo feederServo;

enum FeedState { IDLE, OPENING, FEEDING, CLOSING };
FeedState feedState = IDLE;

const unsigned long feedInterval = 10000;
const unsigned long servoOpenTime = 500;
const unsigned long feedDuration = 1000;
const unsigned long servoCloseTime = 500;

unsigned long lastAutoFeedTime = 0;     // Waktu terakhir FEEDING OTOMATIS
unsigned long feedStartTime = 0;        // Waktu mulai feeding (untuk state machine)
bool autoFeedingNow = false;            // Penanda apakah feeding yang aktif adalah otomatis

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  feederServo.attach(SERVO_PIN);
  feederServo.write(0);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AQUA SMART READY");
  lcd.setCursor(0, 1);
  lcd.print("     BOZZ");
  delay(2000);
  lcd.clear();

  lastAutoFeedTime = millis();
}

void startFeeding(bool isAuto) {
  if (feedState == IDLE) {
    feedState = OPENING;
    feedStartTime = millis();
    autoFeedingNow = isAuto;
    feederServo.write(90);
    digitalWrite(LED_PIN, HIGH);
    Serial.println(isAuto ? "AUTO FEEDING START" : "MANUAL FEEDING START");
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // === Sensor pH dan Buzzer ===
  int phADC = analogRead(PH_SENSOR_PIN);
  float voltage = phADC * (5.0 / 1023.0);
  float phValue = 7 + ((2.5 - voltage) * 3.0);

  if (phValue > 7.0) {
    tone(BUZZER_PIN, 1000);
  } else if (phValue > 6.0) {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // === Tombol manual feeding ===
  if (digitalRead(BUTTON_PIN) == LOW) {
    startFeeding(false);  // false = feeding manual
    delay(300);
  }

  // === Feeding otomatis ===
  if (feedState == IDLE && (currentMillis - lastAutoFeedTime >= feedInterval)) {
    startFeeding(true);   // true = feeding otomatis
  }

  // === Proses feeding (state machine) ===
  if (feedState != IDLE) {
    unsigned long elapsed = currentMillis - feedStartTime;

    switch (feedState) {
      case OPENING:
        if (elapsed >= servoOpenTime) {
          feedState = FEEDING;
          feedStartTime = millis();
          Serial.println("FEEDING...");
        }
        break;

      case FEEDING:
        if (elapsed >= feedDuration) {
          feedState = CLOSING;
          feederServo.write(0);
          feedStartTime = millis();
          Serial.println("FEEDING END");
        }
        break;

      case CLOSING:
        if (elapsed >= servoCloseTime) {
          feedState = IDLE;
          digitalWrite(LED_PIN, LOW);
          if (autoFeedingNow) {
            lastAutoFeedTime = millis();  // Reset timer hanya jika feeding otomatis
          }
          Serial.println("FEEDING COMPLETE");
        }
        break;
    }
  }

  // === LCD DISPLAY ===
  lcd.setCursor(0, 0);
  lcd.print("PH Air:");
  lcd.print(phValue, 1);
  lcd.print("   ");

  if (phValue > 7.0) {
    lcd.setCursor(12, 0);
    lcd.print("HIGH!");
  } else {
    lcd.setCursor(12, 0);
    lcd.print("      ");
  }

  lcd.setCursor(0, 1);
  if (feedState == IDLE) {
    unsigned long timeToNextFeed = (feedInterval > (currentMillis - lastAutoFeedTime)) ?
                                   (feedInterval - (currentMillis - lastAutoFeedTime)) : 0;

    unsigned long seconds = timeToNextFeed / 1000;
    unsigned int hours = seconds / 3600;
    seconds %= 3600;
    unsigned int minutes = seconds / 60;
    seconds %= 60;

    lcd.print("Feed in: ");
    if (hours > 0) {
      lcd.print(hours);
      lcd.print("h ");
    }
    if (minutes > 0 || hours > 0) {
      lcd.print(minutes);
      lcd.print("m ");
    }
    lcd.print(seconds);
    lcd.print("s   ");
  } else {
    lcd.print("Feeding...       ");
  }

  delay(50);
}
