/**
  ******************************************************************************
  * @file    usbd_cdc.c
  * @author  MCD Application Team
  * @brief   This file provides the high layer firmware functions to manage the
  *          following functionalities of the USB CDC Class:
  *           - Initialization and Configuration of high and low layer
  *           - Enumeration as CDC Device (and enumeration for each implemented memory interface)
  *           - OUT/IN data transfer
  *           - Command IN transfer (class requests management)
  *           - Error management
  *
  *  @verbatim
  *
  *          ===================================================================
  *                                CDC Class Driver Description
  *          ===================================================================
  *           This driver manages the "Universal Serial Bus Class Definitions for Communications Devices
  *           Revision 1.2 November 16, 2007" and the sub-protocol specification of "Universal Serial Bus
  *           Communications Class Subclass Specification for PSTN Devices Revision 1.2 February 9, 2007"
  *           This driver implements the following aspects of the specification:
  *             - Device descriptor management
  *             - Configuration descriptor management
  *             - Enumeration as CDC device with 2 data endpoints (IN and OUT) and 1 command endpoint (IN)
  *             - Requests management (as described in section 6.2 in specification)
  *             - Abstract Control Model compliant
  *             - Union Functional collection (using 1 IN endpoint for control)
  *             - Data interface class
  *
  *           These aspects may be enriched or modified for a specific user application.
  *
  *            This driver doesn't implement the following aspects of the specification
  *            (but it is possible to manage these features with some modifications on this driver):
  *             - Any class-specific aspect relative to communication classes should be managed by user application.
  *             - All communication classes other than PSTN are not managed
  *
  *  @endverbatim
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                      www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* BSPDependencies
- "stm32xxxxx_{eval}{discovery}{nucleo_144}.c"
- "stm32xxxxx_{eval}{discovery}_io.c"
EndBSPDependencies */

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"


