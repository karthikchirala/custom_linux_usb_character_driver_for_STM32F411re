/* bme280.h */
#ifndef BME280_H_
#define BME280_H_

#include "stm32f4xx_hal.h"
extern uint8_t bme280_address;

void BME280_Init(void);
float BME280_ReadTemperature(void);
float BME280_ReadPressure(void);
void BME280_Debug(void);

#endif /* BME280_H_ */
