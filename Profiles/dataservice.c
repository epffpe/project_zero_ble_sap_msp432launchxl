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
#include "dataservice.h"

/*******************************************************************************
 *                                   MACROS
 ******************************************************************************/
#define DATA_NUM_ATTR_SUPPORTED 2

/*******************************************************************************
 *                                CONSTANTS
 ******************************************************************************/

/*******************************************************************************
 *                                  TYPEDEFS
 ******************************************************************************/

/*******************************************************************************
 *                                  GLOBAL TYPEDEFS
 ******************************************************************************/

// Data_Service Service UUID
static uint8_t DataServiceUUID[SNP_128BIT_UUID_SIZE] =
 { TI_BASE_UUID_128(DATA_SERVICE_UUID) };

// String UUID
static uint8_t ds_StringUUID[SNP_128BIT_UUID_SIZE] =
 { TI_BASE_UUID_128(DS_STRING_UUID) };

// Stream UUID
static uint8_t ds_StreamUUID[SNP_128BIT_UUID_SIZE] =
 { TI_BASE_UUID_128(DS_STREAM_UUID) };

/*******************************************************************************
 *                             LOCAL VARIABLES
 ******************************************************************************/
static sensorCBs_t *pAppCBs = NULL;

static uint8_t cccdFlag = 0;

static uint16_t connHandle = 0; // Assumes only one connection at a time

/*******************************************************************************
 *                              Profile Attributes - TYPEDEFS
 ******************************************************************************/
static SAP_Service_t    DataService;
static SAP_CharHandle_t DataServiceCharHandles[DATA_NUM_ATTR_SUPPORTED];

// Profile Service attribute
static UUIDType_t DataServiceDecl =
{ SNP_128BIT_UUID_SIZE, DataServiceUUID };

// Characteristic "String" Value variable
static uint8_t ds_StringVal[DS_STRING_LEN] = {0};

// Length of data in characteristic "String" Value variable, initialized to minimal size.
static uint16_t ds_StringValLen = DS_STRING_LEN_MIN;

// Characteristic "Stream" Value variable
static uint8_t ds_StreamVal[DS_STREAM_LEN] = {0};

// Length of data in characteristic "Stream" Value variable, initialized to minimal size.
static uint16_t ds_StreamValLen = DS_STREAM_LEN_MIN;

/*******************************************************************************
 *                              Profile Attributes - TABLE
 ******************************************************************************/

static SAP_UserCCCDAttr_t DataCCCD =
{ SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE };

static SAP_Char_t DataAttrTable[DATA_NUM_ATTR_SUPPORTED] =
{
        // String Characteristic
        {
        { SNP_128BIT_UUID_SIZE, ds_StringUUID }, /* UUID             */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE, /* Properties       */
        SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE, /* Permissions      */
        /* User Description */
        /* CCCD             */
        /* Format           */
        },

        // Stream Characteristic
        {
        { SNP_128BIT_UUID_SIZE, ds_StreamUUID }, /* UUID             */
        SNP_GATT_PROP_NOTIFICATION | SNP_GATT_PROP_WRITE_NORSP, /* Properties       */
        SNP_GATT_PERMIT_READ, /* Permissions      */
        0, /* User Description */
        &DataCCCD /* CCCD             */
        /* Format           */
        }, };

/*******************************************************************************
 *                                  LOCAL FUNCTIONS
 ******************************************************************************/
static void Data_processSNPEventCB(uint16_t event, snpEventParam_t *param);

static bStatus_t DataService_ReadAttrCB(void *context, uint16_t conn,
        uint16_t charHdl, uint16_t offset, uint16_t size, uint16_t * pLen,
        uint8_t *pData);

static bStatus_t DataService_WriteAttrCB(void *context, uint16_t conn,
        uint16_t charHdl, uint16_t len, uint8_t *pData);

static bStatus_t DataService_CCCDIndCB(void *context, uint16_t conn,
        uint16_t cccdHdl, uint8_t type, uint16_t value);

/*******************************************************************************
 *                                 PUBLIC FUNCTIONS
 ******************************************************************************/

/*******************************************************************************
 * @fn      DataService_addService
 *
 * @brief   Initializes the DataService service by registering
 *          GATT attributes with the GATT server.
 *
 * @return  Success or Failure
 */
