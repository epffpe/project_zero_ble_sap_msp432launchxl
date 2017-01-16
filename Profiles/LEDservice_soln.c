/* --COPYRIGHT--,BSD
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/*******************************************************************************
 *                                INCLUDES
 ******************************************************************************/
#include <string.h>

#include <xdc/runtime/Log.h>
#include <xdc/runtime/Diags.h>

#include "bcomdef.h"

#include <ti/sap/snp.h>
#include <ti/sap/snp_rpc.h>
#include <ti/sap/snp_rpc_synchro.h>
#include <ti/sap/sap.h>
#include "LEDservice_soln.h"

/*******************************************************************************
 *                                   MACROS
 ******************************************************************************/
#define LED_NUM_ATTR_SUPPORTED 3

/*******************************************************************************
 *                                CONSTANTS
 ******************************************************************************/

/*******************************************************************************
 *                                  TYPEDEFS
 ******************************************************************************/

/*******************************************************************************
 *                                  GLOBAL TYPEDEFS
 ******************************************************************************/

// LED_Service Service UUID
static uint8_t LEDServiceUUID[SNP_128BIT_UUID_SIZE] =
{ TI_BASE_UUID_128(LED_SERVICE_UUID) };

// LED0 UUID
static uint8_t ls_LED0UUID[SNP_128BIT_UUID_SIZE] =
{ TI_BASE_UUID_128(LS_LED0_UUID) };

// LED1 UUID
static uint8_t ls_LED1UUID[SNP_128BIT_UUID_SIZE] =
{ TI_BASE_UUID_128(LS_LED1_UUID) };

// LED2 UUID
static uint8_t ls_LED2UUID[SNP_128BIT_UUID_SIZE] =
{ TI_BASE_UUID_128(LS_LED2_UUID) };

/*******************************************************************************
 *                             LOCAL VARIABLES
 ******************************************************************************/
static sensorCBs_t *pAppCBs = NULL;

/*******************************************************************************
 *                              Profile Attributes - TYPEDEFS
 ******************************************************************************/
static SAP_Service_t    LEDService;
static SAP_CharHandle_t LEDServiceCharHandles[LED_NUM_ATTR_SUPPORTED];

// Profile Service attribute
static UUIDType_t LEDServiceDecl =
{ SNP_128BIT_UUID_SIZE, LEDServiceUUID };

// Characteristic "LED0" Value variable
static uint8_t ls_LED0Val[LS_LED0_LEN] = {0};

// Length of data in characteristic "LED0" Value variable, initialized to minimal size.
static uint16_t ls_LED0ValLen = LS_LED0_LEN_MIN;

// Characteristic "LED1" Value variable
static uint8_t ls_LED1Val[LS_LED1_LEN] = {0};

// Length of data in characteristic "LED1" Value variable, initialized to minimal size.
static uint16_t ls_LED1ValLen = LS_LED1_LEN_MIN;

// Characteristic "LED2" Value variable
static uint8_t ls_LED2Val[LS_LED2_LEN] = {0};

// Length of data in characteristic "LED2" Value variable, initialized to minimal size.
static uint16_t ls_LED2ValLen = LS_LED2_LEN_MIN;

/*******************************************************************************
 *                              Profile Attributes - TABLE
 ******************************************************************************/
static SAP_Char_t LEDAttrTable[LED_NUM_ATTR_SUPPORTED] =
{
        // LED0 Characteristic
        {
        { SNP_128BIT_UUID_SIZE, ls_LED0UUID }, /* UUID             */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE | SNP_GATT_PROP_WRITE_NORSP, /* Properties       */
        SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE, /* Permissions      */
        /* User Description */
        /* CCCD             */
        /* Format           */
        },

        // LED1 Characteristic
        {
        { SNP_128BIT_UUID_SIZE, ls_LED1UUID }, /* UUID             */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE | SNP_GATT_PROP_WRITE_NORSP, /* Properties       */
        SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE, /* Permissions      */
        /* User Description */
        /* CCCD             */
        /* Format           */
        },

		// LED2 Characteristic
        {
        { SNP_128BIT_UUID_SIZE, ls_LED2UUID }, /* UUID             */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE | SNP_GATT_PROP_WRITE_NORSP, /* Properties       */
        SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE, /* Permissions      */
        /* User Description */
        /* CCCD             */
        /* Format           */
        },
};

