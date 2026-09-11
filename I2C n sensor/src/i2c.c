#include <avr/io.h>
#include <stdint.h>


#define I2C_FREQ 100000UL

void i2c_init(void)
{
    // Prescaler = 1
    TWSR = 0x00;

    // 100 kHz I2C clock
    TWBR = (uint8_t)((F_CPU / I2C_FREQ - 16) / 2);

    // Enable TWI
    TWCR = (1 << TWEN);
}

void i2c_start(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWSTA) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)));
}

uint8_t i2c_write(uint8_t data)
{
    TWDR = data;

    TWCR = (1 << TWINT) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)));

    return (TWSR & 0xF8);
}

uint8_t i2c_read_ack(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWEA)  |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)));

    return TWDR;
}

uint8_t i2c_read_nack(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)));

    return TWDR;
}

void i2c_stop(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWSTO) |
           (1 << TWEN);
}