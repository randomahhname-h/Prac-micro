#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>
#include <avr/io.h>
#include <math.h>
#include <stdint.h>

/*
 * Combined sensor node for a TWO-Arduino battery monitor.
 * Arduino #1: two NTC thermistors + INA219 + MCP2515 CAN.
 * Arduino #2: processing/controller node receives CAN IDs 0x100 and 0x200.
 * INA219 calibration assumption:
 *   0.1-ohm shunt and 0.1 mA per current-register LSB (CAL = 4096).
 * Check the shunt marking/value on the physical module before use.
 */

// ---------------- CAN ----------------
#define CAN_CS_PIN 10
#define CAN_ID_TEMP  0x100UL
#define CAN_ID_POWER 0x200UL
#define CAN_BITRATE  CAN_500KBPS
#define CAN_CLOCK    MCP_8MHZ
MCP_CAN CAN0(CAN_CS_PIN);

// ---------------- INA219 registers ----------------
#define INA_REG_CONFIG  0x00
#define INA_REG_SHUNT   0x01
#define INA_REG_BUS     0x02
#define INA_REG_CURRENT 0x04
#define INA_REG_CAL     0x05

#define INA_CONFIG_VALUE 0x399F  // 32-V full-scale setting, PGA /8, 12-bit, continuous mode
#define INA_CAL_VALUE    4096    // For 0.1-ohm shunt and 0.1-mA current LSB only
static uint8_t inaAddress = 0x40;
static bool inaReady = false;

// ---------------- NTC configuration ----------------
#define NTC1_CH 0               // A0 / ADC0
#define NTC2_CH 1               // A1 / ADC1
const float NTC_R_FIXED = 10000.0f;
const float NTC_R25     = 10000.0f;
const float NTC_BETA    = 3950.0f;
const float NTC_T25_K   = 298.15f;

// ---------------- Task periods ----------------
const uint32_t TEMP_TX_INTERVAL_MS   = 500;
const uint32_t POWER_TX_INTERVAL_MS  = 200;
const uint32_t INA_RETRY_INTERVAL_MS = 2000;
const uint32_t DEBUG_INTERVAL_MS     = 1000;

static uint32_t lastTempTx = 0;
static uint32_t lastPowerTx = 0;
static uint32_t lastInaRetry = 0;
static uint32_t lastDebug = 0;

static int16_t lastTemp1_x10 = 0;
static int16_t lastTemp2_x10 = 0;
static uint8_t lastNtcFaultMask = 0x03;
static uint16_t lastVoltage_mV = 0;
static int16_t lastCurrent_mA = 0;
static int16_t lastPower_mW = 0;
static bool lastPowerValid = false;