/*******************************************************************************
 *                                  LOCAL FUNCTIONS
 ******************************************************************************/
static void LED_processSNPEventCB(uint16_t event, snpEventParam_t *param);

static bStatus_t LEDService_ReadAttrCB(void *context, uint16_t conn,
        uint16_t charHdl, uint16_t offset, uint16_t size, uint16_t * pLen,
        uint8_t *pData);

static bStatus_t LEDService_WriteAttrCB(void *context, uint16_t conn,
        uint16_t charHdl, uint16_t len, uint8_t *pData);

/*******************************************************************************
 *                                 PUBLIC FUNCTIONS
 ******************************************************************************/

/*******************************************************************************
 * @fn      LEDService_addService
 *
 * @brief   Initializes the LedService service by registering
 *          GATT attributes with the GATT server.
 *
 * @return  Success or Failure
 */
bStatus_t LEDService_addService(void)
{
    // Register to recieve Connection Established Events
    SAP_registerEventCB(LED_processSNPEventCB,
            SNP_CONN_EST_EVT | SNP_CONN_TERM_EVT);

    // Build Service to register with NP
    LEDService.serviceUUID = LEDServiceDecl;
    LEDService.serviceType = SNP_PRIMARY_SERVICE;
    LEDService.charTableLen = LED_NUM_ATTR_SUPPORTED;
    LEDService.charTable = LEDAttrTable;
    LEDService.context = NULL;
    LEDService.charReadCallback = LEDService_ReadAttrCB;
    LEDService.charWriteCallback = LEDService_WriteAttrCB;
    LEDService.cccdIndCallback = NULL;
    LEDService.charAttrHandles = LEDServiceCharHandles;

    Log_info1("Registered service, %d attributes", (IArg)LED_NUM_ATTR_SUPPORTED);

    // Service is set up, register with GATT server on the SNP.
    return SAP_registerService(&LEDService);
}

/*******************************************************************************
 * @fn      LEDService_registerAppCBs
 *
 * @brief   Registers the application callback function. Only call
 *          this function once.
 *
 * @param   callbacks - pointer to application callbacks.
 *
 * @return  SUCCESS or bleAlreadyInRequestedMode
 */
bStatus_t LEDService_registerAppCBs(sensorCBs_t *appCallbacks)
{
    if (pAppCBs == NULL)
    {
        if (appCallbacks != NULL)
        {
          pAppCBs = appCallbacks;
          Log_info1("Registered callbacks to application. Struct %p", (IArg)appCallbacks);
        }
        else
        {
          Log_warning0("Null pointer given for app callbacks.");
          return ( FAILURE );
        }
        return (SUCCESS);
    }

    return (bleAlreadyInRequestedMode);
}

