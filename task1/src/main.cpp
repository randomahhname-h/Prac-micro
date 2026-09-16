
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define BLINK_LED  PB5
#define TOGGLE_LED PB4
#define BUZZER_PIN PB1
#define BUTTON_PIN PD2

void timer1_init(void) {
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << CS12) | (1 << CS10);
    OCR1A = 15625;
    TIMSK1 |= (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect) {
    PORTB ^= (1 << BLINK_LED); 
}

int main(void) {
    // 1. Configure Outputs (LEDs and Buzzer)
    DDRB |= (1 << BLINK_LED) | (1 << TOGGLE_LED) | (1 << BUZZER_PIN);
    
    // 2. Configure Input (Button)
    DDRD &= ~(1 << BUTTON_PIN); // Set as input (0)
    PORTD |= (1 << BUTTON_PIN); // Enable internal pull-up resistor
    
    // 3. Start the background timer
    timer1_init();
    sei(); // Enable global interrupts so the timer can run
    
    // 4. Track the button state to detect presses
    uint8_t last_button_state = 1; // 1 means unpressed (due to pull-up)

    while(1) {
        // Read current state of the button
        uint8_t current_button_state = (PIND & (1 << BUTTON_PIN)) ? 1 : 0;
        
        // Detect a "falling edge" (the moment it goes from 1 to 0)
        if (current_button_state == 0 && last_button_state == 1) {
            
            // Toggle the second LED
            PORTB ^= (1 << TOGGLE_LED);
            
            // Sound the buzzer for 0.3s
            PORTB |= (1 << BUZZER_PIN);
            _delay_ms(300);
            PORTB &= ~(1 << BUZZER_PIN);
        }
        
        last_button_state = current_button_state;
        
        // A small 20ms delay to debounce the physical button spring
        _delay_ms(20); 
    }
    return 0;
}