// ============================================================
// AVR ADC: AVcc reference, ADC clock = 16 MHz / 128 = 125 kHz
// ============================================================
static void adc_init_avr(void) {
    ADMUX = (1 << REFS0);  // AVcc reference, right-adjusted result
    ADCSRA = (1 << ADEN) |
             (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    DIDR0 |= (1 << ADC0D) | (1 << ADC1D);
}

static uint16_t adc_read_avr(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {
        // Wait for one ADC conversion.
    }
    return ADC;
}

static bool read_ntc_x10(uint8_t channel, int16_t &temp_x10) {
    const uint16_t raw = adc_read_avr(channel);

    // ADC near either rail often means open/shorted sensor or wiring fault.
    if (raw <= 2 || raw >= 1021) {
        temp_x10 = 0;
        return false;
    }

    // For +5V -> NTC -> ADC node -> R_FIXED -> GND.
    const float rNtc = NTC_R_FIXED * (1023.0f - (float)raw) / (float)raw;
    if (!(rNtc > 0.0f)) {
        temp_x10 = 0;
        return false;
    }

    const float invT = (1.0f / NTC_T25_K) +
                       (1.0f / NTC_BETA) * logf(rNtc / NTC_R25);
    if (!(invT > 0.0f)) {
        temp_x10 = 0;
        return false;
    }

    const float tempC = (1.0f / invT) - 273.15f;
    // Plausibility guard; separate from the battery's warning/critical limits.
    if (tempC < -40.0f || tempC > 150.0f) {
        temp_x10 = 0;
        return false;
    }

    const float scaled = tempC * 10.0f;
    temp_x10 = (int16_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
    return true;
}

// ============================================================
// AVR TWI / I2C master, 100 kHz at 16 MHz
// A timeout prevents permanent blocking if the bus fails.
// ============================================================
static void twi_init(void) {
    TWSR = 0x00;  // prescaler = 1
    TWBR = 72;    // SCL = 16 MHz / (16 + 2*72) = 100 kHz
    TWCR = (1 << TWEN);
}

static bool twi_wait_int(void) {
    const uint32_t start = micros();
    while (!(TWCR & (1 << TWINT))) {
        if ((uint32_t)(micros() - start) > 3000UL) return false;
    }
    return true;
}

static void twi_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    const uint32_t start = micros();
    while (TWCR & (1 << TWSTO)) {
        if ((uint32_t)(micros() - start) > 1000UL) break;
    }
}

static bool twi_start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!twi_wait_int()) return false;
    const uint8_t status = TWSR & 0xF8;
    return status == 0x08 || status == 0x10;
}

static bool twi_write_expect(uint8_t value, uint8_t expectedStatus) {
    TWDR = value;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait_int()) return false;
    return (TWSR & 0xF8) == expectedStatus;
}

static bool twi_read_ack(uint8_t &value) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    if (!twi_wait_int()) return false;
    value = TWDR;
    return (TWSR & 0xF8) == 0x50;
}

static bool twi_read_nack(uint8_t &value) {
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait_int()) return false;
    value = TWDR;
    return (TWSR & 0xF8) == 0x58;
}

static bool ina_probe(uint8_t address) {
    if (!twi_start()) {
        twi_stop();
        return false;
    }
    const bool ack = twi_write_expect((uint8_t)(address << 1), 0x18);
    twi_stop();
    return ack;
}

static bool ina_find_address(void) {
    for (uint8_t address = 0x40; address <= 0x4F; ++address) {
        if (ina_probe(address)) {
            inaAddress = address;
            return true;
        }
    }
    return false;
}

static bool ina_read_reg(uint8_t reg, uint16_t &value) {
    uint8_t hi = 0, lo = 0;
    if (!twi_start() ||
        !twi_write_expect((uint8_t)(inaAddress << 1), 0x18) ||
        !twi_write_expect(reg, 0x28) ||
        !twi_start() ||
        !twi_write_expect((uint8_t)((inaAddress << 1) | 1), 0x40) ||
        !twi_read_ack(hi) || !twi_read_nack(lo)) {
        twi_stop();
        return false;
    }
    twi_stop();
    value = ((uint16_t)hi << 8) | lo;
    return true;
}

static bool ina_write_reg(uint8_t reg, uint16_t value) {
    if (!twi_start() ||
        !twi_write_expect((uint8_t)(inaAddress << 1), 0x18) ||
        !twi_write_expect(reg, 0x28) ||
        !twi_write_expect((uint8_t)(value >> 8), 0x28) ||
        !twi_write_expect((uint8_t)value, 0x28)) {
        twi_stop();
        return false;
    }
    twi_stop();
    return true;
}

static bool ina_init(void) {
    if (!ina_write_reg(INA_REG_CONFIG, INA_CONFIG_VALUE)) return false;
    if (!ina_write_reg(INA_REG_CAL, INA_CAL_VALUE)) return false;

    uint16_t configReadback = 0, calReadback = 0;
    if (!ina_read_reg(INA_REG_CONFIG, configReadback) ||
        !ina_read_reg(INA_REG_CAL, calReadback)) {
        return false;
    }
    return configReadback == INA_CONFIG_VALUE && calReadback == INA_CAL_VALUE;
}