/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_CDC
  * @brief usbd core module
  * @{
  */

/** @defgroup USBD_CDC_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */


/** @defgroup USBD_CDC_Private_Defines
  * @{
  */
/**
  * @}
  */


/** @defgroup USBD_CDC_Private_Macros
  * @{
  */

/**
  * @}
  */


/** @defgroup USBD_CDC_Private_FunctionPrototypes
  * @{
  */

static uint8_t USBD_CDC_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_CDC_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_CDC_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_CDC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CDC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CDC_EP0_RxReady(USBD_HandleTypeDef *pdev);

static uint8_t *USBD_CDC_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_GetOtherSpeedCfgDesc(uint16_t *length);
uint8_t *USBD_CDC_GetDeviceQualifierDescriptor(uint16_t *length);
static uint8_t USBD_WinUSBComm_SetupVendor(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_CDC_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

/**
  * @}
  */

/** @defgroup USBD_CDC_Private_Variables
  * @{
  */


/* CDC interface class callbacks structure */
USBD_ClassTypeDef  USBD_CDC =
{
  USBD_CDC_Init,
  USBD_CDC_DeInit,
  USBD_CDC_Setup,
  NULL,                 /* EP0_TxSent, */
  USBD_CDC_EP0_RxReady,
  USBD_CDC_DataIn,
  USBD_CDC_DataOut,
  NULL,
  NULL,
  NULL,
  USBD_CDC_GetHSCfgDesc,
  USBD_CDC_GetFSCfgDesc,
  USBD_CDC_GetOtherSpeedCfgDesc,
  USBD_CDC_GetDeviceQualifierDescriptor,
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
  USBD_UsrStrDescriptor,
#endif
};

/* USB CDC device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_CDC_CfgDesc[USB_CDC_CONFIG_DESC_SIZ] __ALIGN_END =
{
  /* Configuration Descriptor */
  0x09,                                       /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,                /* bDescriptorType: Configuration */
  USB_CDC_CONFIG_DESC_SIZ,                    /* wTotalLength */
  0x00,
  0x03,                                       /* bNumInterfaces: 3 interfaces (2 CDC + 1 vendor) */
  0x01,                                       /* bConfigurationValue */
  0x00,                                       /* iConfiguration */
#if (USBD_SELF_POWERED == 1U)
  0xC0,                                       /* bmAttributes: Self powered */
#else
  0x80,                                       /* bmAttributes: Bus powered */
#endif
  USBD_MAX_POWER,                             /* MaxPower */

  /* IAD: CDC virtual COM (IF0, IF1) */
  0x08,                                       /* bLength */
  USB_DESC_TYPE_IAD,                          /* bDescriptorType */
  0x00,                                       /* bFirstInterface */
  0x02,                                       /* bInterfaceCount */
  0x02,                                       /* bFunctionClass: CDC */
  0x02,                                       /* bFunctionSubClass: ACM */
  0x01,                                       /* bFunctionProtocol */
  USBD_IDX_INTERFACE_STR,                     /* iFunction: CDC display name */

  /* CDC Control Interface (IF0) */
  0x09,                                       /* bLength */
  USB_DESC_TYPE_INTERFACE,                    /* bDescriptorType */
  0x00,                                       /* bInterfaceNumber */
  0x00,                                       /* bAlternateSetting */
  0x01,                                       /* bNumEndpoints */
  0x02,                                       /* bInterfaceClass: Communication */
  0x02,                                       /* bInterfaceSubClass: ACM */
  0x01,                                       /* bInterfaceProtocol */
  USBD_IDX_INTERFACE_STR,                     /* iInterface: CDC display name */

  /* Header Functional Descriptor */
  0x05,                                       /* bFunctionLength */
  0x24,                                       /* bDescriptorType: CS_INTERFACE */
  0x00,                                       /* bDescriptorSubtype: Header */
  0x10, 0x01,                                 /* bcdCDC */

  /* Call Management Functional Descriptor */
  0x05,                                       /* bFunctionLength */
  0x24,                                       /* bDescriptorType: CS_INTERFACE */
  0x01,                                       /* bDescriptorSubtype: Call Management */
  0x00,                                       /* bmCapabilities */
  0x01,                                       /* bDataInterface */

  /* ACM Functional Descriptor */
  0x04,                                       /* bFunctionLength */
  0x24,                                       /* bDescriptorType: CS_INTERFACE */
  0x02,                                       /* bDescriptorSubtype: ACM */
  0x02,                                       /* bmCapabilities */

  /* Union Functional Descriptor */
  0x05,                                       /* bFunctionLength */
  0x24,                                       /* bDescriptorType: CS_INTERFACE */
  0x06,                                       /* bDescriptorSubtype: Union */
  0x00,                                       /* bMasterInterface */
  0x01,                                       /* bSlaveInterface0 */

  /* Notification Endpoint (EP2 IN) */
  0x07,                                       /* bLength */
  USB_DESC_TYPE_ENDPOINT,                     /* bDescriptorType */
  CDC_CMD_EP,                                 /* bEndpointAddress */
  0x03,                                       /* bmAttributes: Interrupt */
  LOBYTE(CDC_CMD_PACKET_SIZE), HIBYTE(CDC_CMD_PACKET_SIZE),
  CDC_FS_BINTERVAL,                           /* bInterval */

  /* CDC Data Interface (IF1) */
  0x09,                                       /* bLength */
  USB_DESC_TYPE_INTERFACE,                    /* bDescriptorType */
  0x01,                                       /* bInterfaceNumber */
  0x00,                                       /* bAlternateSetting */
  0x02,                                       /* bNumEndpoints */
  0x0A,                                       /* bInterfaceClass: CDC Data */
  0x00, 0x00,                                 /* bInterfaceSubClass/Protocol */
  USBD_IDX_INTERFACE_STR,                     /* iInterface: CDC display name */

  /* CDC OUT Endpoint (EP1 OUT) */
  0x07,                                       /* bLength */
  USB_DESC_TYPE_ENDPOINT,                     /* bDescriptorType */
  CDC_OUT_EP,                                 /* bEndpointAddress */
  0x02,                                       /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE), HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
  0x00,                                       /* bInterval */

  /* CDC IN Endpoint (EP1 IN) */
  0x07,                                       /* bLength */
  USB_DESC_TYPE_ENDPOINT,                     /* bDescriptorType */
  CDC_IN_EP,                                  /* bEndpointAddress */
  0x02,                                       /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE), HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
  0x00,                                       /* bInterval */

  /* IAD: Vendor JSON interface (IF2) */
  0x08,                                       /* bLength */
  USB_DESC_TYPE_IAD,                          /* bDescriptorType */
  0x02,                                       /* bFirstInterface */
  0x01,                                       /* bInterfaceCount */
  0x00,                                       /* bFunctionClass */
  0x00,                                       /* bFunctionSubClass */
  0x00,                                       /* bFunctionProtocol */
  USBD_IDX_REF_INTF_STR,                      /* iFunction: Native display name */

  /* Vendor Data Interface (IF2) */
  0x09,                                       /* bLength */
  USB_DESC_TYPE_INTERFACE,                    /* bDescriptorType */
  0x02,                                       /* bInterfaceNumber */
  0x00,                                       /* bAlternateSetting */
  0x02,                                       /* bNumEndpoints */
  0x00,                                       /* bInterfaceClass: vendor/custom */
  0x01,                                       /* bInterfaceSubClass */
  0x00,                                       /* bInterfaceProtocol */
  USBD_IDX_REF_INTF_STR,                      /* iInterface: Native display name */

  /* Vendor OUT Endpoint (EP3 OUT) */
  0x07,                                       /* bLength */
  USB_DESC_TYPE_ENDPOINT,                     /* bDescriptorType */
  ODRIVE_OUT_EP,                              /* bEndpointAddress */
  0x02,                                       /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE), HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
  0x00,                                       /* bInterval */

  /* Vendor IN Endpoint (EP3 IN) */
  0x07,                                       /* bLength */
  USB_DESC_TYPE_ENDPOINT,                     /* bDescriptorType */
  ODRIVE_IN_EP,                               /* bEndpointAddress */
  0x02,                                       /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE), HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
  0x00                                        /* bInterval */
};

