// #include <avr/io.h>
// #include <util/delay.h>

// #define RS PB0
// #define EN  PB1

// void LCD_pulse_enable(void)
// {
//     PORTB |= (1 << EN);
//     _delay_us(1);

//     PORTB &= ~(1 << EN);
//     _delay_us(100);
// }

// void LCD_send_nibble(uint8_t data)
// {
//     PORTB &= 0x03;              // Keep PB0/PB1, clear PB2-PB5
//     PORTB |= (data << 2);       // Put data on PB2-PB5

//     LCD_pulse_enable();
// }

// void LCD_command(uint8_t cmd)
// {
//     PORTB &= ~(1 << RS);        // RS = 0

//     LCD_send_nibble(cmd >> 4);  // High nibble
//     LCD_send_nibble(cmd & 0x0F); // Low nibble

//     _delay_ms(2);
// }

// void LCD_data(uint8_t data)
// {
//     PORTB |= (1 << RS);         // RS = 1

//     LCD_send_nibble(data >> 4);  // High nibble
//     LCD_send_nibble(data & 0x0F); // Low nibble

//     _delay_us(100);
// }

// void LCD_init(void)
// {
//     // PB0-PB5 = outputs
//     DDRB |= 0x3F;

//     _delay_ms(20);

//     // Initialisation sequence
//     LCD_send_nibble(0x03);
//     _delay_ms(5);

//     LCD_send_nibble(0x03);
//     _delay_us(150);

//     LCD_send_nibble(0x03);

//     // 4-bit mode
//     LCD_send_nibble(0x02);

//     // 4-bit, 2 lines, 5x8 font
//     LCD_command(0x28);

//     // Display ON, cursor OFF
//     LCD_command(0x0C);

//     // Clear display
//     LCD_command(0x01);

//     // Entry mode
//     LCD_command(0x06);
// }

// void LCD_print(const char *str)
// {
//     while (*str)
//     {
//         LCD_data(*str);
//         str++;
//     }
// }

// int main(void)
// {
//     LCD_init();

//     LCD_print("Hello World!");

//     while (1)
//     {
//     }
// }

#include <avr/io.h>
#include <util/delay.h>