static bool ina_read_measurements(uint16_t &pack_mV, int16_t &current_mA) {
    uint16_t busRaw = 0, shuntRawU = 0, currentRawU = 0;
    if (!ina_read_reg(INA_REG_BUS, busRaw) ||
        !ina_read_reg(INA_REG_SHUNT, shuntRawU) ||
        !ina_read_reg(INA_REG_CURRENT, currentRawU)) {
        return false;
    }

    // INA219 bus register bits [15:3] have 4 mV/LSB; bit 0 is overflow.
    if (busRaw & 0x0001) return false;

    const uint16_t bus_mV = (uint16_t)(((busRaw >> 3) & 0x1FFF) * 4U);
    const int16_t shuntRaw = (int16_t)shuntRawU;
    const int16_t currentRaw = (int16_t)currentRawU;
    const int32_t shunt_uV = (int32_t)shuntRaw * 10L;

    // INA219 bus register measures VIN- relative to GND. Add Vshunt
    // (VIN+ - VIN-) to estimate pack-side voltage at VIN+.
    const int32_t pack_uV = (int32_t)bus_mV * 1000L + shunt_uV;
    int32_t packMilliV = (pack_uV >= 0) ? (pack_uV + 500L) / 1000L
                                       : (pack_uV - 500L) / 1000L;
    if (packMilliV < 0 || packMilliV > 65535L) return false;
    pack_mV = (uint16_t)packMilliV;

    // CAL=4096 with 0.1-ohm shunt gives current LSB = 0.1 mA.
    // Convert to whole mA with rounding; negative values denote reverse current.
    int32_t roundedCurrent = (currentRaw >= 0)
                           ? ((int32_t)currentRaw + 5L) / 10L
                           : ((int32_t)currentRaw - 5L) / 10L;
    if (roundedCurrent < -32768L || roundedCurrent > 32767L) return false;
    current_mA = (int16_t)roundedCurrent;
    return true;
}

// ============================================================
// CAN payloads retained for compatibility with controller file:
// ID 0x100: temp1_x10 (i16 BE), temp2_x10 (i16 BE), NTC fault mask
// ID 0x200: voltage_mV (u16 BE), current_mA (i16 BE), power_mW (i16 BE)
// ============================================================
static bool send_temperature_frame(void) {
    int16_t t1 = 0, t2 = 0;
    const bool t1Valid = read_ntc_x10(NTC1_CH, t1);
    const bool t2Valid = read_ntc_x10(NTC2_CH, t2);
    const uint8_t faultMask = (t1Valid ? 0 : 0x01) | (t2Valid ? 0 : 0x02);

    uint8_t data[5];
    data[0] = (uint8_t)((uint16_t)t1 >> 8);
    data[1] = (uint8_t)t1;
    data[2] = (uint8_t)((uint16_t)t2 >> 8);
    data[3] = (uint8_t)t2;
    data[4] = faultMask;

    if (CAN0.sendMsgBuf(CAN_ID_TEMP, 0, sizeof(data), data) != CAN_OK) return false;

    lastTemp1_x10 = t1;
    lastTemp2_x10 = t2;
    lastNtcFaultMask = faultMask;
    return true;
}