/**
  * @}
  */

/** @defgroup USBD_CDC_Private_Functions
  * @{
  */

/**
  * @brief  USBD_CDC_Init
  *         Initialize the CDC interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_CDC_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);
  USBD_CDC_HandleTypeDef *hcdc;

  hcdc = USBD_malloc(sizeof(USBD_CDC_HandleTypeDef));

  if (hcdc == NULL)
  {
    pdev->pClassData = NULL;
    return (uint8_t)USBD_EMEM;
  }

  pdev->pClassData = (void *)hcdc;

  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    /* Open EP IN */
    (void)USBD_LL_OpenEP(pdev, CDC_IN_EP, USBD_EP_TYPE_BULK,
                         CDC_DATA_HS_IN_PACKET_SIZE);

    pdev->ep_in[CDC_IN_EP & 0xFU].is_used = 1U;

    /* Open EP OUT */
    (void)USBD_LL_OpenEP(pdev, CDC_OUT_EP, USBD_EP_TYPE_BULK,
                         CDC_DATA_HS_OUT_PACKET_SIZE);

    pdev->ep_out[CDC_OUT_EP & 0xFU].is_used = 1U;

    /* Open ODRIVE EP IN */
    (void)USBD_LL_OpenEP(pdev, ODRIVE_IN_EP, USBD_EP_TYPE_BULK,
                         CDC_DATA_HS_IN_PACKET_SIZE);

    pdev->ep_in[ODRIVE_IN_EP & 0xFU].is_used = 1U;

    /* Open ODRIVE EP OUT */
    (void)USBD_LL_OpenEP(pdev, ODRIVE_OUT_EP, USBD_EP_TYPE_BULK,
                         CDC_DATA_HS_OUT_PACKET_SIZE);

    pdev->ep_out[ODRIVE_OUT_EP & 0xFU].is_used = 1U;

    /* Set bInterval for CDC CMD Endpoint */
    pdev->ep_in[CDC_CMD_EP & 0xFU].bInterval = CDC_HS_BINTERVAL;
  }
  else
  {
    /* Open EP IN */
    (void)USBD_LL_OpenEP(pdev, CDC_IN_EP, USBD_EP_TYPE_BULK,
                         CDC_DATA_FS_IN_PACKET_SIZE);

    pdev->ep_in[CDC_IN_EP & 0xFU].is_used = 1U;

    /* Open EP OUT */
    (void)USBD_LL_OpenEP(pdev, CDC_OUT_EP, USBD_EP_TYPE_BULK,
                         CDC_DATA_FS_OUT_PACKET_SIZE);

    pdev->ep_out[CDC_OUT_EP & 0xFU].is_used = 1U;

    /* Open ODRIVE EP IN */
    (void)USBD_LL_OpenEP(pdev, ODRIVE_IN_EP, USBD_EP_TYPE_BULK,
                         CDC_DATA_FS_IN_PACKET_SIZE);

    pdev->ep_in[ODRIVE_IN_EP & 0xFU].is_used = 1U;

    /* Open ODRIVE EP OUT */
    (void)USBD_LL_OpenEP(pdev, ODRIVE_OUT_EP, USBD_EP_TYPE_BULK,
                         CDC_DATA_FS_OUT_PACKET_SIZE);

    pdev->ep_out[ODRIVE_OUT_EP & 0xFU].is_used = 1U;

    /* Set bInterval for CMD Endpoint */
    pdev->ep_in[CDC_CMD_EP & 0xFU].bInterval = CDC_FS_BINTERVAL;
  }

  /* Open Command IN EP */
  (void)USBD_LL_OpenEP(pdev, CDC_CMD_EP, USBD_EP_TYPE_INTR, CDC_CMD_PACKET_SIZE);
  pdev->ep_in[CDC_CMD_EP & 0xFU].is_used = 1U;

  /* Init  physical Interface components */
  ((USBD_CDC_ItfTypeDef *)pdev->pUserData)->Init();

  /* Init Xfer states */
  hcdc->TxState = 0U;
  hcdc->RxState = 0U;
  hcdc->CDC_Tx.State = 0U;
  hcdc->CDC_Rx.State = 0U;
  hcdc->REF_Tx.State = 0U;
  hcdc->ODRIVE_Rx.State = 0U;

  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    /* Prepare Out endpoint to receive next packet */
    (void)USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, hcdc->CDC_Rx.Buffer,
                                 CDC_DATA_HS_OUT_PACKET_SIZE);

    /* Prepare ODRIVE Out endpoint to receive next packet */
    (void)USBD_LL_PrepareReceive(pdev, ODRIVE_OUT_EP, hcdc->ODRIVE_Rx.Buffer,
                                 CDC_DATA_HS_OUT_PACKET_SIZE);
  }
  else
  {
    /* Prepare Out endpoint to receive next packet */
    (void)USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, hcdc->CDC_Rx.Buffer,
                                 CDC_DATA_FS_OUT_PACKET_SIZE);

    /* Prepare ODRIVE Out endpoint to receive next packet */
    (void)USBD_LL_PrepareReceive(pdev, ODRIVE_OUT_EP, hcdc->ODRIVE_Rx.Buffer,
                                 CDC_DATA_FS_OUT_PACKET_SIZE);
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_Init
  *         DeInitialize the CDC layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_CDC_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  /* Close EP IN */
  (void)USBD_LL_CloseEP(pdev, CDC_IN_EP);
  pdev->ep_in[CDC_IN_EP & 0xFU].is_used = 0U;

  /* Close EP OUT */
  (void)USBD_LL_CloseEP(pdev, CDC_OUT_EP);
  pdev->ep_out[CDC_OUT_EP & 0xFU].is_used = 0U;

  /* Close Command IN EP */
  (void)USBD_LL_CloseEP(pdev, CDC_CMD_EP);
  pdev->ep_in[CDC_CMD_EP & 0xFU].is_used = 0U;
  pdev->ep_in[CDC_CMD_EP & 0xFU].bInterval = 0U;

  /* Close ODRIVE IN EP */
  (void)USBD_LL_CloseEP(pdev, ODRIVE_IN_EP);
  pdev->ep_in[ODRIVE_IN_EP & 0xFU].is_used = 0U;

  /* Close ODRIVE OUT EP */
  (void)USBD_LL_CloseEP(pdev, ODRIVE_OUT_EP);
  pdev->ep_out[ODRIVE_OUT_EP & 0xFU].is_used = 0U;

  /* DeInit  physical Interface components */
  if (pdev->pClassData != NULL)
  {
    ((USBD_CDC_ItfTypeDef *)pdev->pUserData)->DeInit();
    (void)USBD_free(pdev->pClassData);
    pdev->pClassData = NULL;
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_Setup
  *         Handle the CDC specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t USBD_CDC_Setup(USBD_HandleTypeDef *pdev,
                              USBD_SetupReqTypedef *req)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
  uint16_t len;
  uint8_t ifalt = 0U;
  uint16_t status_info = 0U;
  USBD_StatusTypeDef ret = USBD_OK;

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS:
      if (hcdc == NULL)
      {
        ret = USBD_FAIL;
        break;
      }

      if (req->wLength != 0U)
      {
        if ((req->bmRequest & 0x80U) != 0U)
        {
          ((USBD_CDC_ItfTypeDef *)pdev->pUserData)->Control(req->bRequest,
                                                            (uint8_t *)hcdc->data,
                                                            req->wLength);

          len = MIN(CDC_REQ_MAX_DATA_SIZE, req->wLength);
          (void)USBD_CtlSendData(pdev, (uint8_t *)hcdc->data, len);
        }
        else
        {
          hcdc->CmdOpCode = req->bRequest;
          hcdc->CmdLength = (uint8_t)req->wLength;

          (void)USBD_CtlPrepareRx(pdev, (uint8_t *)hcdc->data, req->wLength);
        }
      }
      else
      {
        ((USBD_CDC_ItfTypeDef *)pdev->pUserData)->Control(req->bRequest,
                                                          (uint8_t *)req, 0U);
      }
      break;

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, &ifalt, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if (pdev->dev_state != USBD_STATE_CONFIGURED)
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    case USB_REQ_TYPE_VENDOR:
      ret = (USBD_StatusTypeDef)USBD_WinUSBComm_SetupVendor(pdev, req);
      break;

    default:
      USBD_CtlError(pdev, req);
      ret = USBD_FAIL;
      break;
  }

  return (uint8_t)ret;
}

