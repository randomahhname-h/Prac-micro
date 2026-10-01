#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <avr/interrupt.h>

#define LCD_PORT PORTD
#define LCD_DDR  DDRD
#define RS PD2
#define EN PD3
#define D4 PD4
#define D5 PD5
#define D6 PD6
#define D7 PD7

// ADC GLOBAL VARIABLES

volatile uint32_t adcSum = 0;
volatile uint16_t adcAverage = 0;
volatile uint8_t sampleCount = 0;
volatile uint8_t samplesReady = 0;

void LCD_PulseEnable(void)
{
    LCD_PORT |= (1 << EN);
    _delay_us(1);

    LCD_PORT &= ~(1 << EN);
    _delay_us(100);
}

void LCD_SendNibble(uint8_t data)
{
    // Clear D4-D7
    LCD_PORT &= ~((1 << D4) |
                  (1 << D5) |
                  (1 << D6) |
                  (1 << D7));

    // Put data on D4-D7
    if (data & 0x01)
        LCD_PORT |= (1 << D4);

    if (data & 0x02)
        LCD_PORT |= (1 << D5);

    if (data & 0x04)
        LCD_PORT |= (1 << D6);

    if (data & 0x08)
        LCD_PORT |= (1 << D7);

    LCD_PulseEnable();
}


void LCD_Command(uint8_t cmd)
{
    // RS = 0: command
    LCD_PORT &= ~(1 << RS);
    LCD_SendNibble(cmd >> 4);

    LCD_SendNibble(cmd & 0x0F);

    _delay_ms(2);
}


void LCD_Data(uint8_t data)
{
    // RS = 1: data
    LCD_PORT |= (1 << RS);
    LCD_SendNibble(data >> 4);
  
    LCD_SendNibble(data & 0x0F);

    _delay_us(50);
}


void LCD_Init(void)
{
    // Set LCD pins as outputs
    LCD_DDR |= (1 << RS) |
               (1 << EN) |
               (1 << D4) |
               (1 << D5) |
               (1 << D6) |
               (1 << D7);

    _delay_ms(20);

    // RS = 0
    LCD_PORT &= ~(1 << RS);

    LCD_SendNibble(0x03);
    _delay_ms(5);

    LCD_SendNibble(0x03);
    _delay_us(150);

    LCD_SendNibble(0x03);
    _delay_us(150);

    // 4-bit mode
    LCD_SendNibble(0x02);

    // 4-bit, 2 lines, 5x8 font
    LCD_Command(0x28);

    // Display ON, cursor OFF
    LCD_Command(0x0C);

    // Entry mode
    LCD_Command(0x06);

    // Clear display
    LCD_Command(0x01);

    _delay_ms(2);
}


void LCD_Clear(void)
{
    LCD_Command(0x01);
    _delay_ms(2);
}


void LCD_SetCursor(uint8_t row, uint8_t column)
{
    uint8_t address;

    if (row == 0)
        address = 0x00 + column;
    else
        address = 0x40 + column;

    LCD_Command(0x80 | address);
}

void LCD_Print(const char *str)
{
    while (*str)
    {
        LCD_Data(*str);
        str++;
    }
}

// NUMBER PRINTING

void LCD_PrintNumber(uint16_t number)
{
    char buffer[6];
    uint8_t i = 0;

    if (number == 0)
    {
        LCD_Data('0');
        return;
    }

    // Extract digits backwards
    while (number > 0)
    {
        buffer[i++] = '0' + (number % 10);
        number /= 10;
    }

    // Print digits forwards
    while (i > 0)
    {
        LCD_Data(buffer[--i]);
    }
}

// ADC INITIALIZATION

void ADC_Init(void)
{
    // AVCC reference, ADC0 selected
    ADMUX = (1 << REFS0);
       //Prescaler = 128
    ADCSRA =
        (1 << ADEN)  |
        (1 << ADATE) |
        (1 << ADIE)  |
        (1 << ADPS2) |
        (1 << ADPS1) |
        (1 << ADPS0);
      // Free Running mode

    ADCSRB = 0x00;

      // ADC0 as analog input.
    DIDR0 = (1 << ADC0D);
}

// ADC INTERRUPT

ISR(ADC_vect)
{
    uint16_t value;
    value = ADC;
    adcSum += value;
    sampleCount++;
    if (sampleCount >= 16)
    {
        adcAverage = adcSum / 16;
        adcSum = 0;
        sampleCount = 0;
        samplesReady = 1;
    }
}

int main(void)
{
    uint16_t voltage_mV;
    LCD_Init();

    ADC_Init();

    sei();

    ADCSRA |= (1 << ADSC);

    LCD_Clear();

    while (1)
    {
      
        if (samplesReady)
        {
            samplesReady = 0;
            voltage_mV =
                ((uint32_t)adcAverage * 5000) / 1023;

            // DISPLAY ADC VALUE

            LCD_SetCursor(0, 0);

            LCD_Print("ADCavg: ");
            LCD_PrintNumber(adcAverage);

            // Clear remaining characters
            LCD_Print("    ");

            // DISPLAY VOLTAGE

            LCD_SetCursor(1, 0);

            LCD_Print("V: ");

            // Integer part
            LCD_PrintNumber(voltage_mV / 1000);

            LCD_Data('.');

            // Two decimal places
            LCD_PrintNumber((voltage_mV % 1000) / 10);

            LCD_Print(" V ");
            _delay_ms(300);       //For LCD readability.

        }
    }
}