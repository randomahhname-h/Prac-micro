#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define SERVO_MIN_PULSE  1000
#define SERVO_MAX_PULSE  5000
// LED1-LED4 -> PB2-PB5
#define LED1 PB2
#define LED2 PB3
#define LED3 PB4
#define LED4 PB5

// LED5-LED8 -> PD1-PD4
#define LED5 PD1
#define LED6 PD2
#define LED7 PD3
#define LED8 PD4

// ======================================================
// TIMER1 / SERVO INITIALIZATION
// ======================================================
void Led_Init(void)
{
  DDRB |= (1 << LED1); 
   PORTB &= ~(1 << LED1);
}
    
void Servo_Init(void)
{
    // PB1 = OC1A = Timer1 PWM output
    DDRB |= (1 << PB1);

    /*
        Fast PWM Mode 14
        TOP = ICR1
    */

    TCCR1A =
        (1 << COM1A1) |
        (1 << WGM11);
 
    TCCR1B =
        (1 << WGM13) |
        (1 << WGM12) |
        (1 << CS11);

    /*
        F_CPU = 16 MHz
        Prescaler = 8

        Timer clock = 2 MHz
        Timer tick = 0.5 us

        20 ms period:
        20 ms / 0.5 us = 40000

        TOP = 39999
    */

    ICR1 = 39999;
}


// ======================================================
// SET SERVO ANGLE
// ======================================================

void Servo_SetAngle(uint8_t angle)
{
    uint16_t pulse;

    /*
        Convert:
        0°   -> 1000
        180° -> 5000

        Number of pulse counts:
        5000 - 1000 = 4000

        Therefore:

        pulse = 2000 + angle * 2000 / 180
    */

    pulse =
        SERVO_MIN_PULSE +
        ((uint32_t)angle *
         (SERVO_MAX_PULSE - SERVO_MIN_PULSE)) / 180;

    OCR1A = pulse;
}


// ======================================================
// MAIN
// ======================================================

int main(void)
{
    uint8_t angle;

    Led_Init();
    Servo_Init();

    while (1)
    {
        // 0° -> 180°
        for (angle = 0; angle <= 180; angle += 1)
        {
            Servo_SetAngle(angle);
            _delay_ms(10);
        }

        // 180° -> 0°
        for (angle = 180; angle >= 1; angle -= 1)
        {
            Servo_SetAngle(angle);
            _delay_ms(10);

        }
        if (angle == 0) {
            PORTB |= (1 << LED1); // Turn on LED1
            _delay_ms(500);
            PORTB &= ~(1 << LED1); // Turn off LED1
        }
    }
}