/**
  * @brief  USBD_CDC_DataIn
  *         Data sent on non-control IN endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t USBD_CDC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_CDC_HandleTypeDef *hcdc;
  PCD_HandleTypeDef *hpcd = pdev->pData;

  if (pdev->pClassData == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;

  if ((pdev->ep_in[epnum].total_length > 0U) &&
      ((pdev->ep_in[epnum].total_length % hpcd->IN_ep[epnum].maxpacket) == 0U))
  {
    /* Update the packet total length */
    pdev->ep_in[epnum].total_length = 0U;

    /* Send ZLP */
    (void)USBD_LL_Transmit(pdev, epnum, NULL, 0U);
  }
  else
  {
    hcdc->TxState = 0U;

    if (((USBD_CDC_ItfTypeDef *)pdev->pUserData)->TransmitCplt != NULL)
    {
      ((USBD_CDC_ItfTypeDef *)pdev->pUserData)->TransmitCplt(hcdc->TxBuffer, &hcdc->TxLength, epnum);
    }
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
// static uint8_t USBD_CDC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
// {
//   USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
//
//   if (pdev->pClassData == NULL)
//   {
//     return (uint8_t)USBD_FAIL;
//   }
//
//   /* Get the received data length */
//   hcdc->RxLength = USBD_LL_GetRxDataSize(pdev, epnum);
//
//   /* USB data will be immediately processed, this allow next USB traffic being
//   NAKed till the end of the application Xfer */
//
//   ((USBD_CDC_ItfTypeDef *)pdev->pUserData)->Receive(hcdc->RxBuffer, &hcdc->RxLength);
//
//   return (uint8_t)USBD_OK;
// }
static uint8_t  USBD_CDC_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_CDC_HandleTypeDef   *hcdc = (USBD_CDC_HandleTypeDef*) pdev->pClassData;

  USBD_CDC_EP_HandleTypeDef* hEP_Rx;
  if (epnum == CDC_OUT_EP) {
    hEP_Rx = &hcdc->CDC_Rx;
  } else if (epnum == ODRIVE_OUT_EP) {
    hEP_Rx = &hcdc->ODRIVE_Rx;
  } else {
    return USBD_FAIL;
  }

  /* Get the received data length */
  hEP_Rx->Length = USBD_LL_GetRxDataSize (pdev, epnum);

  /* USB data will be immediately processed, this allow next USB traffic being
  NAKed till the end of the application Xfer */
  if(pdev->pClassData != NULL)
  {
    ((USBD_CDC_ItfTypeDef *)pdev->pUserData)->Receive(hEP_Rx->Buffer, &hEP_Rx->Length, epnum);

    return USBD_OK;
  }
  else
  {
    return USBD_FAIL;
  }
}

