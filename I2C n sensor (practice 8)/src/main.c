
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>      // Required for dtostrf()

// AHT20 7-bit I2C Address
#define AHT20_ADDR 0x38 

/* UART Configuration */
#define BAUD 9600
#define BRC ((F_CPU/16/BAUD) - 1)

void uart_init(void) {
    UBRR0H = (BRC >> 8);
    UBRR0L = BRC;
    
    // We only need the Transmitter (TXEN0) enabled for this project
    UCSR0B = (1 << TXEN0); 
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8-bit data format
}

void uart_print(const char* str) {
    while (*str) {
        // Automatically add carriage return (\r) before newline (\n) 
        // for proper formatting in the Serial Monitor
        if (*str == '\n') {
            while (!(UCSR0A & (1 << UDRE0)));
            UDR0 = '\r'; 
        }

        while (!(UCSR0A & (1 << UDRE0))); // Wait for buffer to be empty
        UDR0 = *str++;                    // Send character and increment pointer
    }
}

/* I2C Functions */
void i2c_init(void) {
    TWSR = 0x00;              // Prescaler = 1
    TWBR = 72;                // 100 kHz bit rate
    TWCR = (1 << TWEN);       // Enable TWI hardware
}

void i2c_start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))); // Wait until START is transmitted
}

void i2c_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))); // Wait until transmission is complete
}

uint8_t i2c_read_ack(void) {
    TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))); // Wait until data is received
    return TWDR;
}

uint8_t i2c_read_nack(void) {
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))); // Wait until data is received
    return TWDR;
}

void i2c_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
    _delay_us(10); // Small delay to ensure STOP is completed
}

/* AHT20 Functions */
void AHT20_init(void) {
    i2c_start();
    i2c_write((AHT20_ADDR << 1) | 0); // Address + WRITE
    
    // Initialization command sequence
    i2c_write(0xBE);
    i2c_write(0x08);
    i2c_write(0x00);
    
    i2c_stop();
    _delay_ms(10); // Wait for initialization
}

void AHT20_start_measurement(void) {
    i2c_start();
    i2c_write((AHT20_ADDR << 1) | 0); // Address + WRITE
    
    // Trigger measurement command
    i2c_write(0xAC);
    i2c_write(0x33);
    i2c_write(0x00);
    
    i2c_stop();
}

void AHT20_read_data(float *temperature, float *humidity) {
    uint8_t data[7];

    _delay_ms(80); // Wait 80ms for AHT20 measurement to complete

    i2c_start();
    i2c_write((AHT20_ADDR << 1) | 1); // Address + READ

    // Read 6 bytes with ACK, 7th byte with NACK
    data[0] = i2c_read_ack();
    data[1] = i2c_read_ack();
    data[2] = i2c_read_ack();
    data[3] = i2c_read_ack();
    data[4] = i2c_read_ack();
    data[5] = i2c_read_ack();
    data[6] = i2c_read_nack();

    i2c_stop();

    // Reconstruct 20-bit raw data
    uint32_t raw_humidity = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((data[3] >> 4) & 0x0F);
    uint32_t raw_temperature = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    // Convert raw data to actual physical values
    *humidity = ((float)raw_humidity * 100.0) / 1048576.0;
    *temperature = ((float)raw_temperature * 200.0) / 1048576.0 - 50.0;
}

int main(void) {
    // System setup
    uart_init();
    
    _delay_ms(1000);

    i2c_init();
    uart_print("I2C initialized\n");

    AHT20_init();
    uart_print("AHT20 initialized\n\n");

    float temperature;
    float humidity;
    
    // Character arrays to store the converted float strings
    char temp_str[10];
    char hum_str[10];

    // Main infinite loop
    while (1) {
        AHT20_start_measurement();
        AHT20_read_data(&temperature, &humidity);

        // Convert float values to strings (min width 5, 2 decimal places)
        dtostrf(temperature, 5, 2, temp_str);
        dtostrf(humidity, 5, 2, hum_str);

        // Print components manually
        uart_print("Temperature: ");
        uart_print(temp_str);
        uart_print(" C\n");

        uart_print("Humidity: ");
        uart_print(hum_str);
        uart_print(" %\n");

        uart_print("----------------------------\n");

        _delay_ms(1000);
    }
    
    return 0;
}