/*******************************************************************************
 * @fn      LEDService_setParameter
 *
 * @brief   Set a LedService parameter.
 *
 * @param   param - Profile parameter ID
 * @param   len - length of data to write
 * @param   value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t LEDService_setParameter( uint8_t param, uint16_t len, void *value )
{
    bStatus_t ret = SUCCESS;
    uint8_t  *pAttrVal;
    uint16_t valMinLen;
    uint16_t valMaxLen;

    switch (SENSOR_ID_CHAR(param))
    {
    case LED_CHAR0:
        pAttrVal  =  ls_LED0Val;
        valMinLen =  LS_LED0_LEN_MIN;
        valMaxLen =  LS_LED0_LEN;
        Log_info2("SetParameter : %s len: %d", (IArg)"LED0", (IArg)len);
        break;

    case LED_CHAR1:
        pAttrVal  =  ls_LED1Val;
        valMinLen =  LS_LED1_LEN_MIN;
        valMaxLen =  LS_LED1_LEN;
        Log_info2("SetParameter : %s len: %d", (IArg)"LED1", (IArg)len);
        break;

    case LED_CHAR2:
        pAttrVal  =  ls_LED2Val;
        valMinLen =  LS_LED2_LEN_MIN;
        valMaxLen =  LS_LED2_LEN;
        Log_info2("SetParameter : %s len: %d", (IArg)"LED2", (IArg)len);
        break;

    default:
        Log_error1("SetParameter: Parameter #%d not valid.", (IArg)param);
        ret = INVALIDPARAMETER;
        break;
    }

    // Check bounds, update value and send notification or indication if possible.
    if ( len <= valMaxLen && len >= valMinLen )
    {
      memcpy(pAttrVal, value, len);
    }
    else
    {
      Log_error3("Length outside bounds: Len: %d MinLen: %d MaxLen: %d.", (IArg)len, (IArg)valMinLen, (IArg)valMaxLen);
      ret = bleInvalidRange;
    }

    return ret;
}

/*******************************************************************************
 * @fn      LEDService_getParameter
 *
 * @brief   Get a LedService parameter
 *
 * @param   param - Profile parameter IDz
 * @param   value - pointer to data to put.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t LEDService_getParameter(uint8_t param, void *value)
{
    bStatus_t ret = SUCCESS;
    switch (SENSOR_ID_CHAR(param))
    {
      case LED_CHAR0:
        memcpy(value, ls_LED0Val, ls_LED0ValLen);
        Log_info2("GetParameter : %s returning %d bytes", (IArg)"LED0",
                  (IArg)ls_LED0ValLen);
        break;

      case LED_CHAR1:
        memcpy(value, ls_LED1Val, ls_LED1ValLen);
        Log_info2("GetParameter : %s returning %d bytes", (IArg)"LED1",
                  (IArg)ls_LED1ValLen);
        break;

      case LED_CHAR2:
        memcpy(value, ls_LED2Val, ls_LED2ValLen);
        Log_info2("GetParameter : %s returning %d bytes", (IArg)"LED2",
                  (IArg)ls_LED2ValLen);
        break;

      default:
        Log_error1("GetParameter: Parameter #%d not valid.", (IArg)param);
        ret = INVALIDPARAMETER;
        break;
  }
  return ret;
}

/*********************************************************************
 * @fn      LEDService_ReadAttrCB
 *
 * @brief   Read an attribute.
 *
 * @param   context - context used when registering service
 * @param   conn    - connection handle ID
 * @param   charHdl - characteristic value handle
 * @param   offset  - offset of data to be read
 * @param   size    - maximum size of data bytes to be read
 * @param   pLen    - amount of bytes copied into pData
 * @param   pData   - pointer to copy read data
 *
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t LEDService_ReadAttrCB(void *context, uint16_t conn,
        uint16_t charHdl, uint16_t offset, uint16_t size, uint16_t * pLen,
        uint8_t *pData)
{
    // Get characteristic from handle
    uint8_t charID = utilGetCharIDFromHdl(charHdl, LEDServiceCharHandles,
    LED_NUM_ATTR_SUPPORTED);
    uint8_t isValid = 0;

    // Find settings for the characteristic to be read.
    switch (SENSOR_ID_CHAR(charID))
    {
      case LED_CHAR0:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            *pLen = sizeof(ls_LED0Val);
            memcpy(pData, ls_LED0Val, sizeof(ls_LED0Val));
            isValid = 1;
            Log_info3("ReadAttrCB : %s connHandle: %d offset: %d",
                       (IArg)"LED0",
                       (IArg)conn,
                       (IArg)offset);
            break;

        /* Other considerations for LED0 can be inserted here */
        default:
            break;
        }
        break;

    case LED_CHAR1:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            *pLen = sizeof(ls_LED1Val);
            memcpy(pData, ls_LED1Val, sizeof(ls_LED1Val));
            isValid = 1;
            Log_info3("ReadAttrCB : %s connHandle: %d offset: %d",
                       (IArg)"LED1",
                       (IArg)conn,
                       (IArg)offset);
            break;

        /* Other considerations for LED1 can be inserted here */
        default:
          Log_error0("Attribute was not found.");
            break;
        }
        break;

        case LED_CHAR2:
          switch (SENSOR_ID_CHARTYPE(charID))
          {
          case SENSOR_VALUE:
              *pLen = sizeof(ls_LED2Val);
              memcpy(pData, ls_LED2Val, sizeof(ls_LED2Val));
              isValid = 1;
              Log_info3("ReadAttrCB : %s connHandle: %d offset: %d",
                         (IArg)"LED2",
                         (IArg)conn,
                         (IArg)offset);
              break;

          /* Other considerations for LED2 can be inserted here */
          default:
              break;
          }
          break;


      default:
        break;
    }

    if (isValid)
    {
        return (SNP_SUCCESS);
    }

    // Unable to find handle - set len to 0 and return error code
    *pLen = 0;
    return (SNP_UNKNOWN_ATTRIBUTE);
}