/**
  * @brief  USBD_CDC_EP0_RxReady
  *         Handle EP0 Rx Ready event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_CDC_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;

  if (hcdc == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if ((pdev->pUserData != NULL) && (hcdc->CmdOpCode != 0xFFU))
  {
    ((USBD_CDC_ItfTypeDef *)pdev->pUserData)->Control(hcdc->CmdOpCode,
                                                      (uint8_t *)hcdc->data,
                                                      (uint16_t)hcdc->CmdLength);
    hcdc->CmdOpCode = 0xFFU;
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_GetFSCfgDesc
  *         Return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_CDC_GetFSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_CDC_CfgDesc);

  return USBD_CDC_CfgDesc;
}

/**
  * @brief  USBD_CDC_GetHSCfgDesc
  *         Return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_CDC_GetHSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_CDC_CfgDesc);

  return USBD_CDC_CfgDesc;
}

/**
  * @brief  USBD_CDC_GetOtherSpeedCfgDesc
  *         Return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_CDC_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_CDC_CfgDesc);

  return USBD_CDC_CfgDesc;
}

/**
  * @brief  USBD_CDC_GetDeviceQualifierDescriptor
  *         return Device Qualifier descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
uint8_t *USBD_CDC_GetDeviceQualifierDescriptor(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_CDC_DeviceQualifierDesc);

  return USBD_CDC_DeviceQualifierDesc;
}

/**
  * @brief  USBD_CDC_RegisterInterface
  * @param  pdev: device instance
  * @param  fops: CD  Interface callback
  * @retval status
  */
