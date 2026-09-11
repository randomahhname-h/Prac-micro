#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#define BAUD 9600

#define UBRR_VALUE ((F_CPU / (16UL * BAUD)) - 1)
volatile uint8_t flag_adc    = 0;
volatile uint8_t flag_led    = 0;
volatile uint8_t flag_sensor = 0;
volatile uint8_t flag_uart   = 0;


volatile uint8_t counter_20ms   = 0;
volatile uint8_t counter_100ms  = 0;
volatile uint8_t counter_1000ms = 0;


void Scheduler_Init(void)
{
    TCCR1A = 0;

    TCCR1B =
        (1 << WGM12) |   // CTC mode
        (1 << CS11)  |
        (1 << CS10);     // Prescaler = 64

    OCR1A = 2499;
    TIMSK1 |= (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect)
{

    flag_adc = 1;
    counter_20ms++;

    if (counter_20ms >= 2)
    {
        counter_20ms = 0;
        flag_led = 1;
    }

    counter_100ms++;

    if (counter_100ms >= 10)
    {
        counter_100ms = 0;
        flag_sensor = 1;
    }

    counter_1000ms++;

    if (counter_1000ms >= 100)
    {
        counter_1000ms = 0;
        flag_uart = 1;
    }
}

void ADC_Task(void)
{
    /*
        Task 1
        Read ADC every 10 ms

        Put your ADC code here.
    */

    // Example:
    // adcValue = ADC_Read(0);
}


void LED_PWM_Task(void)
{
    /*
        Task 2
        Update LED/PWM every 20 ms

        Put your LED/PWM code here.
    */

    // Example:
    // update LED state
    // update PWM duty cycle
}


void Sensor_Task(void)
{
    /*
        Task 3
        Read sensor every 100 ms

        Put your sensor code here.
    */

    // Example:
    // sensorValue = Sensor_Read();
}


void UART_Task(void)
{
     UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE);

    UCSR0B = (1 << TXEN0);

    UCSR0C =
        (1 << UCSZ01) |
        (1 << UCSZ00);
}


void UART_SendChar(char c)
{
    while (!(UCSR0A & (1 << UDRE0)))
    {
    }

    UDR0 = c;
}


void UART_SendString(const char *str)
{
    while (*str)
    {
        UART_SendChar(*str);
        str++;
    }
}


int main(void)
{
    UART_Init();

    while (1)
    {
        UART_SendString("Hello from ATmega328P!\r\n");
    }
}


int main(void)
{
    // Initialize scheduler
    Scheduler_Init();

    // Enable global interrupts
    sei();


    while (1)
    {
        // ------------------------------------------
        // ADC TASK
        // ------------------------------------------

        if (flag_adc)
        {
            flag_adc = 0;
            ADC_Task();
        }


        // ------------------------------------------
        // LED / PWM TASK
        // ------------------------------------------

        if (flag_led)
        {
            flag_led = 0;
            LED_PWM_Task();
        }


        // ------------------------------------------
        // SENSOR TASK
        // ------------------------------------------

        if (flag_sensor)
        {
            flag_sensor = 0;
            Sensor_Task();
        }


        // ------------------------------------------
        // UART TASK
        // ------------------------------------------

        if (flag_uart)
        {
            flag_uart = 0;
            UART_Task();
        }
    }
}