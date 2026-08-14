#ifndef __USBD_VENDOR_IF_H
#define __USBD_VENDOR_IF_H

#include "usbd_vendor.h"
#include <string.h>

/* Commands */
#define CMD_LED_ON        0x01
#define CMD_LED_OFF       0x02
#define CMD_READ_SENSOR   0x10

/* Response Status */
#define RESP_AUTHORIZED   0xA0
#define RESP_DENIED       0xA1
#define RESP_TIMEOUT      0xA2

#define ACK_SUCCESS       0xAA

typedef struct __attribute__((packed))
{
    uint8_t status;
    float   temperature;
    float   pressure;
    uint8_t uid[4];
} Sensor_Response_t;

extern volatile uint8_t sensor_request_pending;
void handle_read_sensor_request(void);

#endif