static bool send_power_frame(void) {
    uint16_t voltage_mV = 0;
    int16_t current_mA = 0;
    if (!inaReady || !ina_read_measurements(voltage_mV, current_mA)) {
        lastPowerValid = false;
        return false;
    }

    // mV * mA / 1000 = mW. Keep the sign for charge/discharge direction.
    const int32_t p32_mW = ((int32_t)voltage_mV * (int32_t)current_mA) / 1000L;
    if (p32_mW < -32768L || p32_mW > 32767L) {
        lastPowerValid = false;
        return false;
    }
    const int16_t power_mW = (int16_t)p32_mW;

    uint8_t data[6];
    data[0] = (uint8_t)(voltage_mV >> 8);
    data[1] = (uint8_t)voltage_mV;
    data[2] = (uint8_t)((uint16_t)current_mA >> 8);
    data[3] = (uint8_t)current_mA;
    data[4] = (uint8_t)((uint16_t)power_mW >> 8);
    data[5] = (uint8_t)power_mW;

    if (CAN0.sendMsgBuf(CAN_ID_POWER, 0, sizeof(data), data) != CAN_OK) {
        lastPowerValid = false;
        return false;
    }

    lastVoltage_mV = voltage_mV;
    lastCurrent_mA = current_mA;
    lastPower_mW = power_mW;
    lastPowerValid = true;
    return true;
}

static bool init_ina_if_available(void) {
    if (!ina_find_address()) return false;
    return ina_init();
}

void setup(void) {
    Serial.begin(115200);
    adc_init_avr();
    twi_init();
    
Serial.println("Scanning INA219...");

if (ina_find_address()) {
    Serial.print("INA219 found at 0x");
    Serial.println(inaAddress, HEX);
} else {
    Serial.println("INA219 NOT FOUND");
}
    SPI.begin();

    pinMode(CAN_CS_PIN, OUTPUT);
    digitalWrite(CAN_CS_PIN, HIGH);

    Serial.println(F("Combined measurement node booting..."));
    Serial.println(F("NTC inputs: A0/A1; INA219 via I2C A4/A5; MCP2515 via SPI D10-D13"));

    inaReady = init_ina_if_available();
    if (inaReady) {
        Serial.print(F("INA219 initialized at I2C address 0x"));
        Serial.println(inaAddress, HEX);
    } else {
        Serial.println(F("WARNING: INA219 not found/initialized; will retry"));
    }

    while (CAN0.begin(MCP_ANY, CAN_BITRATE, CAN_CLOCK) != CAN_OK) {
        Serial.println(F("MCP2515 init failed; check wiring, module clock and power"));
        delay(500);
    }
    CAN0.setMode(MCP_NORMAL);
    Serial.println(F("CAN initialized: 500 kbps; IDs 0x100 and 0x200"));
}

void loop(void) {
    const uint32_t now = millis();

    if ((uint32_t)(now - lastTempTx) >= TEMP_TX_INTERVAL_MS) {
        lastTempTx = now;
        if (!send_temperature_frame()) {
            Serial.println(F("WARNING: CAN temperature frame TX failed"));
        }
    }

    if ((uint32_t)(now - lastPowerTx) >= POWER_TX_INTERVAL_MS) {
        lastPowerTx = now;
        if (inaReady) {
            send_power_frame();
        } else if ((uint32_t)(now - lastInaRetry) >= INA_RETRY_INTERVAL_MS) {
            lastInaRetry = now;
            inaReady = init_ina_if_available();
            if (inaReady) Serial.println(F("INA219 recovered and initialized"));
        }
    }

    if ((uint32_t)(now - lastDebug) >= DEBUG_INTERVAL_MS) {
        lastDebug = now;
        Serial.print(F("NTC1="));
        if (lastNtcFaultMask & 0x01) Serial.print(F("FAULT"));
        else Serial.print(lastTemp1_x10 / 10.0f, 1);
        Serial.print(F(" C, NTC2="));
        if (lastNtcFaultMask & 0x02) Serial.print(F("FAULT"));
        else Serial.print(lastTemp2_x10 / 10.0f, 1);
        Serial.print(F(" C, INA219="));
        if (lastPowerValid) {
            Serial.print(lastVoltage_mV / 1000.0f, 3);
            Serial.print(F(" V, "));
            Serial.print(lastCurrent_mA / 1000.0f, 3);
            Serial.print(F(" A, "));
            Serial.print(lastPower_mW / 1000.0f, 3);
            Serial.println(F(" W"));
        } else {
            Serial.println(F("NO VALID DATA"));
        }
    }
}
