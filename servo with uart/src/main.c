#include <avr/io.h>
#include <stdlib.h>

#define BAUD 9600
#define BRC ((F_CPU/16/BAUD) - 1)

// Function prototypes
void uart_init(void);
void uart_print(const char* str);
void pwm_init(void);
void set_servo(int angle);

int main(void) {
    uart_init();
    pwm_init();
    
    char buffer[10];
    uint8_t index = 0;
    
    uart_print("System Ready. Enter angle (0-180):\r\n> ");
    
    while(1) {
        // Check if unread serial data is available in the buffer
        if (UCSR0A & (1 << RXC0)) {
            char c = UDR0; 
            
            // Echo the character back so you can see what you are typing
            while (!(UCSR0A & (1 << UDRE0)));
            UDR0 = c;
            
            // If Enter key is pressed (Carriage Return or Line Feed)
            if (c == '\r' || c == '\n') {
                if (index > 0) {
                    buffer[index] = '\0';     // Terminate the string
                    int angle = atoi(buffer); // Convert string to integer
                    
                    // Task: Validate whether the angle is within range
                    if (angle >= 0 && angle <= 180) {
                        set_servo(angle); // Rotate servo
                        uart_print("\r\n[SUCCESS] Servo moved to requested angle.\r\n> ");
                    } else {
                        uart_print("\r\n[ERROR] Angle out of bounds. Must be 0-180.\r\n> ");
                    }
                    index = 0; // Reset buffer for the next command
                }
            } 
            // Store numbers into the buffer
            else if (c >= '0' && c <= '9' && index < 9) {
                buffer[index++] = c;
            }
        }
    }
    return 0;
}

void uart_init(void) {
    // Set baud rate
    UBRR0H = (BRC >> 8);
    UBRR0L = BRC;
    // Enable Receiver and Transmitter
    UCSR0B = (1 << TXEN0) | (1 << RXEN0); 
    // 8-bit data format
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); 
}

void uart_print(const char* str) {
    while (*str) {
        while (!(UCSR0A & (1 << UDRE0))); // Wait for empty transmit buffer
        UDR0 = *str++;                    // Put data into buffer, sends the data
    }
}

void pwm_init(void) {
    DDRB |= (1 << PB1); // Set PB1 as output
    
    // Timer1: Fast PWM Mode 14 (Top = ICR1)
    // Prescaler = 8. (16MHz / 8 = 2MHz timer clock -> 0.5 microseconds per tick)
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    
    // Set period to exactly 20ms (50Hz) required by servos
    // 20,000 microseconds / 0.5us per tick = 40,000 ticks. (0 to 39999)
    ICR1 = 39999; 
    
    // Start at 0 degrees (1ms pulse = 2000 ticks)
    OCR1A = 1000; 
}

void set_servo(int angle) {
    // A 0 degree angle requires a 1ms pulse (2000 ticks)
    // A 180 degree angle requires a 2ms pulse (4000 ticks)
    // We map 0-180 mathematically into the 1000-4000 tick range.
    long ticks = 1000 + ((long)angle * 3999) / 180;
    
    // Loading the ticks into OCR1A immediately changes the PWM pulse width
    OCR1A = ticks;
}