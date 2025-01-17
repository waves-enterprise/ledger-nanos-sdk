/**
  ******************************************************************************
  * @file    usbd_req.c
  * @author  MCD Application Team
  * @version V2.4.1
  * @date    19-June-2015 
  * @brief   This file provides the standard USB requests following chapter 9.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 
#include "os_helpers.h"
#include "os_io.h"
#include "os_pic.h"

/* Includes ------------------------------------------------------------------*/
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"


/** @addtogroup STM32_USBD_STATE_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_REQ 
  * @brief USB standard requests module
  * @{
  */ 

/** @defgroup USBD_REQ_Private_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup USBD_REQ_Private_Defines
  * @{
  */ 

/**
  * @}
  */ 


/** @defgroup USBD_REQ_Private_Macros
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup USBD_REQ_Private_Variables
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup USBD_REQ_Private_FunctionPrototypes
  * @{
  */ 
void USBD_GetDescriptor(USBD_HandleTypeDef *pdev , 
                               USBD_SetupReqTypedef *req);

void USBD_SetAddress(USBD_HandleTypeDef *pdev , 
                            USBD_SetupReqTypedef *req);

void USBD_SetConfig(USBD_HandleTypeDef *pdev , 
                           USBD_SetupReqTypedef *req);

void USBD_GetConfig(USBD_HandleTypeDef *pdev , 
                           USBD_SetupReqTypedef *req);

void USBD_GetStatus(USBD_HandleTypeDef *pdev , 
                           USBD_SetupReqTypedef *req);

void USBD_SetFeature(USBD_HandleTypeDef *pdev , 
                            USBD_SetupReqTypedef *req);

void USBD_ClrFeature(USBD_HandleTypeDef *pdev , 
                            USBD_SetupReqTypedef *req);

uint8_t USBD_GetLen(uint8_t *buf);

/**
  * @}
  */ 


/** @defgroup USBD_REQ_Private_Functions
  * @{
  */ 

unsigned int usbd_is_valid_intf(USBD_HandleTypeDef *pdev , unsigned int intf) {
  return intf < USBD_MAX_NUM_INTERFACES && pdev->interfacesClass[intf].pClass != NULL;
}