bStatus_t DataService_addService(void)
{
    // Register to recieve Connection Established Events
    SAP_registerEventCB(Data_processSNPEventCB,
            SNP_CONN_EST_EVT | SNP_CONN_TERM_EVT);

    // Build Service to register with NP
    DataService.serviceUUID = DataServiceDecl;
    DataService.serviceType = SNP_PRIMARY_SERVICE;
    DataService.charTableLen = DATA_NUM_ATTR_SUPPORTED;
    DataService.charTable = DataAttrTable;
    DataService.context = NULL;
    DataService.charReadCallback = DataService_ReadAttrCB;
    DataService.charWriteCallback = DataService_WriteAttrCB;
    DataService.cccdIndCallback = DataService_CCCDIndCB;
    DataService.charAttrHandles = DataServiceCharHandles;

    Log_info1("Registered service, %d attributes", (IArg)DATA_NUM_ATTR_SUPPORTED);

    // Service is set up, register with GATT server on the SNP.
    return SAP_registerService(&DataService);
}

/*******************************************************************************
 * @fn      DataService_registerAppCBs
 *
 * @brief   Registers the application callback function. Only call
 *          this function once.
 *
 * @param   callbacks - pointer to application callbacks.
 *
 * @return  SUCCESS or bleAlreadyInRequestedMode
 */
bStatus_t DataService_registerAppCBs(sensorCBs_t *appCallbacks)
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
 * @fn      DataService_setParameter
 *
 * @brief   Set a DataService parameter.
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
bStatus_t DataService_setParameter( uint8_t param, uint16_t len, void *value )
{
    bStatus_t ret = SUCCESS;
    uint8_t  *pAttrVal;
    uint16_t *pValLen;
    uint16_t valMinLen;
    uint16_t valMaxLen;
    uint8_t   sendNotiInd = FALSE;
    snpNotifIndReq_t localReq;

    switch (SENSOR_ID_CHAR(param))
    {
    case DATA_STRING_CHAR0:
        pAttrVal  =  ds_StringVal;
        pValLen   = &ds_StringValLen;
        valMinLen =  DS_STRING_LEN_MIN;
        valMaxLen =  DS_STRING_LEN;
        Log_info2("SetParameter : %s len: %d", (IArg)"String", (IArg)len);
        break;

    case DATA_STREAM_CHAR1:
        pAttrVal  =  ds_StreamVal;
        pValLen   = &ds_StreamValLen;
        valMinLen =  DS_STREAM_LEN_MIN;
        valMaxLen =  DS_STREAM_LEN;
        sendNotiInd = TRUE;
        localReq.attrHandle = utilGetHdlFromCharID(DS_STREAM_ID,
                        DataServiceCharHandles,
                        DATA_NUM_ATTR_SUPPORTED);
        Log_info2("SetParameter : %s len: %d", (IArg)"Stream", (IArg)len);
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
      *pValLen = len;

      if (sendNotiInd)
      {
        // Initialize Request
        localReq.connHandle = connHandle;
        localReq.pData = pAttrVal;
        localReq.authenticate = 0; // Default no authentication

        Log_info2("Trying to send noti/ind: connHandle %x, %s",
                  (IArg)localReq.connHandle,
                  (IArg)((*((uint8*) value)==0)?"\x1b[33mNoti/ind disabled\x1b[0m" :
                         (*((uint8*) value)==1)?"Notification enabled" :
                                                  "Indication enabled"));
        // Check for whether a notification or indication should be sent.
        // Both flags should never be allowed to be set by NWP
        if (cccdFlag & SNP_GATT_CLIENT_CFG_NOTIFY)
        {
           localReq.type = SNP_SEND_NOTIFICATION;
           SNP_RPC_sendNotifInd(&localReq, sizeof(uint8_t));
         } else if (cccdFlag & SNP_GATT_CLIENT_CFG_INDICATE)
         {
           localReq.type = SNP_SEND_INDICATION;
           SNP_RPC_sendNotifInd(&localReq, sizeof(uint8_t));
         }
      }
    }
    else
    {
      Log_error3("Length outside bounds: Len: %d MinLen: %d MaxLen: %d.", (IArg)len, (IArg)valMinLen, (IArg)valMaxLen);
      ret = bleInvalidRange;
    }

    return ret;
}

