/* usbd_vendor.c */

#include "usbd_vendor.h"
#include "usbd_vendor_if.h"
#include "usbd_ctlreq.h"

/*─────────────────────────────────────
  Forward Declarations
  ─────────────────────────────────────*/

static uint8_t USBD_VENDOR_Init(
               USBD_HandleTypeDef *pdev,
               uint8_t cfgidx);

static uint8_t USBD_VENDOR_DeInit(
               USBD_HandleTypeDef *pdev,
               uint8_t cfgidx);

static uint8_t USBD_VENDOR_Setup(
               USBD_HandleTypeDef *pdev,
               USBD_SetupReqTypedef *req);

static uint8_t USBD_VENDOR_DataIn(
               USBD_HandleTypeDef *pdev,
               uint8_t epnum);

static uint8_t USBD_VENDOR_DataOut(
               USBD_HandleTypeDef *pdev,
               uint8_t epnum);

static uint8_t *USBD_VENDOR_GetCfgDesc(
                uint16_t *length);

/*─────────────────────────────────────
  Configuration Descriptor

  This tells Linux about our device:
  - It is a Vendor Specific device
  - It has 2 Bulk Endpoints
  - EP1 OUT for receiving commands
  - EP1 IN  for sending ACK
  ─────────────────────────────────────*/

static uint8_t USBD_VENDOR_CfgDesc[
               USB_VENDOR_CONFIG_DESC_SIZE] =
{
    /*───────────────────────────────
      Configuration Descriptor
      9 bytes
      ───────────────────────────────*/
    0x09,   /* bLength */
    0x02,   /* bDescriptorType = Configuration */
    0x20,   /* wTotalLength = 32 bytes low  */
    0x00,   /* wTotalLength = 32 bytes high */
    0x01,   /* bNumInterfaces = 1 */
    0x01,   /* bConfigurationValue = 1 */
    0x00,   /* iConfiguration = no string */
    0xC0,   /* bmAttributes = Self Powered */
    0x32,   /* bMaxPower = 100mA */

    /*───────────────────────────────
      Interface Descriptor
      9 bytes
      ───────────────────────────────*/
    0x09,   /* bLength */
    0x04,   /* bDescriptorType = Interface */
    0x00,   /* bInterfaceNumber = 0 */
    0x00,   /* bAlternateSetting = 0 */
    0x02,   /* bNumEndpoints = 2 */
    0xFF,   /* bInterfaceClass = Vendor Specific ✅ */
    0x00,   /* bInterfaceSubClass = 0 */
    0x00,   /* bInterfaceProtocol = 0 */
    0x00,   /* iInterface = no string */

    /*───────────────────────────────
      Endpoint OUT Descriptor
      Linux to STM32
      7 bytes
      ───────────────────────────────*/
    0x07,              /* bLength */
    0x05,              /* bDescriptorType = Endpoint */
    VENDOR_EPOUT_ADDR, /* bEndpointAddress = 0x01 */
    0x02,              /* bmAttributes = Bulk */
    VENDOR_EPOUT_SIZE, /* wMaxPacketSize low */
    0x00,              /* wMaxPacketSize high */
    0x00,              /* bInterval */

    /*───────────────────────────────
      Endpoint IN Descriptor
      STM32 to Linux
      7 bytes
      ───────────────────────────────*/
    0x07,             /* bLength */
    0x05,             /* bDescriptorType = Endpoint */
    VENDOR_EPIN_ADDR, /* bEndpointAddress = 0x81 */
    0x02,             /* bmAttributes = Bulk */
    VENDOR_EPIN_SIZE, /* wMaxPacketSize low */
    0x00,             /* wMaxPacketSize high */
    0x00,             /* bInterval */
};

/*─────────────────────────────────────
  Class Structure

  USB Core uses this structure to
  call the correct functions when
  USB events occur
  ─────────────────────────────────────*/

USBD_ClassTypeDef USBD_VENDOR =
{
    USBD_VENDOR_Init,
    USBD_VENDOR_DeInit,
    USBD_VENDOR_Setup,
    NULL,
    NULL,
    USBD_VENDOR_DataIn,
    USBD_VENDOR_DataOut,
    NULL,
    NULL,
    NULL,
    USBD_VENDOR_GetCfgDesc,
    USBD_VENDOR_GetCfgDesc,
    USBD_VENDOR_GetCfgDesc,
    NULL,
};

/*─────────────────────────────────────
  Init Function

  Called when USB cable is connected

  What it does:
  1. Allocates memory
  2. Opens EP1 IN endpoint
  3. Opens EP1 OUT endpoint
  4. Prepares to receive data
  ─────────────────────────────────────*/