/**
* @brief  USBD_StdDevReq
*         Handle standard usb device requests
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
USBD_StatusTypeDef  USBD_StdDevReq (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef  *req)
{
  USBD_StatusTypeDef ret = USBD_OK;  
  
  switch (req->bRequest) 
  {
  case USB_REQ_GET_DESCRIPTOR: 
    
    USBD_GetDescriptor (pdev, req) ;
    break;
    
  case USB_REQ_SET_ADDRESS:                      
    USBD_SetAddress(pdev, req);
    break;
    
  case USB_REQ_SET_CONFIGURATION:                    
    USBD_SetConfig (pdev , req);
    break;
    
  case USB_REQ_GET_CONFIGURATION:                 
    USBD_GetConfig (pdev , req);
    break;
    
  case USB_REQ_GET_STATUS:                                  
    USBD_GetStatus (pdev , req);
    break;
    
    
  case USB_REQ_SET_FEATURE:   
    USBD_SetFeature (pdev , req);    
    break;
    
  case USB_REQ_CLEAR_FEATURE:                                   
    USBD_ClrFeature (pdev , req);
    break;
    
  default:  
    USBD_CtlError(pdev , req);
    break;
  }
  
  return ret;
}

/**
* @brief  USBD_StdItfReq
*         Handle standard usb interface requests
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
USBD_StatusTypeDef  USBD_StdItfReq (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef  *req)
{
  USBD_StatusTypeDef ret = USBD_OK; 
  
  switch (pdev->dev_state) 
  {
  case USBD_STATE_CONFIGURED:
    
    if (usbd_is_valid_intf(pdev, LOBYTE(req->wIndex))) 
    {
      ((Setup_t)PIC(pdev->interfacesClass[LOBYTE(req->wIndex)].pClass->Setup)) (pdev, req);
      
      if((req->wLength == 0)&& (ret == USBD_OK))
      {
         USBD_CtlSendStatus(pdev);
      }
    } 
    else 
    {                                               
       USBD_CtlError(pdev , req);
    }
    break;
    
  default:
     USBD_CtlError(pdev , req);
    break;
  }
  return USBD_OK;
}

/**
* @brief  USBD_StdEPReq
*         Handle standard usb endpoint requests
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
USBD_StatusTypeDef  USBD_StdEPReq (USBD_HandleTypeDef *pdev , USBD_SetupReqTypedef  *req)
{
  
  uint8_t   ep_addr;
  USBD_StatusTypeDef ret = USBD_OK; 
  ep_addr  = LOBYTE(req->wIndex);

  // avoid processing for out of bounds endpoints
  if ((ep_addr & 0x7F) > IO_USB_MAX_ENDPOINTS) {
    USBD_CtlError(pdev , req);
    return ret;
  }

  // Note: this function consider endpoints are assigned to interfaces as a bijection (endpoint 
  // 0x01 and 0x81 are assigend to interface 1). This is done to avoid a long endpoint lookup
  // with the declared interfaces to handle the corresponding setup packet.
  // Moreover, endpoint directed setup packet are really uncommon.
  
  /* Check if it is a class request */
  if ((req->bmRequest & 0x60) == 0x20 && usbd_is_valid_intf(pdev, LOBYTE(req->wIndex)))
  {
    ((Setup_t)PIC(pdev->interfacesClass[LOBYTE(req->wIndex)].pClass->Setup)) (pdev, req);
    
    return USBD_OK;
  }


  switch (req->bRequest) 
  {
    
  case USB_REQ_SET_FEATURE :
    
    switch (pdev->dev_state) 
    {
    case USBD_STATE_ADDRESSED:          
      if ((ep_addr != 0x00) && (ep_addr != 0x80)) 
      {
        USBD_LL_StallEP(pdev , ep_addr);
      }
      break;  
      
    case USBD_STATE_CONFIGURED:   
      if (req->wValue == USB_FEATURE_EP_HALT)
      {
        if ((ep_addr != 0x00) && (ep_addr != 0x80)) 
        { 
          USBD_LL_StallEP(pdev , ep_addr);
          
        }
      }
      if(usbd_is_valid_intf(pdev, LOBYTE(req->wIndex))) {
        ((Setup_t)PIC(pdev->interfacesClass[LOBYTE(req->wIndex)].pClass->Setup)) (pdev, req);   
      }
      USBD_CtlSendStatus(pdev);
      break;
        
    default:
      USBD_CtlError(pdev , req);
      break;    
    }
    break;
    
  case USB_REQ_CLEAR_FEATURE :
    
    switch (pdev->dev_state) 
    {
    case USBD_STATE_ADDRESSED:          
      if ((ep_addr != 0x00) && (ep_addr != 0x80)) 
      {
        USBD_LL_StallEP(pdev , ep_addr);
      }
      break;  
      
    case USBD_STATE_CONFIGURED:   
      if (req->wValue == USB_FEATURE_EP_HALT)
      {
        if ((ep_addr & 0x7F) != 0x00) 
        {        
          USBD_LL_ClearStallEP(pdev , ep_addr);

          if(usbd_is_valid_intf(pdev, LOBYTE(req->wIndex))) {
            ((Setup_t)PIC(pdev->interfacesClass[LOBYTE(req->wIndex)].pClass->Setup)) (pdev, req);
          }
        }
        USBD_CtlSendStatus(pdev);
      }
      break;
      
    default:                         
      USBD_CtlError(pdev , req);
      break;    
    }
    break;
    
  case USB_REQ_GET_STATUS:                  
    switch (pdev->dev_state) 
    {
    case USBD_STATE_ADDRESSED:          
      if ((ep_addr & 0x7F) != 0x00) 
      {
        USBD_LL_StallEP(pdev , ep_addr);
      }
      break;  
      
    case USBD_STATE_CONFIGURED:
      {
        unsigned short status = USBD_LL_IsStallEP(pdev, ep_addr)? 1 : 0;        
        USBD_CtlSendData (pdev,
                          (uint8_t *)&status,
                          2);
      }
      break;
      
    default:
      USBD_CtlError(pdev , req);
      break;
    }
    break;
    
  default:
    break;
  }
  return ret;
}
/**
* @brief  USBD_GetDescriptor
*         Handle Get Descriptor requests
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
void USBD_GetDescriptor(USBD_HandleTypeDef *pdev , 
                               USBD_SetupReqTypedef *req)
{
  uint16_t len = 0;
  uint8_t *pbuf = NULL;
  
    
  switch (req->wValue >> 8)
  { 
#if (USBD_LPM_ENABLED == 1)
  case USB_DESC_TYPE_BOS:
    if(pdev->pDesc->GetBOSDescriptor != NULL) {
      pbuf = ((GetBOSDescriptor_t)PIC(pdev->pDesc->GetBOSDescriptor))(pdev->dev_speed, &len);
    }
    else {
      goto default_error;
    }
    break;
#endif    
  case USB_DESC_TYPE_DEVICE:
    pbuf = ((GetDeviceDescriptor_t)PIC(pdev->pDesc->GetDeviceDescriptor))(pdev->dev_speed, &len);
    break;
    
  case USB_DESC_TYPE_CONFIGURATION:     
    if(pdev->interfacesClass[0].pClass != NULL) {
      if(pdev->dev_speed == USBD_SPEED_HIGH )   
      {
        pbuf   = (uint8_t *)((GetHSConfigDescriptor_t)PIC(pdev->interfacesClass[0].pClass->GetHSConfigDescriptor))(&len);
        //pbuf[1] = USB_DESC_TYPE_CONFIGURATION; CONST BUFFER KTHX
      }
      else
      {
        pbuf   = (uint8_t *)((GetFSConfigDescriptor_t)PIC(pdev->interfacesClass[0].pClass->GetFSConfigDescriptor))(&len);
        //pbuf[1] = USB_DESC_TYPE_CONFIGURATION; CONST BUFFER KTHX
      }
    }
    break;
    
  case USB_DESC_TYPE_STRING:
    switch ((uint8_t)(req->wValue))
    {
    case USBD_IDX_LANGID_STR:
     pbuf = ((GetLangIDStrDescriptor_t)PIC(pdev->pDesc->GetLangIDStrDescriptor))(pdev->dev_speed, &len);        
      break;
      
    case USBD_IDX_MFC_STR:
      pbuf = ((GetManufacturerStrDescriptor_t)PIC(pdev->pDesc->GetManufacturerStrDescriptor))(pdev->dev_speed, &len);
      break;
      
    case USBD_IDX_PRODUCT_STR:
      pbuf = ((GetProductStrDescriptor_t)PIC(pdev->pDesc->GetProductStrDescriptor))(pdev->dev_speed, &len);
      break;
      
    case USBD_IDX_SERIAL_STR:
      pbuf = ((GetSerialStrDescriptor_t)PIC(pdev->pDesc->GetSerialStrDescriptor))(pdev->dev_speed, &len);
      break;
      
    case USBD_IDX_CONFIG_STR:
      pbuf = ((GetConfigurationStrDescriptor_t)PIC(pdev->pDesc->GetConfigurationStrDescriptor))(pdev->dev_speed, &len);
      break;
      
    case USBD_IDX_INTERFACE_STR:
      pbuf = ((GetInterfaceStrDescriptor_t)PIC(pdev->pDesc->GetInterfaceStrDescriptor))(pdev->dev_speed, &len);
      break;
      
    default:
#if (USBD_SUPPORT_USER_STRING == 1)
      if(pdev->interfacesClass[0].pClass != NULL) {
        pbuf = ((GetUsrStrDescriptor_t)PIC(pdev->interfacesClass[0].pClass->GetUsrStrDescriptor))(pdev, (req->wValue) , &len);
      }
      break;
#else      
      goto default_error;
#endif   
    }
    break;
  case USB_DESC_TYPE_DEVICE_QUALIFIER:                   

    if(pdev->dev_speed == USBD_SPEED_HIGH && pdev->interfacesClass[0].pClass != NULL )   
    {
      pbuf   = (uint8_t *)((GetDeviceQualifierDescriptor_t)PIC(pdev->interfacesClass[0].pClass->GetDeviceQualifierDescriptor))(&len);
      break;
    }
    else
    {
      goto default_error;
    } 

  case USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION:
    if(pdev->dev_speed == USBD_SPEED_HIGH && pdev->interfacesClass[0].pClass != NULL)   
    {
      pbuf   = (uint8_t *)((GetOtherSpeedConfigDescriptor_t)PIC(pdev->interfacesClass[0].pClass->GetOtherSpeedConfigDescriptor))(&len);
      // pbuf[1] = USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION; CONST BUFFER KTHX
      break; 
    }
    else
    {
      goto default_error;
    }

  default: 
  default_error:
     USBD_CtlError(pdev , req);
    return;
  }
  
  if((len != 0)&& (req->wLength != 0))
  {
    
    len = MIN(len , req->wLength);
    
    // prepare abort if host does not read the whole data
    //USBD_CtlReceiveStatus(pdev);

    // start transfer
    USBD_CtlSendData (pdev, 
                      pbuf,
                      len);
  }
  
}

/**
* @brief  USBD_SetAddress
*         Set device address
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
void USBD_SetAddress(USBD_HandleTypeDef *pdev , 
                            USBD_SetupReqTypedef *req)
{
  uint8_t  dev_addr; 
  
  if ((req->wIndex == 0) && (req->wLength == 0)) 
  {
    dev_addr = (uint8_t)(req->wValue) & 0x7F;     
    
    if (pdev->dev_state == USBD_STATE_CONFIGURED) 
    {
      USBD_CtlError(pdev , req);
    } 
    else 
    {
      pdev->dev_address = dev_addr;
      USBD_LL_SetUSBAddress(pdev, dev_addr);               
      USBD_CtlSendStatus(pdev);                         
      
      if (dev_addr != 0) 
      {
        pdev->dev_state  = USBD_STATE_ADDRESSED;
      } 
      else 
      {
        pdev->dev_state  = USBD_STATE_DEFAULT; 
      }
    }
  } 
  else 
  {
     USBD_CtlError(pdev , req);                        
  } 
}

/**
* @brief  USBD_SetConfig
*         Handle Set device configuration request
* @param  pdev: device instanceUSBD_SetConfig
* @param  req: usb request
* @retval status
*/
void USBD_SetConfig(USBD_HandleTypeDef *pdev , 
                           USBD_SetupReqTypedef *req)
{
  
  uint8_t  cfgidx;
  
  cfgidx = (uint8_t)(req->wValue);                 
  
  if (cfgidx > USBD_MAX_NUM_CONFIGURATION ) 
  {            
     USBD_CtlError(pdev , req);                              
  } 
  else 
  {
    switch (pdev->dev_state) 
    {
    case USBD_STATE_ADDRESSED:
      if (cfgidx) 
      {                                                                               
        pdev->dev_config = cfgidx;
        pdev->dev_state = USBD_STATE_CONFIGURED;
        if(USBD_SetClassConfig(pdev , cfgidx) == USBD_FAIL)
        {
          USBD_CtlError(pdev , req);  
          return;
        }
      }
      USBD_CtlSendStatus(pdev);
      break;
      
    case USBD_STATE_CONFIGURED:
      if (cfgidx == 0) 
      {                           
        pdev->dev_state = USBD_STATE_ADDRESSED;
        pdev->dev_config = cfgidx;          
        USBD_ClrClassConfig(pdev , cfgidx);
        USBD_CtlSendStatus(pdev);
      } 
      else  if (cfgidx != pdev->dev_config) 
      {
        /* Clear old configuration */
        USBD_ClrClassConfig(pdev , pdev->dev_config);
        
        /* set new configuration */
        pdev->dev_config = cfgidx;
        if(USBD_SetClassConfig(pdev , cfgidx) == USBD_FAIL)
        {
          USBD_CtlError(pdev , req);  
          return;
        }
      }
      USBD_CtlSendStatus(pdev);
      break;
      
    default:          
       USBD_CtlError(pdev , req);                     
      break;
    }
  }
}

