#ifndef USERMOD_MIRI_PINS_H
#define USERMOD_MIRI_PINS_H

// Main I2C bus → PCA @0x62 + TMP1075 @0x49.
// Live scan on hardware: devices ACK on GPIO 9/10 (not 21/22).
#ifndef MIRI_I2C_SDA
#define MIRI_I2C_SDA 9
#endif

#ifndef MIRI_I2C_SCL
#define MIRI_I2C_SCL 10
#endif

// Low-priority I2C bus used by daughterboards (Wire1 on ESP32 targets).
#ifndef MIRI_LP_I2C_SDA
#define MIRI_LP_I2C_SDA 25
#endif

#ifndef MIRI_LP_I2C_SCL
#define MIRI_LP_I2C_SCL 26
#endif

// Power/fuse monitor ADC pin.
#ifndef MIRI_PIN_ADC_FUSE
#define MIRI_PIN_ADC_FUSE 0
#endif

// PWM and bus-control defaults (override in build flags as needed).
#ifndef MIRI_PIN_LED_R
#define MIRI_PIN_LED_R 13
#endif

#ifndef MIRI_PIN_LED_G
#define MIRI_PIN_LED_G 12
#endif

#ifndef MIRI_PIN_LED_B
#define MIRI_PIN_LED_B 14
#endif

#ifndef MIRI_PIN_LED_W
#define MIRI_PIN_LED_W 27
#endif

#ifndef MIRI_PIN_STRIP1_CLK
#define MIRI_PIN_STRIP1_CLK 15
#endif

#ifndef MIRI_PIN_STRIP1_DATA
#define MIRI_PIN_STRIP1_DATA 13
#endif

#ifndef MIRI_PIN_STRIP2_CLK
#define MIRI_PIN_STRIP2_CLK 12
#endif

#ifndef MIRI_PIN_STRIP2_DATA
#define MIRI_PIN_STRIP2_DATA 14
#endif

// Output enable is WLED's relay pin (LED Settings → Relay GPIO), not a
// separate Miri GPIO. On the board that line is typically GPIO5, active-low
// (invert relay in settings so lights-on drives enable).
#ifndef MIRI_PIN_CH_EN
#define MIRI_PIN_CH_EN -1
#endif

#endif
