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
#ifndef DATA_SERVICE_H
#define DATA_SERVICE_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 *                                INCLUDES
 ******************************************************************************/
#include "st_util.h"

/*******************************************************************************
 *                                CONSTANTS
 ******************************************************************************/

// Characteristic Types - These must be listed in order that they appear
//                        in service
#define DATA_STRING_CHAR0           0x00
#define DATA_STREAM_CHAR1           0x01

#define DS_STRING_ID                SENSOR_ID_CREATE(DATA_STRING_CHAR0,SENSOR_VALUE)
#define DS_STREAM_ID                SENSOR_ID_CREATE(DATA_STREAM_CHAR1,SENSOR_VALUE)

// Data Service UUID
#define DATA_SERVICE_UUID           0x1130

// Data Characteristic UUIDs
#define DS_STRING_UUID              0x1131
#define DS_STREAM_UUID              0x1132

#define DS_STRING_LEN               40
#define DS_STREAM_LEN               20

#define DS_STRING_LEN_MIN           0
#define DS_STREAM_LEN_MIN           0

/*******************************************************************************
 *                                  TYPEDEFS
 ******************************************************************************/
// Fields in characteristic "String"
//   Field "String" format: uint8[], bits: -1

// Fields in characteristic "Stream"
//   Field "Stream" format: , bits: -1

/*******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************/
/*
 * DataService_addService- Initializes the DataService service by registering
 *          GATT attributes with the GATT server.
 *
 *    rspTaskId - The ICall Task Id that should receive responses for Indications.
 */
extern bStatus_t DataService_addService(void);

/*
 * DataService_registerAppCBs - Registers the application callback function.
 *                    Only call this function once.
 *
 *    appCallbacks - pointer to application callbacks.
 */
extern bStatus_t DataService_registerAppCBs(sensorCBs_t *appCallbacks);

/*
 * DataService_setParameter - Set a DataService parameter.
 *
 *    param - Profile parameter ID
 *    len   - length of data to write
 *    value - pointer to data to write.  This is dependent on
 *            the parameter ID and may be cast to the appropriate
 *            data type (example: data type of uint16_t will be cast to
 *            uint16_t pointer).
 */
extern bStatus_t DataService_setParameter( uint8_t param, uint16_t len,
        void *value);

/*
 * DataService_getParameter - Get a DataService parameter.
 *
 *    param - Profile parameter ID
 *    len   - pointer to a variable that contains the maximum length that can be written to *value.
              After the call, this value will contain the actual returned length.
 *    value - pointer to data to write.  This is dependent on
 *            the parameter ID and may be cast to the appropriate
 *            data type (example: data type of uint16_t will be cast to
 *            uint16_t pointer).
 */
extern bStatus_t DataService_getParameter( uint8_t param, void *value );

#ifdef __cplusplus
}
#endif

#endif /* DATASERVICE_H */