/**
* @brief  USBD_GetConfig
*         Handle Get device configuration request
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
void USBD_GetConfig(USBD_HandleTypeDef *pdev , 
                           USBD_SetupReqTypedef *req)
{

  if (req->wLength != 1) 
  {                   
     USBD_CtlError(pdev , req);
  }
  else 
  {
    switch (pdev->dev_state )  
    {
    case USBD_STATE_ADDRESSED:                     
      pdev->dev_default_config = 0;
      USBD_CtlSendData (pdev, 
                        (uint8_t *)&pdev->dev_default_config,
                        1);
      break;
      
    case USBD_STATE_CONFIGURED:   
      USBD_CtlSendData (pdev, 
                        (uint8_t *)&pdev->dev_config,
                        1);
      break;
      
    default:
       USBD_CtlError(pdev , req);
      break;
    }
  }
}

/**
* @brief  USBD_GetStatus
*         Handle Get Status request
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
void USBD_GetStatus(USBD_HandleTypeDef *pdev , 
                           USBD_SetupReqTypedef *req)
{
  
    
  switch (pdev->dev_state) 
  {
  case USBD_STATE_ADDRESSED:
  case USBD_STATE_CONFIGURED:
    
#if ( USBD_SELF_POWERED == 1)
    pdev->dev_config_status = USB_CONFIG_SELF_POWERED;                                  
#else
    pdev->dev_config_status = 0;                                   
#endif
                      
    if (pdev->dev_remote_wakeup) USBD_CtlReceiveStatus(pdev);
    {
       pdev->dev_config_status |= USB_CONFIG_REMOTE_WAKEUP;                                
    }
    
    USBD_CtlSendData (pdev, 
                      (uint8_t *)& pdev->dev_config_status,
                      2);
    break;
    
  default :
    USBD_CtlError(pdev , req);                        
    break;
  }
}


/**
* @brief  USBD_SetFeature
*         Handle Set device feature request
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
void USBD_SetFeature(USBD_HandleTypeDef *pdev , 
                            USBD_SetupReqTypedef *req)
{

  if (req->wValue == USB_FEATURE_REMOTE_WAKEUP)
  {
    pdev->dev_remote_wakeup = 1;  
    if(usbd_is_valid_intf(pdev, LOBYTE(req->wIndex))) {
      ((Setup_t)PIC(pdev->interfacesClass[LOBYTE(req->wIndex)].pClass->Setup)) (pdev, req);   
    }
    USBD_CtlSendStatus(pdev);
  }

}


/**
* @brief  USBD_ClrFeature
*         Handle clear device feature request
* @param  pdev: device instance
* @param  req: usb request
* @retval status
*/
void USBD_ClrFeature(USBD_HandleTypeDef *pdev , 
                            USBD_SetupReqTypedef *req)
{
  switch (pdev->dev_state)
  {
  case USBD_STATE_ADDRESSED:
  case USBD_STATE_CONFIGURED:
    if (req->wValue == USB_FEATURE_REMOTE_WAKEUP) 
    {
      pdev->dev_remote_wakeup = 0; 
      if(usbd_is_valid_intf(pdev, LOBYTE(req->wIndex))) {
        ((Setup_t)PIC(pdev->interfacesClass[LOBYTE(req->wIndex)].pClass->Setup)) (pdev, req);   
      }
      USBD_CtlSendStatus(pdev);
    }
    break;
    
  default :
     USBD_CtlError(pdev , req);
    break;
  }
}

