#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <math.h>

#define PH_SENSOR_CHANNEL 0
#define BUTTON_PIN PD7
#define LED_PIN PD3
#define SERVO_PIN PB1
#define BUZZER_PIN PB0
#define TRIG_PIN PB2
#define ECHO_PIN PB3

#define LCD_ADDR 0x27

typedef enum { IDLE, OPENING, FEEDING, CLOSING } FeedState;
volatile FeedState feedState = IDLE;

const unsigned long feedInterval = 10000;
const unsigned long servoOpenTime = 500;
const unsigned long feedDuration = 1000;
const unsigned long servoCloseTime = 500;

volatile unsigned long lastAutoFeedTime = 0;
volatile unsigned long feedStateStart = 0;
volatile uint8_t autoFeedingNow = 0;
volatile unsigned long millis_counter = 0;

// Tambahan variabel untuk perubahan serial monitor
float lastPhValue = -1.0f;
uint8_t lastFeedState = IDLE;
uint8_t lastFeedPercent = 255;

// ---------- UART ----------
void uart_init(unsigned int baud) {
    unsigned int ubrr = F_CPU/16/baud - 1;
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_transmit(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

void uart_print(const char* str) {
    while (*str) {
        uart_transmit(*str++);
    }
}

void uart_println(const char* str) {
    uart_print(str);
    uart_print("\r\n");
}

void uart_print_float(float value, uint8_t decimal) {
    char buffer[10];
    dtostrf(value, 5, decimal, buffer);
    uart_print(buffer);
}

// ---------- Timer, millis ----------
ISR(TIMER0_OVF_vect) {
    millis_counter++;
}

unsigned long millis() {
    unsigned long ms;
    cli();
    ms = millis_counter;
    sei();
    return ms;
}

void timer0_init() {
    TCCR0A = 0;
    TCCR0B = (1 << CS01) | (1 << CS00);
    TIMSK0 = (1 << TOIE0);
    sei();
}

// ---------- ADC ----------
void adc_init() {
    ADMUX = (1 << REFS0) | (PH_SENSOR_CHANNEL & 0x0F);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_read(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

// ---------- I2C + LCD ----------
void i2c_init() {
    TWSR = 0x00;
    TWBR = 72;
    TWCR = (1 << TWEN);
}

void i2c_start() {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void i2c_stop() {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    _delay_us(10);
}

void i2c_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void lcd_write_nibble(uint8_t nibble, uint8_t rs) {
    uint8_t data = (nibble << 4) | 0x08;
    if (rs) data |= 0x01;

    i2c_start();
    i2c_write(LCD_ADDR << 1);
    i2c_write(data | 0x04);
    _delay_us(1);
    i2c_write(data & ~0x04);
    i2c_stop();
    _delay_us(50);
}

void lcd_command(uint8_t cmd) {
    lcd_write_nibble(cmd >> 4, 0);
    lcd_write_nibble(cmd & 0x0F, 0);
    _delay_ms(2);
}

void lcd_data(uint8_t data) {
    lcd_write_nibble(data >> 4, 1);
    lcd_write_nibble(data & 0x0F, 1);
    _delay_us(50);
}

void lcd_init() {
    _delay_ms(50);
    lcd_write_nibble(0x03, 0);
    _delay_ms(5);
    lcd_write_nibble(0x03, 0);
    _delay_us(150);
    lcd_write_nibble(0x03, 0);
    lcd_write_nibble(0x02, 0);

    lcd_command(0x28);
    lcd_command(0x0C);
    lcd_command(0x06);
    lcd_command(0x01);
    _delay_ms(2);
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_command(0x80 | (col + row_offsets[row]));
}

void lcd_print(const char *str) {
    while (*str) {
        lcd_data(*str++);
    }
}

// ---------- Servo ----------
void servo_init() {
    DDRB |= (1 << SERVO_PIN);
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    ICR1 = 39999;
    OCR1A = 1000;
}

void servo_write(uint8_t angle) {
    if (angle > 180) angle = 180;
    uint16_t pulse_us = 500 + ((uint32_t)angle * 2000) / 180;
    OCR1A = pulse_us * 2;
}

// ---------- Buzzer ----------
void buzzer_init() {
    DDRB |= (1 << BUZZER_PIN);
}

void buzzer_on(uint16_t frequency) {
    if (frequency == 0) return;
    uint16_t delay = (1000000UL / (2 * frequency));
    for (uint16_t i = 0; i < 100; i++) {
        PORTB |= (1 << BUZZER_PIN);
        _delay_us(delay);
        PORTB &= ~(1 << BUZZER_PIN);
        _delay_us(delay);
    }
}

void buzzer_off() {
    PORTB &= ~(1 << BUZZER_PIN);
}

// ---------- Utility ----------
uint8_t button_pressed() {
    return !(PIND & (1 << BUTTON_PIN));
}

void led_on() {
    PORTD |= (1 << LED_PIN);
}

void led_off() {
    PORTD &= ~(1 << LED_PIN);
}

void delay_ms(uint16_t ms) {
    while (ms--) _delay_ms(1);
}

void startFeeding(uint8_t isAuto) {
    if (feedState == IDLE) {
        feedState = OPENING;
        feedStateStart = millis();
        autoFeedingNow = isAuto;
        servo_write(90);
        led_on();
        if (!isAuto)
            uart_println("Manual feeding ON");
        else
            uart_println("Auto feeding ON");
    }
}

void lcd_print_int(int val) {
    char buf[6];
    itoa(val, buf, 10);
    lcd_print(buf);
}

//-----------ultrasonik---------------
void ultrasonic_init() {
    DDRB |= (1 << TRIG_PIN);    // TRIG sebagai output
    DDRB &= ~(1 << ECHO_PIN);   // ECHO sebagai input
}

void trigger_ultrasonic() {
    PORTB &= ~(1 << TRIG_PIN);   // LOW
    _delay_us(2);
    PORTB |= (1 << TRIG_PIN);    // HIGH
    _delay_us(10);
    PORTB &= ~(1 << TRIG_PIN);   // LOW
}


uint16_t read_echo_us() {
    while (!(PINB & (1 << ECHO_PIN))) {
        if (TCNT1 > 30000) return 0;
    }

    TCNT1 = 0;

    while (PINB & (1 << ECHO_PIN)) {
        if (TCNT1 > 30000) break;
    }

    return TCNT1 / 2; // konversi tick ke mikrodetik
}



uint8_t get_feed_percentage() {
    trigger_ultrasonic();
    uint16_t duration = read_echo_us();       // mikrodetik
    float distance_cm = duration / 58.0f;

    if (distance_cm >= 10.0f) return 0;
    return (uint8_t)(((10.0f - distance_cm) / 10.0f) * 100.0f);
}






// ---------- Main ----------
int main(void) {
    DDRD &= ~(1 << BUTTON_PIN);
    PORTD |= (1 << BUTTON_PIN);
    DDRD |= (1 << LED_PIN);
    PORTD &= ~(1 << LED_PIN);

    buzzer_init();
    adc_init();
    i2c_init();
    lcd_init();
    servo_init();
    ultrasonic_init();
    timer0_init();
    uart_init(9600);

    lcd_set_cursor(0, 0);
    lcd_print("AQUA SMART READY");
    lcd_set_cursor(0, 1);
    lcd_print("     BOZZ");
    delay_ms(2000);
    lcd_command(0x01);

    lastAutoFeedTime = millis();

    while (1) {
        unsigned long currentMillis = millis();
        uint16_t phADC = adc_read(PH_SENSOR_CHANNEL);
        float voltage = phADC * (5.0f / 1023.0f);
        float phValue = 7.0f + ((2.5f - voltage) * 3.0f);

        // Serial jika pH berubah
        if (fabs(phValue - lastPhValue) >= 0.1f) {
            uart_print("PH changed: ");
            uart_print_float(phValue, 2);
            uart_println("");
            lastPhValue = phValue;
        }

        // Serial jika state feeding berubah
        if (feedState != lastFeedState) {
            uart_print("Feed State: ");
            switch(feedState) {
                case IDLE: uart_println("IDLE"); break;
                case OPENING: uart_println("OPENING"); break;
                case FEEDING: uart_println("FEEDING"); break;
                case CLOSING: uart_println("CLOSING"); break;
            }
            lastFeedState = feedState;
        }

        // Serial jika sisa pakan berubah
        uint8_t pakan_percent = get_feed_percentage();
        if (pakan_percent != lastFeedPercent) {
            uart_print("Sisa Pakan: ");
            char buf[5];
            itoa(pakan_percent, buf, 10);
            uart_print(buf);
            uart_println("%");
            lastFeedPercent = pakan_percent;
        }

        // Warning pH
        if (phValue > 7.0f) {
            buzzer_on(1000);
            uart_println("WARNING: pH HIGH!");
        } else {
            buzzer_off();
        }

        // Tombol manual feeding
        if (button_pressed()) {
            startFeeding(0);
            delay_ms(300);
        }

        // Feeding otomatis
        if (feedState == IDLE && (currentMillis - lastAutoFeedTime >= feedInterval)) {
            startFeeding(1);
        }

        // FSM feeding
        if (feedState != IDLE) {
            unsigned long elapsed = currentMillis - feedStateStart;

            switch (feedState) {
                case OPENING:
                    if (elapsed >= servoOpenTime) {
                        feedState = FEEDING;
                        feedStateStart = millis();
                    }
                    break;

                case FEEDING:
                    if (elapsed >= feedDuration) {
                        feedState = CLOSING;
                        servo_write(0);
                        feedStateStart = millis();
                    }
                    break;

                case CLOSING:
                    if (elapsed >= servoCloseTime) {
                        feedState = IDLE;
                        led_off();
                        if (autoFeedingNow) {
                            lastAutoFeedTime = millis();
                            uart_println("Auto feeding DONE");
                        }
                    }
                    break;

                default:
                    break;
            }
        }

        // LCD update
        lcd_set_cursor(0, 0);
        lcd_print("PH Air:");
        char phStr[6];
        dtostrf(phValue, 4, 1, phStr);
        lcd_print(phStr);
        lcd_print("   ");
        if (phValue > 7.0f) {
            lcd_set_cursor(12, 0);
            lcd_print("HIGH!");
        } else {
            lcd_set_cursor(12, 0);
            lcd_print("      ");
        }

        lcd_set_cursor(0, 1);
        if (feedState == IDLE) {
            unsigned long timeToNextFeed = (feedInterval > (currentMillis - lastAutoFeedTime)) ?
                                           (feedInterval - (currentMillis - lastAutoFeedTime)) : 0;

            unsigned long seconds = timeToNextFeed / 1000;
            unsigned int hours = seconds / 3600;
            seconds %= 3600;
            unsigned int minutes = seconds / 60;
            seconds %= 60;

            lcd_print("Feed in: ");
            if (hours > 0) {
                lcd_print_int(hours);
                lcd_print("h ");
            }
            if (minutes > 0 || hours > 0) {
                lcd_print_int(minutes);
                lcd_print("m ");
            }
            lcd_print_int(seconds);
            lcd_print("s   ");
        } else {
            lcd_print("Feeding...       ");
        }


        delay_ms(50);
    }

    return 0;
}