/*******************************************************************************
 * @fn      DataService_getParameter
 *
 * @brief   Get a DataService parameter
 *
 * @param   param - Profile parameter IDz
 * @param   value - pointer to data to put.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t DataService_getParameter(uint8_t param, void *value )
{
    bStatus_t ret = SUCCESS;
    switch (SENSOR_ID_CHAR(param))
    {
      case DATA_STRING_CHAR0:
        memcpy(value, ds_StringVal, ds_StringValLen);
        Log_info2("GetParameter : %s returning %d bytes", (IArg)"String",
                  (IArg)ds_StringValLen);
        break;

      case DS_STREAM_ID:
        memcpy(value, ds_StreamVal, ds_StreamValLen);
        Log_info2("GetParameter : %s returning %d bytes", (IArg)"Stream",
                  (IArg)ds_StreamValLen);
        break;

      default:
        Log_error1("GetParameter: Parameter #%d not valid.", (IArg)param);
        ret = INVALIDPARAMETER;
        break;
  }
  return ret;
}

/*********************************************************************
 * @fn      DataService_ReadAttrCB
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
static bStatus_t DataService_ReadAttrCB(void *context, uint16_t connectionHandle,
        uint16_t charHdl, uint16_t offset, uint16_t size, uint16_t * pLen,
        uint8_t *pData)
{
    // Get characteristic from handle
    uint8_t charID = utilGetCharIDFromHdl(charHdl, DataServiceCharHandles,
    DATA_NUM_ATTR_SUPPORTED);
    uint8_t isValid = 0;

    // Update Conn Handle (assumes one connection)
    connHandle = connectionHandle;

    // Find settings for the characteristic to be read.
    switch (SENSOR_ID_CHAR(charID))
    {
      case DATA_STRING_CHAR0:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            *pLen = MIN(size, ds_StringValLen);
            memcpy(pData, ds_StringVal, *pLen);
            isValid = 1;
            Log_info3("ReadAttrCB : %s connHandle: %d offset: %d",
                       (IArg)"String",
                       (IArg)connHandle,
                       (IArg)offset);
            break;

        /* Other considerations for String can be inserted here */
        default:
            break;
        }
        break;

    case DATA_STREAM_CHAR1:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            *pLen = MIN(size, ds_StringValLen);
            memcpy(pData, ds_StreamVal, *pLen);
            isValid = 1;
            Log_info3("ReadAttrCB : %s connHandle: %d offset: %d",
                       (IArg)"Stream",
                       (IArg)connHandle,
                       (IArg)offset);
            break;

        /* Other considerations for Stream can be inserted here */
        default:
          Log_error0("Attribute was not found.");
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
 * @fn      DataService_WriteAttrCB
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
static bStatus_t DataService_WriteAttrCB(void *context, uint16_t connectionHandle,
        uint16_t charHdl, uint16_t len, uint8_t *pData)
{
    bStatus_t status = SNP_UNKNOWN_ATTRIBUTE;
    uint8 notifyApp = SENSOR_UNKNOWN_CHAR;

    // Update Conn Handle (assumes one connection)
    connHandle = connectionHandle;

    // Get characteristic from handle
    uint8_t charID = utilGetCharIDFromHdl(charHdl, DataServiceCharHandles,
    DATA_NUM_ATTR_SUPPORTED);

    // Find settings for the characteristic to be read.
    switch (SENSOR_ID_CHAR(charID))
    {
      case DATA_STRING_CHAR0:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            memcpy(ds_StringVal, pData, len);
            status = SNP_SUCCESS;
            notifyApp = DS_STRING_ID;
            ds_StringValLen = len;
            break;

        /* Other considerations for LED0 can be inserted here */
        default:
            break;
        }
        break;

    case DATA_STREAM_CHAR1:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            memcpy(ds_StreamVal, pData, len);
            status = SNP_SUCCESS;
            notifyApp = DS_STREAM_ID;
            ds_StreamValLen = len;
            break;

        /* Other considerations for LED1 can be inserted here */
        default:
            break;
        }
        break;

      default:
/*          Log_error0("Attribute was not found."); */
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
 * @fn      DataService_CCCDIndCB
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
bStatus_t DataService_CCCDIndCB(void *context, uint16_t connectionHandle,
        uint16_t cccdHdl, uint8_t type, uint16_t value)
{
    bStatus_t status = SNP_UNKNOWN_ATTRIBUTE;
    uint8 notifyApp = SENSOR_UNKNOWN_CHAR;

    // Update Conn Handle (assumes one connection)
    connHandle = connectionHandle;

    // Get characteristic from handle
    uint8_t charID = utilGetCharIDFromHdl(cccdHdl, DataServiceCharHandles,
    DATA_NUM_ATTR_SUPPORTED);

    switch (SENSOR_ID_CHAR(charID))
    {
    case DATA_STREAM_CHAR1:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_CCCD:
            // Set Global cccd Flag which will be used to to gate Indications
            // or Notitfications when SetParameter() is called
            cccdFlag = value;
            notifyApp = charID;
            status = SNP_SUCCESS;
            break;
        default:
            // Should not receive VALUE || USERDESC || FORMAT
            break;
        }
        break;

    default:
        // No other Characteristics have CCCB attributes
        break;
    }

    // If a characteristic value changed then callback function to notify application of change
    if ((notifyApp != SENSOR_UNKNOWN_CHAR) && pAppCBs
            && pAppCBs->pfnSensorCCCD)
    {
        pAppCBs->pfnSensorCCCD(notifyApp, value);
    }

    return (status);
}

/*******************************************************************************
 * @fn      Data_processSNPEventCB
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
static void Data_processSNPEventCB(uint16_t event, snpEventParam_t *param)
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