/**
* @brief  USBD_ParseSetupRequest 
*         Copy buffer into setup structure
* @param  pdev: device instance
* @param  req: usb request
* @retval None
*/

void USBD_ParseSetupRequest(USBD_SetupReqTypedef *req, uint8_t *pdata)
{
  req->bmRequest     = *(uint8_t *)  (pdata);
  req->bRequest      = *(uint8_t *)  (pdata +  1);
  req->wValue        = SWAPBYTE      (pdata +  2);
  req->wIndex        = SWAPBYTE      (pdata +  4);
  req->wLength       = SWAPBYTE      (pdata +  6);

}

/**
* @brief  USBD_CtlError 
*         Handle USB low level Error
* @param  pdev: device instance
* @param  req: usb request
* @retval None
*/
void USBD_CtlStall( USBD_HandleTypeDef *pdev)
{
  USBD_LL_StallEP(pdev , 0x80);
  USBD_LL_StallEP(pdev , 0);
}

__weak void USBD_CtlError( USBD_HandleTypeDef *pdev ,
                            USBD_SetupReqTypedef *req)
{
  UNUSED(req);
  USBD_CtlStall(pdev);
}


/**
  * @brief  USBD_GetString
  *         Convert Ascii string into unicode one
  * @param  desc : descriptor buffer
  * @param  unicode : Formatted string buffer (unicode)
  * @param  len : descriptor length
  * @retval None
  */
void USBD_GetString(uint8_t *desc, uint8_t *unicode, uint16_t *len)
{
  uint8_t idx = 0;
  
  if (desc != NULL) 
  {
    *len =  USBD_GetLen(desc) * 2 + 2;    
    unicode[idx++] = *len;
    unicode[idx++] =  USB_DESC_TYPE_STRING;
    
    while (*desc != '\0') 
    {
      unicode[idx++] = *desc++;
      unicode[idx++] =  0x00;
    }
  } 
}

/**
  * @brief  USBD_GetLen
  *         return the string length
   * @param  buf : pointer to the ascii string buffer
  * @retval string length
  */
uint8_t USBD_GetLen(uint8_t *buf)
{
    uint8_t  len = 0;

    while (*buf != '\0') 
    {
        len++;
        buf++;
    }

    return len;
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