uint8_t USBD_CDC_RegisterInterface(USBD_HandleTypeDef *pdev,
                                   USBD_CDC_ItfTypeDef *fops)
{
  if (fops == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  pdev->pUserData = fops;

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_SetTxBuffer
  * @param  pdev: device instance
  * @param  pbuff: Tx Buffer
  * @retval status
  */
// uint8_t USBD_CDC_SetTxBuffer(USBD_HandleTypeDef *pdev,
//                              uint8_t *pbuff, uint32_t length)
// {
//   USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
//
//   if (hcdc == NULL)
//   {
//     return (uint8_t)USBD_FAIL;
//   }
//
//   hcdc->TxBuffer = pbuff;
//   hcdc->TxLength = length;
//
//   return (uint8_t)USBD_OK;
// }

uint8_t  USBD_CDC_SetTxBuffer  (USBD_HandleTypeDef   *pdev,
                                uint8_t  *pbuff,
                                uint16_t length,
                                uint8_t endpoint_pair)
{
  USBD_CDC_HandleTypeDef   *hcdc = (USBD_CDC_HandleTypeDef*) pdev->pClassData;

  USBD_CDC_EP_HandleTypeDef* hEP_Tx;
  if (endpoint_pair == CDC_OUT_EP) {
    hEP_Tx = &hcdc->CDC_Tx;
  } else if (endpoint_pair == ODRIVE_OUT_EP) {
    hEP_Tx = &hcdc->REF_Tx;
  } else {
    return USBD_FAIL;
  }

  hEP_Tx->Buffer = pbuff;
  hEP_Tx->Length = length;
  if (endpoint_pair == CDC_OUT_EP) {
    hcdc->TxBuffer = pbuff;
    hcdc->TxLength = length;
  }

  return USBD_OK;
}


/**
  * @brief  USBD_CDC_SetRxBuffer
  * @param  pdev: device instance
  * @param  pbuff: Rx Buffer
  * @retval status
  */
// uint8_t USBD_CDC_SetRxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff)
// {
//   USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
//
//   if (hcdc == NULL)
//   {
//     return (uint8_t)USBD_FAIL;
//   }
//
//   hcdc->RxBuffer = pbuff;
//
//   return (uint8_t)USBD_OK;
// }
uint8_t  USBD_CDC_SetRxBuffer  (USBD_HandleTypeDef   *pdev,
                                uint8_t  *pbuff, uint8_t endpoint_pair)
{
  USBD_CDC_HandleTypeDef   *hcdc = (USBD_CDC_HandleTypeDef*) pdev->pClassData;

  USBD_CDC_EP_HandleTypeDef* hEP_Rx;
  if (endpoint_pair == CDC_OUT_EP) {
    hEP_Rx = &hcdc->CDC_Rx;
  } else if (endpoint_pair == ODRIVE_OUT_EP) {
    hEP_Rx = &hcdc->ODRIVE_Rx;
  } else {
    return USBD_FAIL;
  }

  hEP_Rx->Buffer = pbuff;

  return USBD_OK;
}

/**
  * @brief  USBD_CDC_TransmitPacket
  *         Transmit packet on IN endpoint
  * @param  pdev: device instance
  * @retval status
  */
// uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef *pdev)
// {
//   USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
//   USBD_StatusTypeDef ret = USBD_BUSY;
//
//   if (pdev->pClassData == NULL)
//   {
//     return (uint8_t)USBD_FAIL;
//   }
//
//   if (hcdc->TxState == 0U)
//   {
//     /* Tx Transfer in progress */
//     hcdc->TxState = 1U;
//
//     /* Update the packet total length */
//     pdev->ep_in[CDC_IN_EP & 0xFU].total_length = hcdc->TxLength;
//
//     /* Transmit next packet */
//     (void)USBD_LL_Transmit(pdev, CDC_IN_EP, hcdc->TxBuffer, hcdc->TxLength);
//
//     ret = USBD_OK;
//   }
//
//   return (uint8_t)ret;
// }
uint8_t  USBD_CDC_TransmitPacket(USBD_HandleTypeDef *pdev, uint8_t endpoint_pair)
{
  USBD_CDC_HandleTypeDef   *hcdc = (USBD_CDC_HandleTypeDef*) pdev->pClassData;

  if(pdev->pClassData != NULL)
  {
    // Select Endpoint
    USBD_CDC_EP_HandleTypeDef* hEP_Tx;
    uint8_t in_ep;
    if (endpoint_pair == CDC_OUT_EP) {
      hEP_Tx = &hcdc->CDC_Tx;
      in_ep = CDC_IN_EP;
    } else if (endpoint_pair == ODRIVE_OUT_EP) {
      hEP_Tx = &hcdc->REF_Tx;
      in_ep = ODRIVE_IN_EP;
    } else {
      return USBD_FAIL;
    }

    if(hEP_Tx->State == 0)
    {
      /* Tx Transfer in progress */
      hEP_Tx->State = 1;
      pdev->ep_in[in_ep & 0xFU].total_length = hEP_Tx->Length;
      hcdc->TxBuffer = hEP_Tx->Buffer;
      hcdc->TxLength = hEP_Tx->Length;

      /* Transmit next packet */
      USBD_LL_Transmit(pdev,
                       in_ep,
                       hEP_Tx->Buffer,
                       hEP_Tx->Length);

      return USBD_OK;
    }
    else
    {
      return USBD_BUSY;
    }
  }
  else
  {
    return USBD_FAIL;
  }
}

/**
  * @brief  USBD_CDC_ReceivePacket
  *         prepare OUT Endpoint for reception
  * @param  pdev: device instance
  * @retval status
  */
// uint8_t USBD_CDC_ReceivePacket(USBD_HandleTypeDef *pdev)
// {
//   USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
//
//   if (pdev->pClassData == NULL)
//   {
//     return (uint8_t)USBD_FAIL;
//   }
//
//   if (pdev->dev_speed == USBD_SPEED_HIGH)
//   {
//     /* Prepare Out endpoint to receive next packet */
//     (void)USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, hcdc->RxBuffer,
//                                  CDC_DATA_HS_OUT_PACKET_SIZE);
//   }
//   else
//   {
//     /* Prepare Out endpoint to receive next packet */
//     (void)USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, hcdc->RxBuffer,
//                                  CDC_DATA_FS_OUT_PACKET_SIZE);
//   }
//
//   return (uint8_t)USBD_OK;
// }

uint8_t  USBD_CDC_ReceivePacket(USBD_HandleTypeDef *pdev, uint8_t endpoint_pair)
{
  USBD_CDC_HandleTypeDef   *hcdc = (USBD_CDC_HandleTypeDef*) pdev->pClassData;

  /* Suspend or Resume USB Out process */
  if(pdev->pClassData != NULL)
  {
    // Select Endpoint
    USBD_CDC_EP_HandleTypeDef* hEP_Rx;
    uint8_t out_ep;
    if (endpoint_pair == CDC_OUT_EP) {
      hEP_Rx = &hcdc->CDC_Rx;
      out_ep = CDC_OUT_EP;
    } else if (endpoint_pair == ODRIVE_OUT_EP) {
      hEP_Rx = &hcdc->ODRIVE_Rx;
      out_ep = ODRIVE_OUT_EP;
    } else {
      return USBD_FAIL;
    }

    /* Prepare Out endpoint to receive next packet */
    USBD_LL_PrepareReceive(pdev,
                           out_ep,
                           hEP_Rx->Buffer,
                           pdev->dev_speed == USBD_SPEED_HIGH ? CDC_DATA_HS_OUT_PACKET_SIZE : CDC_DATA_FS_OUT_PACKET_SIZE);

    return USBD_OK;
  }
  else
  {
    return USBD_FAIL;
  }
}

/* WinUSB support ------------------------------------------------------------*/
/* This advertises IF2 (vendor interface) as WINUSB for Windows driverless access. */
#define USBD_WINUSB_IF_NUM                         0x02U
#define USB_WINUSBCOMM_COMPAT_ID_OS_DESC_SIZ      0x28U
#define USB_WINUSBCOMM_PROPS_OS_DESC_SIZ          0x8EU

__ALIGN_BEGIN static uint8_t USBD_WinUSBComm_Extended_Compat_ID_OS_Desc[USB_WINUSBCOMM_COMPAT_ID_OS_DESC_SIZ] __ALIGN_END =
{
  0x28, 0x00, 0x00, 0x00,                         /* dwLength */
  0x00, 0x01,                                     /* bcdVersion */
  0x04, 0x00,                                     /* wIndex */
  0x01,                                           /* bCount */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       /* reserved */
  USBD_WINUSB_IF_NUM,                             /* bFirstInterfaceNumber */
  0x00,                                           /* reserved */
  'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,       /* compatibleID */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* subCompatibleID */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00              /* reserved */
};

__ALIGN_BEGIN static uint8_t USBD_WinUSBComm_Extended_Properties_OS_Desc[USB_WINUSBCOMM_PROPS_OS_DESC_SIZ] __ALIGN_END =
{
  0x8E, 0x00, 0x00, 0x00,                         /* dwLength */
  0x00, 0x01,                                     /* bcdVersion */
  0x05, 0x00,                                     /* wIndex */
  0x01, 0x00,                                     /* wCount */

  0x84, 0x00, 0x00, 0x00,                         /* dwSize */
  0x01, 0x00, 0x00, 0x00,                         /* dwPropertyDataType = unicode string */
  0x28, 0x00,                                     /* wPropertyNameLength */
  'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00,
  't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00,
  'U', 0x00, 'I', 0x00, 'D', 0x00, 0x00, 0x00,
  0x4E, 0x00, 0x00, 0x00,                         /* dwPropertyDataLength */
  '{', 0x00, 'E', 0x00, 'A', 0x00, '0', 0x00, 'B', 0x00, 'D', 0x00, '5', 0x00, 'C', 0x00,
  '3', 0x00, '-', 0x00, '5', 0x00, '0', 0x00, 'F', 0x00, '3', 0x00, '-', 0x00, '4', 0x00,
  '8', 0x00, '8', 0x00, '8', 0x00, '-', 0x00, '8', 0x00, '4', 0x00, 'B', 0x00, '4', 0x00,
  '-', 0x00, '7', 0x00, '4', 0x00, 'E', 0x00, '5', 0x00, '0', 0x00, 'E', 0x00, '1', 0x00,
  '6', 0x00, '4', 0x00, '9', 0x00, 'D', 0x00, 'B', 0x00, '}', 0x00, 0x00, 0x00
};

static uint8_t USBD_WinUSBComm_SendOSDescriptor(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req,
                                                 uint8_t *desc, uint16_t desc_size)
{
  uint16_t tx_len = MIN(desc_size, req->wLength);
  (void)USBD_CtlSendData(pdev, desc, tx_len);
  return (uint8_t)USBD_OK;
}

static uint8_t USBD_WinUSBComm_SetupVendor(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  if (req->bRequest != MS_VendorCode)
  {
    USBD_CtlError(pdev, req);
    return (uint8_t)USBD_FAIL;
  }

  /* Extended Compat ID descriptor (OS 1.0): wIndex = 0x0004 */
  if (req->wIndex == 0x0004U)
  {
    return USBD_WinUSBComm_SendOSDescriptor(pdev, req,
                                            USBD_WinUSBComm_Extended_Compat_ID_OS_Desc,
                                            (uint16_t)sizeof(USBD_WinUSBComm_Extended_Compat_ID_OS_Desc));
  }

  /*
   * Extended Properties descriptor (OS 1.0): accept both encodings seen in the wild.
   * - Common form: wIndex = 0x0005, wValue = interface number
   * - Alternate form: wValue = 0x0005, wIndex = interface number
   */
  if (((req->wIndex == 0x0005U) && (LOBYTE(req->wValue) == USBD_WINUSB_IF_NUM)) ||
      ((req->wValue == 0x0005U) && (LOBYTE(req->wIndex) == USBD_WINUSB_IF_NUM)))
  {
    return USBD_WinUSBComm_SendOSDescriptor(pdev, req,
                                            USBD_WinUSBComm_Extended_Properties_OS_Desc,
                                            (uint16_t)sizeof(USBD_WinUSBComm_Extended_Properties_OS_Desc));
  }

  USBD_CtlError(pdev, req);
  return (uint8_t)USBD_FAIL;
}
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
