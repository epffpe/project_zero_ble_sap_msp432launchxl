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
#ifndef BUTTONSERVICE_H
#define BUTTONSERVICE_H

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
#define BUTTON_CHAR0                0x00
#define BUTTON_CHAR1                0x01
#define BUTTON_CHAR2				0x02

#define BS_BUTTON0_ID               SENSOR_ID_CREATE(BUTTON_CHAR0,SENSOR_VALUE)
#define BS_BUTTON1_ID               SENSOR_ID_CREATE(BUTTON_CHAR1,SENSOR_VALUE)
#define BS_BUTTON2_ID				SENSOR_ID_CREATE(BUTTON_CHAR2,SENSOR_VALUE)

#define BUTTON_SERVICE_UUID         0x1120

// Button Characteristic UUIDs
#define BS_BUTTON0_UUID             0x1121
#define BS_BUTTON1_UUID             0x1122
#define BS_BUTTON2_UUID             0x1123

#define BS_BUTTON0_LEN              1
#define BS_BUTTON1_LEN              1
#define BS_BUTTON2_LEN              1

#define BS_BUTTON0_LEN_MIN          1
#define BS_BUTTON1_LEN_MIN          1
#define BS_BUTTON2_LEN_MIN          1

/*******************************************************************************
 *                                  TYPEDEFS
 ******************************************************************************/
// Fields in characteristic "BUTTON0"
//   Field "Value" format: uint8, bits: 8

// Fields in characteristic "BUTTON1"
//   Field "Value" format: uint8, bits: 8

/*******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************/
/*
 * ButtonService_addService- Initializes the ButtonService service by registering
 *          GATT attributes with the GATT server.
 *
 *    rspTaskId - The ICall Task Id that should receive responses for Indications.
 */
extern bStatus_t ButtonService_addService(void);

/*
 * ButtonService_registerAppCBs - Registers the application callback function.
 *                    Only call this function once.
 *
 *    appCallbacks - pointer to application callbacks.
 */
extern bStatus_t ButtonService_registerAppCBs(sensorCBs_t *appCallbacks);

/*
 * ButtonService_setParameter - Set a ButtonService parameter.
 *
 *    param - Profile parameter ID
 *    len   - length of data to write
 *    value - pointer to data to write.  This is dependent on
 *            the parameter ID and may be cast to the appropriate
 *            data type (example: data type of uint16_t will be cast to
 *            uint16_t pointer).
 */
extern bStatus_t ButtonService_setParameter( uint8_t param, uint16_t len,
        void *value);

/*
 * ButtonService_getParameter - Get a ButtonService parameter.
 *
 *    param - Profile parameter ID
 *    len   - pointer to a variable that contains the maximum length that can be written to *value.
              After the call, this value will contain the actual returned length.
 *    value - pointer to data to write.  This is dependent on
 *            the parameter ID and may be cast to the appropriate
 *            data type (example: data type of uint16_t will be cast to
 *            uint16_t pointer).
 */
extern bStatus_t ButtonService_getParameter( uint8_t param, void *value );

#ifdef __cplusplus
}
#endif

#endif /* BUTTONSERVICE_H */