static uint8_t USBD_VENDOR_Init(
               USBD_HandleTypeDef *pdev,
               uint8_t cfgidx)
{
    USBD_VENDOR_HandleTypeDef *hvendor;

    /* Allocate memory */
    hvendor = (USBD_VENDOR_HandleTypeDef*)
               USBD_malloc(
               sizeof(USBD_VENDOR_HandleTypeDef));

    if(hvendor == NULL)
        return USBD_FAIL;

    /* Clear memory */
    memset(hvendor, 0,
           sizeof(USBD_VENDOR_HandleTypeDef));

    /* Store in pdev */
    pdev->pClassData = hvendor;

    /* Open EP1 IN (STM32 to Linux) */
    USBD_LL_OpenEP(pdev,
                   VENDOR_EPIN_ADDR,
                   USBD_EP_TYPE_BULK,
                   VENDOR_EPIN_SIZE);

    /* Open EP1 OUT (Linux to STM32) */
    USBD_LL_OpenEP(pdev,
                   VENDOR_EPOUT_ADDR,
                   USBD_EP_TYPE_BULK,
                   VENDOR_EPOUT_SIZE);

    /* Prepare to receive first packet */
    USBD_LL_PrepareReceive(pdev,
                           VENDOR_EPOUT_ADDR,
                           hvendor->rx_buffer,
                           VENDOR_EPOUT_SIZE);

    return USBD_OK;
}

/*─────────────────────────────────────
  DeInit Function

  Called when USB cable is disconnected
  ─────────────────────────────────────*/

static uint8_t USBD_VENDOR_DeInit(
               USBD_HandleTypeDef *pdev,
               uint8_t cfgidx)
{
    /* Close endpoints */
    USBD_LL_CloseEP(pdev, VENDOR_EPIN_ADDR);
    USBD_LL_CloseEP(pdev, VENDOR_EPOUT_ADDR);

    /* Free memory */
    if(pdev->pClassData != NULL)
    {
        USBD_free(pdev->pClassData);
        pdev->pClassData = NULL;
    }

    return USBD_OK;
}

/*─────────────────────────────────────
  Setup Function

  Called for Control Requests
  We have no special control requests
  so this is minimal
  ─────────────────────────────────────*/

static uint8_t USBD_VENDOR_Setup(
               USBD_HandleTypeDef *pdev,
               USBD_SetupReqTypedef *req)
{
    return USBD_OK;
}

/*─────────────────────────────────────
  DataOut Function

  Called when Linux sends data to STM32

  What it does:
  1. Gets how many bytes were received
  2. Calls application callback
  3. Prepares to receive next packet
  ─────────────────────────────────────*/

static uint8_t USBD_VENDOR_DataOut(
               USBD_HandleTypeDef *pdev,
               uint8_t epnum)
{
    USBD_VENDOR_HandleTypeDef *hvendor;

    hvendor = (USBD_VENDOR_HandleTypeDef*)
               pdev->pClassData;

    /* Get number of bytes received */
    hvendor->rx_length =
    USBD_LL_GetRxDataSize(pdev, epnum);

    /* Call application callback
       LED control happens here */
    VENDOR_IF_Receive(hvendor->rx_buffer,
                      hvendor->rx_length);

    /* Prepare to receive next packet */
    USBD_LL_PrepareReceive(pdev,
                           VENDOR_EPOUT_ADDR,
                           hvendor->rx_buffer,
                           VENDOR_EPOUT_SIZE);

    return USBD_OK;
}

/*─────────────────────────────────────
  DataIn Function

  Called after STM32 successfully
  sends data to Linux

  Clears tx_busy flag so next
  transmission can happen
  ─────────────────────────────────────*/

static uint8_t USBD_VENDOR_DataIn(
               USBD_HandleTypeDef *pdev,
               uint8_t epnum)
{
    USBD_VENDOR_HandleTypeDef *hvendor;

    hvendor = (USBD_VENDOR_HandleTypeDef*)
               pdev->pClassData;

    /* Transmission complete
       Ready to send next data */
    hvendor->tx_busy = 0;

    return USBD_OK;
}

/*─────────────────────────────────────
  Transmit Function

  Application calls this to send
  data back to Linux

  We use this to send ACK (0xAA)
  ─────────────────────────────────────*/

uint8_t USBD_VENDOR_Transmit(
        USBD_HandleTypeDef *pdev,
        uint8_t *buf,
        uint16_t len)
{
    USBD_VENDOR_HandleTypeDef *hvendor;

    hvendor = (USBD_VENDOR_HandleTypeDef*)
               pdev->pClassData;

    /* Check if previous send is complete */
    if(hvendor->tx_busy == 1)
        return USBD_BUSY;

    /* Mark as busy */
    hvendor->tx_busy = 1;

    /* Copy data to tx buffer */
    memcpy(hvendor->tx_buffer, buf, len);

    /* Send via USB */
    USBD_LL_Transmit(pdev,
                     VENDOR_EPIN_ADDR,
                     hvendor->tx_buffer,
                     len);

    return USBD_OK;
}

/*─────────────────────────────────────
  GetCfgDesc Function

  Returns Configuration Descriptor
  when Linux requests it
  ─────────────────────────────────────*/

static uint8_t *USBD_VENDOR_GetCfgDesc(
                uint16_t *length)
{
    *length = sizeof(USBD_VENDOR_CfgDesc);

    return USBD_VENDOR_CfgDesc;
}
