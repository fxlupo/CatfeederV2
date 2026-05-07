// =============================================================================
// CatFeeder ESP32 – Pin-Konfiguration
// Finale Pinbelegung laut Schaltplan
// =============================================================================
#pragma once

// ─── I2C Bus ────────────────────────────────────────────────────────────────
// Geräte: INA219 (0x40), AS5600 (0x36), DS3231 (0x68), VL53L0X (0x29)
#define PIN_SDA              21
#define PIN_SCL              22

// ─── Stepper Motor (DRV8825 / Red Expansion Board) ─────────────────────────
#define PIN_STEP             25
#define PIN_DIR              26
#define PIN_EN_DRV           27   // LOW = Treiber aktiv

// ─── Servos (PWM Futterklappen) ─────────────────────────────────────────────
#define PIN_SERVO1           18
#define PIN_SERVO2           19

// ─── Endschalter (INPUT_PULLUP, LOW = ausgelöst) ───────────────────────────
#define PIN_S1_OPEN          32
#define PIN_S1_CLOSE         33
#define PIN_S2_OPEN          13
#define PIN_S2_CLOSE         14

// ─── IR Sensoren ────────────────────────────────────────────────────────────
#define PIN_IR1_A0           34   // Analog (Input-Only)
#define PIN_IR2_A0           35   // Analog (Input-Only)
#define PIN_IR1_D0            2   // Digital
#define PIN_IR2_D0            4   // Digital

// ─── VL53L0X ────────────────────────────────────────────────────────────────
#define PIN_VL53_XSHUT        5   // XSHUT (LOW = Standby, HIGH = aktiv)

// ─── I2C-Adressen ───────────────────────────────────────────────────────────
#define I2C_ADDR_INA219    0x40
#define I2C_ADDR_AS5600    0x36
#define I2C_ADDR_DS3231    0x68
#define I2C_ADDR_VL53L0X   0x29

// ─── Freie GPIOs (Reserve) ──────────────────────────────────────────────────
// GPIO  0  – bootkritisch (Strapping-Pin)
// GPIO 12  – bootkritisch (Strapping-Pin)
// GPIO 15  – bootkritisch (Strapping-Pin)
// GPIO 16  – frei
// GPIO 17  – frei
// GPIO 23  – frei
// GPIO 36  – frei (Input-Only / VP)
// GPIO 39  – frei (Input-Only / VN)