/*********************************************************************
 * @fn      LEDService_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 * @param   method - type of write message
 *
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t LEDService_WriteAttrCB(void *context, uint16_t conn,
        uint16_t charHdl, uint16_t len, uint8_t *pData)
{
    bStatus_t status = SNP_UNKNOWN_ATTRIBUTE;
    uint8 notifyApp = SENSOR_UNKNOWN_CHAR;

    // Get characteristic from handle
    uint8_t charID = utilGetCharIDFromHdl(charHdl, LEDServiceCharHandles,
    LED_NUM_ATTR_SUPPORTED);

    // Find settings for the characteristic to be read.
    switch (SENSOR_ID_CHAR(charID))
    {
      case LED_CHAR0:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            if (len == sizeof(ls_LED0Val))
            {
                memcpy(ls_LED0Val, pData, sizeof(ls_LED0Val));
                status = SNP_SUCCESS;
                notifyApp = LS_LED0_ID;
            }

            Log_info3("WriteAttrCB : %s connHandle(%d) len(%d)",
                   (IArg)"LED0",
                   (IArg)conn,
                   (IArg)len);
            break;

        /* Other considerations for LED0 can be inserted here */
        default:
            break;
        }
        break;

    case LED_CHAR1:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            if (len == sizeof(ls_LED1Val))
            {
                memcpy(ls_LED1Val, pData, sizeof(ls_LED1Val));
                status = SNP_SUCCESS;
                notifyApp = LS_LED1_ID;
            }

            Log_info3("WriteAttrCB : %s connHandle(%d) len(%d)",
                   (IArg)"LED1",
                   (IArg)conn,
                   (IArg)len);
            break;

        /* Other considerations for LED1 can be inserted here */
        default:
            break;
        }
        break;

        case LED_CHAR2:
          switch (SENSOR_ID_CHARTYPE(charID))
          {
          case SENSOR_VALUE:
              if (len == sizeof(ls_LED2Val))
              {
                  memcpy(ls_LED2Val, pData, sizeof(ls_LED2Val));
                  status = SNP_SUCCESS;
                  notifyApp = LS_LED2_ID;
              }

              Log_info3("WriteAttrCB : %s connHandle(%d) len(%d)",
                     (IArg)"LED2",
                     (IArg)conn,
                     (IArg)len);
              break;

          /* Other considerations for LED2 can be inserted here */
          default:
              break;
          }
          break;


      default:
        Log_error0("Attribute was not found.");
        break;
    }

    // If a charactersitic value changed then callback function to notify application of change
    if ((notifyApp != SENSOR_UNKNOWN_CHAR) && pAppCBs
            && pAppCBs->pfnSensorChange)
    {
        pAppCBs->pfnSensorChange(notifyApp);
    }

    return (status);
}

/*******************************************************************************
 * @fn      LED_processSNPEventCB
 *
 * @brief   This is a callback operating in the NPI task. It will be invoked
 *          whenever an event is received from the SNP that this profile has
 *          registered for
 *
 * @param   event  - event mask
 * @param   pValue - pointer event struct
 *
 * @return  status
 */
static void LED_processSNPEventCB(uint16_t event, snpEventParam_t *param)
{
    switch (event)
    {
    case SNP_CONN_EST_EVT:
    {
    }
        break;

    case SNP_CONN_TERM_EVT:
    {
    }
        break;

    default:
        break;
    }
}

