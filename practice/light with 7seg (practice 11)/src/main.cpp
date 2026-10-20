
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

#define BAUD 9600
#define BRC ((F_CPU/16/BAUD) - 1)
#define DARK_THRESHOLD 512 // Adjust based on your room's lighting

// 7-Segment Bitmasks for 0-9 (Bit 6=G, Bit 0=A)
const uint8_t digit_map[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// Global array holding the 4 numbers to display
volatile uint8_t display_buffer[4] = {0, 0, 0, 0};

// --- Function Prototypes ---
void init_hardware(void);
uint16_t adc_read(uint8_t channel);
void uart_print(const char* str);
void update_display_buffer(uint16_t value);

// --- Timer0 Interrupt for Multiplexing ---
// This runs automatically ~1000 times a second
// --- Timer0 Interrupt for Multiplexing (COMMON ANODE FIX) ---
// --- Timer0 Interrupt for Multiplexing (ACTIVE-LOW DIGITS FIX) ---
ISR(TIMER0_OVF_vect) {
    static uint8_t current_digit = 0;
    
    // 1. Turn OFF all digits (Active LOW: pull HIGH to turn OFF)
    PORTC |= (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4);
    
    // 2. Load the segment data and invert it for Common Anode
    uint8_t mask = ~digit_map[display_buffer[current_digit]];
    
    // Write Segments A-F to PD2-PD7
    PORTD = (PORTD & 0x03) | ((mask & 0x3F) << 2);
    // Write Segment G to PB0
    PORTB = (PORTB & 0xFE) | ((mask & 0x40) >> 6);
    
    // 3. Turn ON the specific digit (Active LOW: pull LOW to turn ON)
    PORTC &= ~(1 << (PC1 + current_digit));
    
    // 4. Move to the next digit for the next interrupt
    current_digit++;
    if (current_digit > 3) current_digit = 0;
}

int main(void) {
    init_hardware();
    char serial_buffer[50];
    
    sei(); // Enable global interrupts to start the display multiplexing
    
    uart_print("Light Sensor Active.\r\n");

    while(1) {
        // 1. Read the LDR (0 to 1023)
        uint16_t light_val = adc_read(0);
        
        // 2. Update the 7-segment display buffer
        update_display_buffer(light_val);
        
        // 3. Logic & Serial Output
        // In our voltage divider, Dark = high resistance = lower voltage (ADC goes toward 0)
        if (light_val < DARK_THRESHOLD) {
            PORTB |= (1 << PB5); // Turn LED ON
            sprintf(serial_buffer, "ADC: %04d | State: DARK  | LED: ON \r\n", light_val);
        } else {
            PORTB &= ~(1 << PB5); // Turn LED OFF
            sprintf(serial_buffer, "ADC: %04d | State: BRIGHT| LED: OFF\r\n", light_val);
        }
        
        uart_print(serial_buffer);
        
        // Wait 500ms before reading again. 
        // Notice the display does NOT flicker during this delay thanks to the interrupt!
        _delay_ms(500); 
    }
    return 0;
}

void init_hardware(void) {
    // UART Init
    UBRR0H = (BRC >> 8);
    UBRR0L = BRC;
    UCSR0B = (1 << TXEN0); 
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

    // ADC Init
    // REFS0 sets reference to AVCC (5V). 
    ADMUX = (1 << REFS0); 
    // ADEN enables ADC. ADPS[0-2] sets prescaler to 128 for 125kHz ADC clock
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    // Timer0 Init for Display Multiplexing
    // Prescaler 64 -> (16MHz / 64 / 256) = ~976 Hz overflow interrupt
    TCCR0B = (1 << CS01) | (1 << CS00);
    TIMSK0 = (1 << TOIE0); // Enable Timer0 Overflow Interrupt

    // GPIO Init
    DDRD |= 0xFC; // PD2-PD7 as output (Segments A-F)
    DDRB |= (1 << PB0) | (1 << PB5); // PB0 (Segment G) and PB5 (LED) as output
    DDRC |= (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4); // PC1-PC4 as output (Digits)
}

uint16_t adc_read(uint8_t channel) {
    // Ensure channel is 0-7, set it in the ADMUX register
    ADMUX = (ADMUX & 0xF8) | (channel & 0x07);
    
    // Start the conversion
    ADCSRA |= (1 << ADSC);
    
    // Wait for conversion to complete (ADSC bit automatically clears)
    while (ADCSRA & (1 << ADSC));
    
    // Read ADC register (combines ADCL and ADCH automatically in AVR-GCC)
    return ADC;
}

void update_display_buffer(uint16_t value) {
    // Chop the 0-1023 integer into 4 individual digits for the display array
    display_buffer[0] = (value / 1000) % 10; // Thousands
    display_buffer[1] = (value / 100) % 10;  // Hundreds
    display_buffer[2] = (value / 10) % 10;   // Tens
    display_buffer[3] = value % 10;          // Ones
}

void uart_print(const char* str) {
    while (*str) {
        while (!(UCSR0A & (1 << UDRE0))); 
        UDR0 = *str++;                    
    }
}