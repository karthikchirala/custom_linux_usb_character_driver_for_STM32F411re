/* usbd_vendor.h */

#ifndef __USBD_VENDOR_H
#define __USBD_VENDOR_H

#include "usbd_ioreq.h"

/*─────────────────────────────────────
  Endpoint Addresses

  0x81 means:
  0x80 = IN direction (STM32 to Linux)
  0x01 = Endpoint number 1

  0x01 means:
  OUT direction (Linux to STM32)
  Endpoint number 1
  ─────────────────────────────────────*/

#define VENDOR_EPIN_ADDR         0x81
#define VENDOR_EPIN_SIZE         0x40

#define VENDOR_EPOUT_ADDR        0x01
#define VENDOR_EPOUT_SIZE        0x40

/*─────────────────────────────────────
  Configuration Descriptor Total Size

  9  bytes = Configuration Descriptor
  9  bytes = Interface Descriptor
  7  bytes = Endpoint OUT Descriptor
  7  bytes = Endpoint IN Descriptor
  Total    = 32 bytes
  ─────────────────────────────────────*/

#define USB_VENDOR_CONFIG_DESC_SIZE    32

/*─────────────────────────────────────
  Handle Structure

  This structure holds all the
  data buffers and state variables
  used during USB communication
  ─────────────────────────────────────*/

typedef struct
{
    /* Data received from Linux stored here */
    uint8_t  rx_buffer[64];

    /* Data to be sent to Linux stored here */
    uint8_t  tx_buffer[64];

    /* How many bytes were received */
    uint32_t rx_length;

    /* Is transmit currently busy */
    uint8_t  tx_busy;

} USBD_VENDOR_HandleTypeDef;

/*─────────────────────────────────────
  Class Structure Declaration
  ─────────────────────────────────────*/

extern USBD_ClassTypeDef USBD_VENDOR;

/*─────────────────────────────────────
  Transmit Function Declaration

  Application calls this function
  to send data back to Linux
  ─────────────────────────────────────*/

uint8_t USBD_VENDOR_Transmit(
        USBD_HandleTypeDef *pdev,
        uint8_t *buf,
        uint16_t len);

#endif /* __USBD_VENDOR_H */
