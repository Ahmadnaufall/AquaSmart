#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

// Konstanta
#define JUMLAH_SAMPEL 500
#define VREF 5.0
#define BAUD 9600
#define UBRR_VALUE ((F_CPU / (16UL * BAUD)) - 1)

// Fungsi inisialisasi ADC
void adc_init() {
    ADMUX = (1 << REFS0);                // AVcc sebagai referensi, input ADC0 (A0)
    ADCSRA = (1 << ADEN) |               // Enable ADC
             (1 << ADPS2) | (1 << ADPS1); // Prescaler 64
}

// Fungsi pembacaan ADC di channel 0
uint16_t read_adc() {
    ADMUX &= 0xF0; // Pilih channel ADC0
    ADCSRA |= (1 << ADSC); // Start conversion
    while (ADCSRA & (1 << ADSC)); // Tunggu hingga selesai
    return ADC;
}

// Inisialisasi USART
void uart_init() {
    UBRR0H = (UBRR_VALUE >> 8);
    UBRR0L = UBRR_VALUE;
    UCSR0B = (1 << TXEN0); // Enable transmitter
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data
}

// Kirim 1 karakter
void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

// Kirim string
void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

// Kirim angka desimal sebagai string
void uart_print_float(const char* label, float value, uint8_t decimal_places) {
    char buffer[32];
    dtostrf(value, 7, decimal_places, buffer); // Konversi float ke string
    uart_puts(label);
    uart_puts(buffer);
    uart_putc('\n');
}

// Kirim integer
void uart_print_int(const char* label, int value) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    uart_puts(label);
    uart_puts(buffer);
    uart_putc('\n');
}

// Main program
int main(void) {
    adc_init();
    uart_init();

    while (1) {
        uint32_t totalPh = 0;

        for (int i = 0; i < JUMLAH_SAMPEL; i++) {
            totalPh += read_adc();
            _delay_ms(1); // Delay 1ms
        }

        int rata2Ph = totalPh / JUMLAH_SAMPEL;
        uart_print_int("Nilai rata-rata ADC Ph: ", rata2Ph);

        float TeganganPh = (VREF / 1024.0) * rata2Ph;
        uart_print_float("TeganganPh: ", TeganganPh, 3);

        float Po = 7.00 + ((2.95 - TeganganPh) / 0.21);
        float reg = 2.693 + 0.5898 * Po;

        uart_print_float("Nilai pH cairan: ", Po, 3);

        _delay_ms(1000); // Delay 1 detik
    }
}
