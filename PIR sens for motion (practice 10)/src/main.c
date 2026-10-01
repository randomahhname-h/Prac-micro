
#include <avr/io.h>
#include <util/delay.h>

#define BAUD 9600
#define BRC ((F_CPU/16/BAUD) - 1)

// Define our pins for readability
#define PIR_PIN PD2
#define LED_PIN PB5

// Function prototypes
void uart_init(void);
void uart_print(const char* str);

int main(void) {
    uart_init(); // Boot up the serial communication
    
    // 1. Configure GPIO Pins
    DDRD &= ~(1 << PIR_PIN); // Set PD2 (PIR) as INPUT (0)
    DDRB |= (1 << LED_PIN);  // Set PB5 (LED) as OUTPUT (1)
    
    // Ensure LED is off at startup
    PORTB &= ~(1 << LED_PIN);
    
    // 2. State Tracking Variables
    // We assume the system starts with no motion (LOW)
    uint8_t last_motion_state = 0; 
    
    uart_print("System Ready. Monitoring area for motion...\r\n");
    
    while(1) {
        // Read the current state of the PIR pin. 
        // If the pin is HIGH, current_state becomes 1. Otherwise, 0.
        uint8_t current_state = (PIND & (1 << PIR_PIN)) ? 1 : 0;
        
        // 3. Check for a state change (edge detection)
        if (current_state != last_motion_state) {
            
            // If the state changed, evaluate the new state
            if (current_state == 1) {
                // Motion just started!
                PORTB |= (1 << LED_PIN); // Turn LED ON
                uart_print("[ALERT] Motion Detected! BUZZER ON.\r\n");
            } else {
                // Motion just stopped!
                PORTB &= ~(1 << LED_PIN); // Turn LED OFF
                uart_print("[INFO] Area Clear. BUZZER OFF.\r\n");
            }
            
            // Update the tracker so we don't trigger this again until the state flips
            last_motion_state = current_state;
        }
        
        // A tiny 50ms delay to act as a debounce buffer
        _delay_ms(50); 
    }
    return 0;
}

void uart_init(void) {
    UBRR0H = (BRC >> 8);
    UBRR0L = BRC;
    
    // We only need the Transmitter (TXEN0) enabled for this project
    UCSR0B = (1 << TXEN0); 
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data
}

void uart_print(const char* str) {
    while (*str) {
        while (!(UCSR0A & (1 << UDRE0))); // Wait for buffer to be empty
        UDR0 = *str++;                    // Send character
    }
}