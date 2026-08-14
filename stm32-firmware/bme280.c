/* bme280.c - Fixed: calibration reading added */
#include "bme280.h"

extern I2C_HandleTypeDef hi2c1;

#define BME280_ADDR_76    (0x76 << 1)
#define BME280_ADDR_77    (0x77 << 1)
#define BME280_REG_ID     0xD0
#define BME280_REG_RESET  0xE0
#define BME280_REG_CTRL_HUM  0xF2
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG    0xF5

static int32_t t_fine = 0;

/* NOT static - so we can check over USB if needed */
uint8_t bme280_address = 0;

static uint16_t dig_T1, dig_P1;
static int16_t  dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5,
                dig_P6, dig_P7, dig_P8, dig_P9;

/* ✅ THE MISSING FUNCTION - reads calibration from sensor */
static void BME280_ReadCalibration(void)
{
    uint8_t cal[24];

    if(HAL_I2C_Mem_Read(&hi2c1, bme280_address, 0x88, 1, cal, 24, 100) != HAL_OK)
        return;

    dig_T1 = (cal[1]  << 8) | cal[0];
    dig_T2 = (cal[3]  << 8) | cal[2];
    dig_T3 = (cal[5]  << 8) | cal[4];

    dig_P1 = (cal[7]  << 8) | cal[6];
    dig_P2 = (cal[9]  << 8) | cal[8];
    dig_P3 = (cal[11] << 8) | cal[10];
    dig_P4 = (cal[13] << 8) | cal[12];
    dig_P5 = (cal[15] << 8) | cal[14];
    dig_P6 = (cal[17] << 8) | cal[16];
    dig_P7 = (cal[19] << 8) | cal[18];
    dig_P8 = (cal[21] << 8) | cal[20];
    dig_P9 = (cal[23] << 8) | cal[22];
}

void BME280_Init(void)
{
    uint8_t id = 0, cfg = 0;

    /* Detect address */
    if(HAL_I2C_Mem_Read(&hi2c1, BME280_ADDR_76, BME280_REG_ID, 1, &id, 1, 100) == HAL_OK && id == 0x60)
        bme280_address = BME280_ADDR_76;
    else if(HAL_I2C_Mem_Read(&hi2c1, BME280_ADDR_77, BME280_REG_ID, 1, &id, 1, 100) == HAL_OK && id == 0x60)
        bme280_address = BME280_ADDR_77;
    else
    {
        bme280_address = 0;
        return;
    }

    /* Soft reset */
    cfg = 0xB6;
    HAL_I2C_Mem_Write(&hi2c1, bme280_address, BME280_REG_RESET, 1, &cfg, 1, 100);
    HAL_Delay(100);

    /* ✅ READ CALIBRATION - this was missing! */
    BME280_ReadCalibration();

    /* Configure */
    cfg = 0x01;
    HAL_I2C_Mem_Write(&hi2c1, bme280_address, BME280_REG_CTRL_HUM, 1, &cfg, 1, 100);

    cfg = 0x27;
    HAL_I2C_Mem_Write(&hi2c1, bme280_address, BME280_REG_CTRL_MEAS, 1, &cfg, 1, 100);

    HAL_Delay(100);
}

float BME280_ReadTemperature(void)
{
    uint8_t data[3];
    int32_t adc_T, var1, var2;

    if(bme280_address == 0) return 0.0f;

    if(HAL_I2C_Mem_Read(&hi2c1, bme280_address, 0xFA, 1, data, 3, 100) != HAL_OK)
        return 0.0f;

    adc_T = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);

    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;

    return (float)((t_fine * 5 + 128) >> 8) / 100.0f;
}

float BME280_ReadPressure(void)
{
    uint8_t data[3];
    int32_t adc_P;
    int64_t var1, var2, p;

    if(bme280_address == 0) return 0.0f;

    if(HAL_I2C_Mem_Read(&hi2c1, bme280_address, 0xF7, 1, data, 3, 100) != HAL_OK)
        return 0.0f;

    adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + ((int64_t)dig_P4 << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)dig_P1) >> 33;

    if(var1 == 0) return 0.0f;

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)dig_P7 << 4);

    return (float)p / 25600.0f;   /* ✅ hPa (was Pa before) */
}
