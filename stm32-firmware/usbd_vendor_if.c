/* usbd_vendor_if.c -  */
#include "usbd_vendor_if.h"
#include "usb_device.h"
#include "main.h"
#include "MFRC522_STM32.h"
#include "bme280.h"

extern USBD_HandleTypeDef hUsbDeviceFS;
extern MFRC522_t mfrc522;

volatile uint8_t sensor_request_pending = 0;

/* RGB LED Control */
#define RGB_RED_ON()     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET)
#define RGB_RED_OFF()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET)
#define RGB_GREEN_ON()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET)
#define RGB_GREEN_OFF()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET)
#define RGB_BLUE_ON()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET)
#define RGB_BLUE_OFF()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET)

#define RGB_OFF()        do { RGB_RED_OFF(); RGB_GREEN_OFF(); RGB_BLUE_OFF(); } while(0)
#define RGB_WAITING()    do { RGB_RED_OFF(); RGB_GREEN_OFF(); RGB_BLUE_ON();  } while(0)
#define RGB_SUCCESS()    do { RGB_RED_OFF(); RGB_GREEN_ON(); RGB_BLUE_OFF(); } while(0)
#define RGB_FAIL()       do { RGB_RED_ON();  RGB_GREEN_OFF(); RGB_BLUE_OFF();} while(0)

/* Authorized Cards - Update with your actual card UIDs */
typedef struct {
    uint8_t uid[4];
    uint8_t active;
} AuthUser_t;

AuthUser_t authorized_users[] = {
    {{0x63, 0xB5, 0x3C, 0xDA}, 1},
    {{0x73, 0x95, 0x43, 0xDA}, 1},
    {{0xAC, 0x14, 0x2B, 0x02}, 1},
    {{0x00, 0x00, 0x00, 0x00}, 0}
};

uint8_t is_authorized(uint8_t *uid)
{
    for(int i = 0; authorized_users[i].active; i++)
    {
        if(memcmp(authorized_users[i].uid, uid, 4) == 0)
            return 1;
    }
    return 0;
}

/* RFID Card Reading with 10 second timeout */
uint8_t wait_for_rfid_card(uint8_t *uid)
{
    uint8_t atqa[2];
    uint32_t start = HAL_GetTick();

    RGB_WAITING();                    // Blue LED = Waiting for card

    while((HAL_GetTick() - start) < 10000)   // 10 seconds timeout
    {
        if(MFRC522_RequestA(&mfrc522, atqa) == STATUS_OK)
        {
            if(MFRC522_ReadUid(&mfrc522, uid) == STATUS_OK)
            {
                return 1;   // Card successfully read
            }
        }
        HAL_Delay(100);
    }
    return 0;   // Timeout
}

/* Main Logic */
void handle_read_sensor_request(void)
{
    Sensor_Response_t response;
    uint8_t uid[4] = {0};

    if(wait_for_rfid_card(uid))
    {
        if(is_authorized(uid))
        {
            /* ==================== CARD GRANTED ==================== */
            RGB_SUCCESS();

            response.status      = RESP_AUTHORIZED;
            response.temperature = BME280_ReadTemperature();
            response.pressure    = BME280_ReadPressure();
            memcpy(response.uid, uid, 4);
        }
        else
        {
            /* ==================== CARD DENIED ==================== */
            RGB_FAIL();

            response.status = RESP_DENIED;
            response.temperature = 0.0f;
            response.pressure = 0.0f;
            memcpy(response.uid, uid, 4);
        }
    }
    else
    {
        /* ==================== TIMEOUT ==================== */
        RGB_FAIL();

        response.status = RESP_TIMEOUT;
        response.temperature = 0.0f;
        response.pressure = 0.0f;
        memset(response.uid, 0, 4);
    }

    USBD_VENDOR_Transmit(&hUsbDeviceFS, (uint8_t*)&response, sizeof(Sensor_Response_t));

    HAL_Delay(2000);
    RGB_OFF();
}

/* USB Receive Callback */
void VENDOR_IF_Receive(uint8_t *buf, uint32_t len)
{
    uint8_t ack = ACK_SUCCESS;

    if(len == 0) return;

    switch(buf[0])
    {
        case CMD_LED_ON:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
            USBD_VENDOR_Transmit(&hUsbDeviceFS, &ack, 1);
            break;

        case CMD_LED_OFF:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            USBD_VENDOR_Transmit(&hUsbDeviceFS, &ack, 1);
            break;

        case CMD_READ_SENSOR:
            sensor_request_pending = 1;     // Handled in main loop
            break;

        default:
            break;
    }
}
