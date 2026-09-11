#include <Arduino.h>
#include <avr/io.h>

/*
 * ============================================================
 * PRACTICE 8: I2C AND SENSOR
 * ATmega328P + AHT20
 *
 * NO Wire.h
 * NO I2C LIBRARY
 *
 * I2C:
 *   PC4 / A4 = SDA
 *   PC5 / A5 = SCL
 *
 * AHT20:
 *   7-bit I2C Address = 0x38
 * ============================================================
 */

#define AHT20_ADDR 0x38

// ------------------------------------------------------------
// I2C INITIALIZATION
// ------------------------------------------------------------

void i2c_init(void)
{
    /*
     * ATmega328P clock = 16 MHz
     * I2C clock = 100 kHz
     *
     * Formula:
     *
     * SCL = F_CPU / (16 + 2*TWBR*4^TWPS)
     *
     * TWPS = 0
     * TWBR = 72
     */

    TWSR = 0x00;              // Prescaler = 1
    TWBR = 72;                // 100 kHz

    TWCR = (1 << TWEN);       // Enable TWI
}


// ------------------------------------------------------------
// I2C START
// ------------------------------------------------------------

void i2c_start(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWSTA) |
           (1 << TWEN);

    // Wait until START is transmitted
    while (!(TWCR & (1 << TWINT)));
}


// ------------------------------------------------------------
// I2C WRITE
// ------------------------------------------------------------

void i2c_write(uint8_t data)
{
    // Put data into TWI Data Register
    TWDR = data;

    // Start transmission
    TWCR = (1 << TWINT) |
           (1 << TWEN);

    // Wait until transmission is complete
    while (!(TWCR & (1 << TWINT)));
}


// ------------------------------------------------------------
// I2C READ WITH ACK
// ------------------------------------------------------------

uint8_t i2c_read_ack(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWEA)  |
           (1 << TWEN);

    // Wait until data is received
    while (!(TWCR & (1 << TWINT)));

    return TWDR;
}


// ------------------------------------------------------------
// I2C READ WITH NACK
// ------------------------------------------------------------

uint8_t i2c_read_nack(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWEN);

    // Wait until data is received
    while (!(TWCR & (1 << TWINT)));

    return TWDR;
}


// ------------------------------------------------------------
// I2C STOP
// ------------------------------------------------------------

void i2c_stop(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWEN)   |
           (1 << TWSTO);

    // Small delay to make sure STOP is completed
    delayMicroseconds(10);
}


// ============================================================
// AHT20 FUNCTIONS
// ============================================================


// ------------------------------------------------------------
// AHT20 INITIALIZATION
// ------------------------------------------------------------

void AHT20_init(void)
{
    /*
     * AHT20 initialization command:
     *
     * START
     * Address + WRITE
     * 0xBE
     * 0x08
     * 0x00
     * STOP
     */

    i2c_start();

    // AHT20 address + WRITE
    i2c_write((AHT20_ADDR << 1) | 0);

    // Initialization command
    i2c_write(0xBE);
    i2c_write(0x08);
    i2c_write(0x00);

    i2c_stop();

    delay(10);
}


// ------------------------------------------------------------
// START AHT20 MEASUREMENT
// ------------------------------------------------------------

void AHT20_start_measurement(void)
{
    /*
     * Measurement command:
     *
     * START
     * Address + WRITE
     * 0xAC
     * 0x33
     * 0x00
     * STOP
     */

    i2c_start();

    // Address + WRITE
    i2c_write((AHT20_ADDR << 1) | 0);

    // Measurement command
    i2c_write(0xAC);
    i2c_write(0x33);
    i2c_write(0x00);

    i2c_stop();
}


// ------------------------------------------------------------
// READ AHT20 DATA
// ------------------------------------------------------------

void AHT20_read_data(float *temperature, float *humidity)
{
    uint8_t data[7];

    /*
     * Wait for AHT20 measurement
     */
    delay(80);


    /*
     * ============================================
     * READ 7 BYTES
     *
     * START
     * ADDRESS + READ
     * DATA0 -> ACK
     * DATA1 -> ACK
     * DATA2 -> ACK
     * DATA3 -> ACK
     * DATA4 -> ACK
     * DATA5 -> ACK
     * DATA6 -> NACK
     * STOP
     * ============================================
     */

    i2c_start();

    // Address + READ
    i2c_write((AHT20_ADDR << 1) | 1);


    // Read 7 bytes
    data[0] = i2c_read_ack();
    data[1] = i2c_read_ack();
    data[2] = i2c_read_ack();
    data[3] = i2c_read_ack();
    data[4] = i2c_read_ack();
    data[5] = i2c_read_ack();

    // Last byte -> NACK
    data[6] = i2c_read_nack();

    i2c_stop();


    /*
     * ============================================
     * CONVERT RAW DATA
     * ============================================
     */

    // 20-bit raw humidity
    uint32_t raw_humidity;

    raw_humidity =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4)  |
        ((data[3] >> 4) & 0x0F);


    // 20-bit raw temperature
    uint32_t raw_temperature;

    raw_temperature =
        ((uint32_t)(data[3] & 0x0F) << 16) |
        ((uint32_t)data[4] << 8) |
        data[5];


    /*
     * Humidity:
     *
     * RH = raw / 2^20 * 100
     */

    *humidity =
        ((float)raw_humidity * 100.0) / 1048576.0;


    /*
     * Temperature:
     *
     * T = raw / 2^20 * 200 - 50
     */

    *temperature =
        ((float)raw_temperature * 200.0) / 1048576.0 - 50.0;
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    /*
     * Start Serial Monitor
     */
    Serial.begin(9600);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("Practice 8 - I2C + AHT20");
    Serial.println("ATmega328P");
    Serial.println("================================");


    /*
     * Initialize I2C
     */
    i2c_init();

    Serial.println("I2C initialized");


    /*
     * Initialize AHT20
     */
    AHT20_init();

    Serial.println("AHT20 initialized");

    Serial.println();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    float temperature;
    float humidity;


    /*
     * Start measurement
     */
    AHT20_start_measurement();


    /*
     * Read sensor
     */
    AHT20_read_data(&temperature, &humidity);


    /*
     * Display result
     */
    Serial.print("Temperature: ");
    Serial.print(temperature, 2);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity, 2);
    Serial.println(" %");


    Serial.println("----------------------------");


    /*
     * Wait 1 second
     */
    delay(1000);
}