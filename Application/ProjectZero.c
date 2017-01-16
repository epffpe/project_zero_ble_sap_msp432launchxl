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
 *                              INCLUDES
 ******************************************************************************/
/* Standard Defines */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* TI Defines */
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Log.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>
#include <ti/sap/sap.h>
#include <ti/sbl/sbl.h>
#include <ti/sbl/sbl_image.h>
#include <driverlib.h>

/* Local Defines */
#include "util.h"
#include "simpleGATTProfile.h"
#include "ProjectZero.h"
#include "Board.h"
#include "LEDservice.h"
#include "buttonservice.h"
#include "dataservice.h"

/*******************************************************************************
 *                            CONSTANTS
 ******************************************************************************/
/* Advertising interval when device is discoverable (units 625us, 160=100ms)) */
#define DEFAULT_ADVERTISING_INTERVAL          160

/* Limited discoverable mode advertises for 30.72s, and then stops
   General discoverable mode advertises indefinitely */
#define DEFAULT_DISCOVERABLE_MODE             SAP_GAP_ADTYPE_FLAGS_GENERAL

/* Minimum connection interval (units of 1.25ms, 80=100ms) if automatic
   parameter update request is enabled */
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL     80

/* Maximum connection interval (units of 1.25ms, 800=1000ms) if automatic
   parameter update request is enabled */
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     800

/**
 * Project Zero States.
 */
typedef enum
{
    PROJECT_ZERO_RESET, PROJECT_ZERO_IDLE, PROJECT_ZERO_START_ADV,
    PROJECT_ZERO_CONNECTED, PROJECT_ZERO_CANCEL_ADV, PROJECT_ZERO_UPDATE_SNP
} ProjectZero_States_t;

/* Task configuration */
#define PROJECT_ZERO_TASK_PRIORITY                     1

#ifndef PROJECT_ZERO_TASK_STACK_SIZE
#define PROJECT_ZERO_TASK_STACK_SIZE                   712
#endif

/* Application Events */
#define PROJECT_ZERO_NONE                Event_Id_NONE   // No Event
#define PROJECT_ZERO_EVT_PUI             Event_Id_00     // Power-Up Indication
#define PROJECT_ZERO_EVT_ADV_ENB         Event_Id_01     // Advertisement Enable
#define PROJECT_ZERO_EVT_ADV_END         Event_Id_02     // Advertisement Ended
#define PROJECT_ZERO_EVT_CONN_EST        Event_Id_03     // Connection Established Event
#define PROJECT_ZERO_EVT_CONN_TERM       Event_Id_04     // Connection Terminated Event
#define PROJECT_ZERO_EVT_BSL_BUTTON      Event_Id_24     // BSL Button - LP S2
#define PROJECT_ZERO_EVT_BUTTON_RESET    Event_Id_25     // RESET
#define PROJECT_ZERO_EVT_BUTTON_RIGHT    Event_Id_30     // RIGHT Button Press - LP S1
#define PROJECT_ZERO_ERROR               Event_Id_31     // Error

/* How often to perform periodic event (in msec) */
#define PROJECT_ZERO_PERIODIC_EVT_PERIOD               5000

#define PROJECT_ZERO_DEFAULT_CONN_HANDLE               0xFFFF

/* Company Identifier: Texas Instruments Inc. (13) */
#define TI_COMPANY_ID                        0x000D
#define TI_ST_DEVICE_ID                      0x03
#define TI_ST_KEY_DATA_ID                    0x00

/* Struct for message about button state */
typedef struct
{
  uint8_t  pinId;
  uint8_t  state;
} button_state_t;

/*******************************************************************************
 *                             LOCAL VARIABLES
 ******************************************************************************/

/* Used to block SNP calls during a synchronous transaction. */
Event_Handle ProjectZeroEvent;

/* Project Zero State Machine */
ProjectZero_States_t projectZeroState;

/* Variable used to start the SNP update process */
bool startSBLTimer = false;

/* Clock objects for debouncing the buttons */
static Clock_Struct button0DebounceClock;
static Clock_Struct button1DebounceClock;

/* Clock object for SNP update */
static Clock_Struct updateSNPClock;

/* State of the buttons */
static uint8_t button0State = 0;
static uint8_t button1State = 0;

/* Task configuration */
Task_Struct ProjectZeroTask;
Char ProjectZeroTaskStack[PROJECT_ZERO_TASK_STACK_SIZE];

Task_Struct updateSNPTask;
Char updateSNPTaskStack[PROJECT_ZERO_TASK_STACK_SIZE];
Semaphore_Handle updateSNPSemHandle;
Error_Block eb;

/* SAP Parameters for opening serial port to SNP */
SAP_Params sapParams;

/* GAP - SCAN RSP data (max size = 31 bytes) */
static uint8_t scanRspData[] =
{
        // complete name
        0x0D,// length of this data
        SAP_GAP_ADTYPE_LOCAL_NAME_COMPLETE, 'P', 'r', 'o', 'j', 'e', 'c', 't', ' ',
        'Z', 'e', 'r', 'o',

        // connection interval range
        0x05,// length of this data
        0x12, //SAP_GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
        LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL), HI_UINT16(
        DEFAULT_DESIRED_MIN_CONN_INTERVAL), LO_UINT16(
        DEFAULT_DESIRED_MAX_CONN_INTERVAL), HI_UINT16(
        DEFAULT_DESIRED_MAX_CONN_INTERVAL),

        // Tx power level
        0x02,// length of this data
        0x0A, //SAP_GAP_ADTYPE_POWER_LEVEL,
        0       // 0dBm
        };

/* SNP Device Name */
uint8_t updateSNPDevName[] = { 'P', 'r', 'o', 'j', 'e', 'c', 't', ' ',
        'Z', 'e', 'r', 'o',};

/* GAP - Advertisement data (max size = 31 bytes, though this is
   best kept short to conserve power while advertisting) */
static uint8_t advertData[] =
{
/* Flags; this sets the device to use limited discoverable
   mode (advertises for 30 seconds at a time) instead of general
   discoverable mode (advertises indefinitely) */
        0x02,// length of this data
        SAP_GAP_ADTYPE_FLAGS,
        DEFAULT_DISCOVERABLE_MODE | SAP_GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

        // Manufacturer specific advertising data
        0x06, 0xFF, //SAP_GAP_ADTYPE_MANUFACTURER_SPECIFIC,
        LO_UINT16(TI_COMPANY_ID), HI_UINT16(TI_COMPANY_ID),
        TI_ST_DEVICE_ID,
        TI_ST_KEY_DATA_ID, 0x00                                    // Key state
        };

/* Connection Handle - only one device currently allowed to connect to SNP */
static uint16_t connHandle = PROJECT_ZERO_DEFAULT_CONN_HANDLE;

/* BD Addr of the NWP */
static char nwpstr[] = "NWP:  0xFFFFFFFFFFFF";
#define nwpstrIDX       8

/* BD Addr of peer device in connection */
static char peerstr[] = "Peer: 0xFFFFFFFFFFFF";
#define peerstrIDX       8

/* Placeholder variable for characteristic intialization */
uint8_t initVal[40] = {0};
uint8_t initString[] = "This is a pretty long string, isn't it!";

/*******************************************************************************
 *                                  LOCAL FUNCTIONS
 ******************************************************************************/

static void ProjectZero_init(void);
static void ProjectZero_taskFxn(UArg a0, UArg a1);
static void ProjectZero_updateSNP_taskFxn(UArg a0, UArg a1);
static void ProjectZero_initServices(void);

static void ProjectZero_asyncCB(uint8_t cmd1, void *pParams);
static void ProjectZero_processSNPEventCB(uint16_t event, snpEventParam_t *param);

static void buttonDebounceSwiFxn(UArg buttonId);
static void user_handleButtonPress(button_state_t *pState);

static void buttonCallbackFxnBUTTON0(void);
static void buttonCallbackFxnBUTTON1(void);

static void updateSNPSwiFxn(UArg paramId);

static void ProjectZero_processLEDServiceCB(uint8_t charID);
static void ProjectZero_processLEDServicecccdCB(uint8_t charID, uint16_t value);

static void ProjectZero_processButtonServiceCB(uint8_t charID);
static void ProjectZero_processButtonServicecccdCB(uint8_t charID, uint16_t value);

static void ProjectZero_processDataServiceCB(uint8_t charID);
static void ProjectZero_processDataServicecccdCB(uint8_t charID, uint16_t value);

/*******************************************************************************
 *                                 PROFILE CALLBACKS
 ******************************************************************************/
/*
 * LED Characteristic value change callback
 */
static sensorCBs_t ProjectZero_LEDServiceCBs =
{ ProjectZero_processLEDServiceCB, ProjectZero_processLEDServicecccdCB };

/*
 * Button Characteristic value change callback
 */
static sensorCBs_t ProjectZero_ButtonServiceCBs =
{ ProjectZero_processButtonServiceCB, ProjectZero_processButtonServicecccdCB };

/*
 * Data Characteristic value change callback
 */
static sensorCBs_t ProjectZero_DataServiceCBs =
{ ProjectZero_processDataServiceCB, ProjectZero_processDataServicecccdCB };

/*******************************************************************************
 *                                 PUBLIC FUNCTIONS
 ******************************************************************************/

/*******************************************************************************
 * @fn      ProjectZero_convertBdAddr2Str
 *
 * @brief   Convert Bluetooth address to string. Only needed when
 *          LCD display is used.
 *
 * @param   pAddr - BD address
 *
 * @return  BD address as a string
 ******************************************************************************/
static void ProjectZero_convertBdAddr2Str(char *str, uint8_t *pAddr)
{
    uint8_t charCnt;
    char hex[] = "0123456789ABCDEF";

    // Start from end of addr
    pAddr += B_ADDR_LEN;

    for (charCnt = B_ADDR_LEN; charCnt > 0; charCnt--)
    {
        *str++ = hex[*--pAddr >> 4];
        *str++ = hex[*pAddr & 0x0F];
    }

    return;
}

/*******************************************************************************
 * @fn      ProjectZero_createTask
 *
 * @brief   Task creation function for the Simple BLE Peripheral.
 *
 * @param   None.
 *
 * @return  None.
 ******************************************************************************/
void ProjectZero_createTask(void)
{
    Task_Params taskParams;

    // Configure task
    Task_Params_init(&taskParams);
    taskParams.stack = ProjectZeroTaskStack;
    taskParams.stackSize = PROJECT_ZERO_TASK_STACK_SIZE;
    taskParams.priority = PROJECT_ZERO_TASK_PRIORITY;

    Task_construct(&ProjectZeroTask, ProjectZero_taskFxn, &taskParams, NULL);
}

/*******************************************************************************
 * @fn      updateSNP_createTask
 *
 * @brief   Task creation function to update the SNP.
 *
 * @param   None.
 *
 * @return  None.
 ******************************************************************************/
void updateSNP_createTask(void)
{
    Task_Params taskParams;

    // Configure task
    Task_Params_init(&taskParams);
    taskParams.stack = updateSNPTaskStack;
    taskParams.stackSize = PROJECT_ZERO_TASK_STACK_SIZE;
    taskParams.priority = PROJECT_ZERO_TASK_PRIORITY;

    Task_construct(&updateSNPTask, ProjectZero_updateSNP_taskFxn, &taskParams, NULL);
}

/*******************************************************************************
 * @fn      ProjectZero_init
 *
 * @brief   Called during initialization and contains application
 *          specific initialization (ie. hardware initialization/setup,
 *          table initialization, power up notification, etc), and
 *          profile initialization/setup.
 *
 * @param   None.
 *
 * @return  None.
 ******************************************************************************/
static void ProjectZero_init(void)
{
    /* Create RTOS Event */
    ProjectZeroEvent = Event_create(NULL, NULL);

    // Register Key Handler */
    GPIO_setCallback(Board_BUTTON0, (GPIO_CallbackFxn) buttonCallbackFxnBUTTON0);
    GPIO_enableInt(Board_BUTTON0);

    GPIO_setCallback(Board_BUTTON1, (GPIO_CallbackFxn) buttonCallbackFxnBUTTON1);
    GPIO_enableInt(Board_BUTTON1);

    Log_info0("Initializing the user task, hardware, BLE stack and services.");

    /* Semaphore used to start the SNP update */
    updateSNPSemHandle = Semaphore_create(0, NULL, &eb);
    if(updateSNPSemHandle == NULL)
    {
        System_abort("semaphore create failed");
    }

    /* Register to receive notifications from LED service if characteristics 
       have been written to */
    LEDService_registerAppCBs(&ProjectZero_LEDServiceCBs);

    /* Register to receive notifications from Button service if characteristics
       have been written to */
    ButtonService_registerAppCBs(&ProjectZero_ButtonServiceCBs);

    /* Register to receive notifications from Data service if characteristics
       have been written to */
    DataService_registerAppCBs(&ProjectZero_DataServiceCBs);

    /* Create the debounce clock objects for Button 0 and Button 1 */
    Clock_Params clockParams;
    Clock_Params_init(&clockParams);

    /* Both clock objects use the same callback, so differentiate on argument 
       given to the callback in Swi context */
    clockParams.arg = Board_BUTTON0;
    GPIO_setConfig(Board_BUTTON0, (GPIO_PinConfig)(GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING) );

    /* Initialize to 50 ms timeout when Clock_start is called.
       Timeout argument is in ticks, so convert from ms to ticks via 
       tickPeriod. */
    Clock_construct(&button0DebounceClock, buttonDebounceSwiFxn,
                    50 * (1000/Clock_tickPeriod),
                    &clockParams);

    // Second button
    clockParams.arg = Board_BUTTON1;
    GPIO_setConfig(Board_BUTTON1, (GPIO_PinConfig)(GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING) );
    Clock_construct(&button1DebounceClock, buttonDebounceSwiFxn,
                    50 * (1000/Clock_tickPeriod),
                    &clockParams);

    // Initialize to 3 seconds timeout when Clock_start is called.
    Clock_construct(&updateSNPClock, updateSNPSwiFxn,
                    3000 * (1000/Clock_tickPeriod),
                    &clockParams);
}

/*******************************************************************************
 * @fn      ProjectZero_updateSNP_taskFxn
 *
 * @brief   Application task entry point fto update the SNP.
 *
 * @param   a0, a1 - not used.
 *
 * @return  None.
 ******************************************************************************/
static void ProjectZero_updateSNP_taskFxn(UArg a0, UArg a1)
{
    SBL_Params params;
    SBL_Image image;
    uint8 sblStatus;

    for (;;)
    {
        if (Semaphore_pend(updateSNPSemHandle,BIOS_WAIT_FOREVER))
        {
            /* Close NP so SBL can use serial port */
            SAP_close();

            // Initalize SBL Params and open port to target device
            SBL_initParams(&params);
            params.targetInterface = SBL_DEV_INTERFACE_UART;
            params.localInterfaceID = Board_UART1;
            params.resetPinID = Board_DP0;
            params.blPinID = Board_DP2;

            // If SBL cannot be opened the process cannot proceed
            if((sblStatus = SBL_open(&params)) == SBL_SUCCESS)
            {
                // Reset target and force into SBL code
                SBL_openTarget();

                Log_info0("\n Programming the CC26xx... ");

                // Notify user that we are updating the SNP
                GPIO_write(Board_LED0, Board_LED_OFF);

                // Turn ON all LEDs
                GPIO_write(Board_LED1, Board_LED_ON);
                GPIO_write(Board_LED2, Board_LED_ON);
                GPIO_write(Board_LED3, Board_LED_ON);

                // Flash new image to target
                image.imgType = SBL_IMAGE_TYPE_INT;
                image.imgInfoLocAddr = (uint32_t)&SNP_Code[0];
                image.imgLocAddr = (uint32_t)&SNP_Code[0];
                image.imgTargetAddr = SNP_IMAGE_START;
                sblStatus = SBL_writeImage(&image);

                // Reset target and exit SBL code
                SBL_closeTarget();

                // Close SBL port to target device
                SBL_close();
            }

            if (sblStatus != SBL_SUCCESS)
            {
                Log_info0("Programming failed!\r\n");
            } else
            {
                Log_info0("Programming passed!\r\n");
            }

            Log_info0("Resetting device.\r\n");

            Task_sleep(100);

            // Regardless of successful write we must restart the SNP
            // Force Reset
            MAP_SysCtl_rebootDevice();
        }
    }
}

/*******************************************************************************
 * @fn      ProjectZero_taskFxn
 *
 * @brief   Application task entry point for the Simple BLE Peripheral.
 *
 * @param   a0, a1 - not used.
 *
 * @return  None.
 ******************************************************************************/
static void ProjectZero_taskFxn(UArg a0, UArg a1)
{
    UInt events;
    uint8 enableAdv = 1;
    uint8 disableAdv = 0;

    /* Initialize State Machine */
    projectZeroState = PROJECT_ZERO_RESET;

    /* Initialize application */
    ProjectZero_init();

    /* Application main loop */
    for (;;)
    {
        switch (projectZeroState)
        {
        case PROJECT_ZERO_RESET:
        {
            /* Make sure CC26xx is not in BSL */
            GPIO_write(Board_DP0, Board_LED_OFF); // MSP_EXP432P401R_RESET
            GPIO_write(Board_DP2, Board_LED_ON);  // MSP_EXP432P401R_BL

            Task_sleep(10); // 10ms delay
            GPIO_write(Board_DP0, Board_LED_ON); // MSP_EXP432P401R_RESET

            /* Initialize UART port parameters within SAP parameters */
            SAP_initParams(SAP_PORT_REMOTE_UART, &sapParams);

            sapParams.port.remote.mrdyPinID = Board_MRDY;
            sapParams.port.remote.srdyPinID = Board_SRDY;
            sapParams.port.remote.boardID = Board_UART1;

            // Setup NP module
            SAP_open(&sapParams);

            /* Register Application thread's callback to receive asynchronous 
                requests from the NP. */
            SAP_setAsyncCB(ProjectZero_asyncCB);

            /* Reset the NP, and await a powerup indication.
               Clear any pending power indications received prior to this reset
               call */
            Event_pend(ProjectZeroEvent, PROJECT_ZERO_NONE,
                       PROJECT_ZERO_EVT_PUI, 1000);
            SAP_reset();

            events = Event_pend(ProjectZeroEvent, PROJECT_ZERO_NONE,
            PROJECT_ZERO_EVT_PUI + PROJECT_ZERO_EVT_BSL_BUTTON, BIOS_WAIT_FOREVER);

            if (events & PROJECT_ZERO_EVT_BSL_BUTTON)
            {
            } 
            else
            {
                /* Read BD ADDR */
                SAP_setParam(SAP_PARAM_HCI, SNP_HCI_OPCODE_READ_BDADDR, 0,
                        NULL);

                /* Setup Services - Service creation is blocking so no need
                   to pend */
                ProjectZero_initServices();

                projectZeroState = PROJECT_ZERO_START_ADV;
            }
        }
            break;

        case PROJECT_ZERO_START_ADV:
        {
            /* Turn on user LED to indicate advertising */
            GPIO_write(Board_LED0, Board_LED_ON);

            /* Set advertising data. */
            SAP_setServiceParam(SNP_GGS_SERV_ID, SNP_GGS_DEVICE_NAME_ATT,
                                   sizeof(updateSNPDevName), updateSNPDevName);

            SAP_setParam(SAP_PARAM_ADV, SAP_ADV_DATA_NOTCONN,
                    sizeof(advertData), advertData);

            /* Set Scan Response data. */
            SAP_setParam(SAP_PARAM_ADV, SAP_ADV_DATA_SCANRSP,
                    sizeof(scanRspData), scanRspData);

            /* Enable Advertising and await NP response */
            SAP_setParam(SAP_PARAM_ADV, SAP_ADV_STATE, 1, &enableAdv);
            Event_pend(ProjectZeroEvent, PROJECT_ZERO_NONE,
                       PROJECT_ZERO_EVT_ADV_ENB, BIOS_WAIT_FOREVER);

            /* Wait for connection or button press to cancel advertisement */
            events = Event_pend(ProjectZeroEvent, PROJECT_ZERO_EVT_ADV_END,
                                PROJECT_ZERO_EVT_CONN_EST,
                                30000);

            projectZeroState = (events & PROJECT_ZERO_EVT_CONN_EST) ?
                     PROJECT_ZERO_CONNECTED : PROJECT_ZERO_CANCEL_ADV;
        }
            break;

        case PROJECT_ZERO_CONNECTED:
            /* Before connecting, NP will send the stop ADV meesage */
            Event_pend(ProjectZeroEvent, PROJECT_ZERO_NONE,
                       PROJECT_ZERO_EVT_ADV_END, BIOS_WAIT_FOREVER);

            /* Events that can happen during connection - Client Disconnection
                                                        - AP Disconnection */
            events = Event_pend(ProjectZeroEvent, PROJECT_ZERO_NONE,
                                PROJECT_ZERO_EVT_CONN_TERM, BIOS_WAIT_FOREVER);

            if (events & PROJECT_ZERO_EVT_CONN_TERM)
            {
                /* Client has disconnected from server */
                SAP_setParam(SAP_PARAM_CONN, SAP_CONN_STATE, sizeof(connHandle),
                        (uint8_t *) &connHandle);
                Event_pend(ProjectZeroEvent, PROJECT_ZERO_NONE,
                           PROJECT_ZERO_EVT_CONN_TERM, BIOS_WAIT_FOREVER);

                projectZeroState = PROJECT_ZERO_CANCEL_ADV;
            }
            break;

        case PROJECT_ZERO_CANCEL_ADV:
            /* Cancel Advertisement */
            SAP_setParam(SAP_PARAM_ADV, SAP_ADV_STATE, 1, &disableAdv);
            Event_pend(ProjectZeroEvent, PROJECT_ZERO_NONE,
                       PROJECT_ZERO_EVT_ADV_END, BIOS_WAIT_FOREVER);

            projectZeroState = PROJECT_ZERO_IDLE;
            break;

        case PROJECT_ZERO_IDLE:
            // Turn off user LED to indicate stop advertising
            GPIO_write(Board_LED0, Board_LED_OFF);
            projectZeroState = PROJECT_ZERO_START_ADV;
            break;

        default:
            break;
        }
    }
}

/*******************************************************************************
 * @fn      user_handleButtonPress
 *
 * @brief   Handle a debounced button press or release in Task context.
 *          Invoked by the taskFxn based on a message received from a callback.
 *
 * @param   pState  pointer to button_state_t message sent from debounce Swi.
 *
 * @return  None.
 ******************************************************************************/
static void user_handleButtonPress(button_state_t *pState)
{
  Log_info2("%s %s",
    (IArg)(pState->pinId == Board_BUTTON0?"Button 0":"Button 1"),
    (IArg)(pState->state?"\x1b[32mpressed\x1b[0m":
                         "\x1b[33mreleased\x1b[0m"));

  /* Update the service with the new value.
     Will automatically send notification/indication if enabled. */
  switch (pState->pinId)
  {
    case Board_BUTTON0:
      ButtonService_setParameter(BS_BUTTON0_ID,
                                 sizeof(pState->state),
                                 &pState->state);
      break;
    case Board_BUTTON1:
      ButtonService_setParameter(BS_BUTTON1_ID,
                                 sizeof(pState->state),
                                 &pState->state);
      break;
  }
}

/*******************************************************************************
 * @fn      ProjectZero_initServices
 *
 * @brief   Configure SNP and register services.
 *
 * @param   None.
 *
 * @return  None.
 ******************************************************************************/
static void ProjectZero_initServices(void)
{
    //////////////////////// SNP Service Addition and Registry! ////////////////////////
    // Setup the SimpleProfile Characteristic Values
    {
    }

    // Initalization of characteristics in LED_Service that can provide data
    LEDService_setParameter(LS_LED0_ID, LS_LED0_LEN, initVal);
    LEDService_setParameter(LS_LED1_ID, LS_LED1_LEN, initVal);
    LEDService_addService();

    // Initalization of characteristics in Button_Service that can provide data
    ButtonService_setParameter(BS_BUTTON0_ID, BS_BUTTON0_LEN, initVal);
    ButtonService_setParameter(BS_BUTTON1_ID, BS_BUTTON1_LEN, initVal);
    ButtonService_addService();

    // Initalization of characteristics in Button_Service that can provide data
    DataService_setParameter(DS_STRING_ID, sizeof(initString), initString);
    DataService_setParameter(DS_STREAM_ID, DS_STREAM_LEN, initVal);
    DataService_addService();

    ////////////////////////      Set event callback      ////////////////////////
    SAP_registerEventCB(ProjectZero_processSNPEventCB, 0xFFFF);
}

/*
 * This is a callback operating in the NPI task.
 * These are events this application has registered for.
 *
 */
static void ProjectZero_processSNPEventCB(uint16_t event, snpEventParam_t *param)
{
    switch (event)
    {
    case SNP_CONN_EST_EVT:
    {
        snpConnEstEvt_t * connEstEvt = (snpConnEstEvt_t *) param;

        // Update Peer Addr String
        connHandle = connEstEvt->connHandle;
        ProjectZero_convertBdAddr2Str(&peerstr[peerstrIDX], connEstEvt->pAddr);

        // Notify state machine of established connection
        Event_post(ProjectZeroEvent, PROJECT_ZERO_EVT_CONN_EST);
    }
        break;

    case SNP_CONN_TERM_EVT:
    {
        connHandle = PROJECT_ZERO_DEFAULT_CONN_HANDLE;
        // Notify state machine of disconnection event
        Event_post(ProjectZeroEvent, PROJECT_ZERO_EVT_CONN_TERM);
    }
        break;

    case SNP_ADV_STARTED_EVT:
    {
        snpAdvStatusEvt_t *advEvt = (snpAdvStatusEvt_t *) param;
        if (advEvt->status == SNP_SUCCESS)
        {
            // Notify state machine of Advertisement Enabled
            Event_post(ProjectZeroEvent, PROJECT_ZERO_EVT_ADV_ENB);
        } else
        {
            Event_post(ProjectZeroEvent, PROJECT_ZERO_ERROR);
        }
    }
        break;

    case SNP_ADV_ENDED_EVT:
    {
        snpAdvStatusEvt_t *advEvt = (snpAdvStatusEvt_t *) param;
        if (advEvt->status == SNP_SUCCESS)
        {
            // Notify state machine of Advertisement Disabled
            Event_post(ProjectZeroEvent, PROJECT_ZERO_EVT_ADV_END);
        }
    }
        break;

    default:
        break;
    }
}

/*
 * This is a callback operating in the NPI task.
 * These are Asynchronous indications.
 *
 */
static void ProjectZero_asyncCB(uint8_t cmd1, void *pParams)
{
    switch (SNP_GET_OPCODE_HDR_CMD1(cmd1))
    {
    case SNP_DEVICE_GRP:
    {
        switch (cmd1)
        {
        case SNP_POWER_UP_IND:
            // Notify state machine of Power Up Indication
            Event_post(ProjectZeroEvent, PROJECT_ZERO_EVT_PUI);
            break;

        case SNP_HCI_CMD_RSP:
        {
            snpHciCmdRsp_t *hciRsp = (snpHciCmdRsp_t *) pParams;
            switch (hciRsp->opcode)
            {
            case SNP_HCI_OPCODE_READ_BDADDR:
                // Update NWP Addr String
                ProjectZero_convertBdAddr2Str(&nwpstr[nwpstrIDX], hciRsp->pData);
                break;
            default:
                break;
            }
        }
            break;

        case SNP_EVENT_IND:
            // Notify state machine of Advertisement Enabled
            Event_post(ProjectZeroEvent, PROJECT_ZERO_EVT_ADV_ENB);
            break;

        default:
            break;
        }
    }
        break;

    default:
        break;
    }
}

/*******************************************************************************
 * @fn      buttonDebounceSwiFxn
 *
 * @brief   Callback from Clock module on timeout, determines new state
 *          after debouncing
 *
 * @param   buttonId    The pin being debounced
 *
 * @return  none
 ******************************************************************************/
static void buttonDebounceSwiFxn(UArg buttonId)
{
  // Used to send message to app
  button_state_t buttonMsg = { .pinId = buttonId };
  uint8_t        sendMsg   = FALSE;

  // Get current value of the button pin after the clock timeout
  uint8_t buttonPinVal = GPIO_read(buttonId);

  // Set interrupt direction to opposite of debounced state
  // If button is now released (button is active low, so release is high)
  if (buttonPinVal)
  {
    // Enable negative edge interrupts to wait for press
    GPIO_setConfig(buttonId, (GPIO_PinConfig)(GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING) );
  }
  else
  {
    // Enable positive edge interrupts to wait for relesae
    GPIO_setConfig(buttonId, (GPIO_PinConfig)(GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_RISING) );
  }

  switch(buttonId)
  {
    case Board_BUTTON0:
      // If button is now released (buttonPinVal is active low, so release is 1)
      // and button state was pressed (buttonstate is active high so press is 1)
      if (buttonPinVal && button0State)
      {
        // Button was released
        buttonMsg.state = button0State = 0;
        sendMsg = TRUE;
      }
      else if (!buttonPinVal && !button0State)
      {
        // Button was pressed
        buttonMsg.state = button0State = 1;
        sendMsg = TRUE;
      }
      break;

    case Board_BUTTON1:
      // If button is now released (buttonPinVal is active low, so release is 1)
      // and button state was pressed (buttonstate is active high so press is 1)
      if (buttonPinVal && button1State)
      {
        // Button was released
        buttonMsg.state = button1State = 0;
        sendMsg = TRUE;
      }
      else if (!buttonPinVal && !button1State)
      {
        // Button was pressed
        buttonMsg.state = button1State = 1;
        sendMsg = TRUE;
      }
      break;
  }

  if ((projectZeroState != PROJECT_ZERO_CONNECTED) &&
          (button0State && button1State))
  {
      if (!startSBLTimer)
      {
          startSBLTimer = true;

          // Start 6 seconds timer
          Clock_start(Clock_handle(&updateSNPClock));
      }
  }
  else
  {
      // Stop timer
      startSBLTimer = false;
      Clock_stop(Clock_handle(&updateSNPClock));
  }

  /* Enable interrupt on that pin */
  GPIO_enableInt(buttonId);

  if (sendMsg == TRUE)
  {
    user_handleButtonPress(&buttonMsg);
  }
}

/*******************************************************************************
 * @fn      updateSNPSwiFxn
 *
 * @brief   Callback from Clock module on timeout, starts the SNP Update
 *
 * @param   paramId - not used.
 *
 * @return  none
 ******************************************************************************/
static void updateSNPSwiFxn(UArg paramId)
{
    // Change to UPDATE_SNP
    projectZeroState = PROJECT_ZERO_UPDATE_SNP;

    // Disable buttons interrupts while we update the SNP
    GPIO_disableInt(Board_BUTTON0);
    GPIO_disableInt(Board_BUTTON1);

    // Deactivate Application task
    Task_setPri(Task_handle(&ProjectZeroTask), -1);

    // Post to start the update task
    Semaphore_post(updateSNPSemHandle);
}

/*******************************************************************************
 * @fn      buttonCallbackFxnBUTTON0
 *
 * @brief   Callback from GPIO driver on interrupt
 *
 * @param   none
 *
 * @return  none
 ******************************************************************************/
void buttonCallbackFxnBUTTON0(void)
{
    Log_info0("Button interrupt: Button 0");

    /*  Disable interrupt on the pin */
    GPIO_disableInt (Board_BUTTON0);
    GPIO_clearInt(Board_BUTTON0);
    Clock_start(Clock_handle(&button0DebounceClock));
}

/*******************************************************************************
 * @fn      buttonCallbackFxnBUTTON1
 *
* @brief    Callback from GPIO driver on interrupt
 *
 * @param   none
 *
 * @return  none
 ******************************************************************************/
void buttonCallbackFxnBUTTON1(void)
{
    Log_info0("Button interrupt: Button 1");

    /*  Disable interrupt on the pin */
    GPIO_disableInt (Board_BUTTON1);
    GPIO_clearInt(Board_BUTTON1);
    Clock_start(Clock_handle(&button1DebounceClock));
}

/*
 * Callbacks for the LED Service
 */
static void ProjectZero_processLEDServiceCB(uint8_t charID)
{
  uint8_t value = 0;

    switch (SENSOR_ID_CHAR(charID))
    {
    case LED_CHAR0:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            LEDService_getParameter(charID, &value);
            if (value)
            {
              GPIO_write(Board_LED1, Board_LED_ON);
            }
            else
            {
              GPIO_write(Board_LED1, Board_LED_OFF);
            }

            break;

        default:
            // Should not receive other types
            break;
        }
        break;
    case LED_CHAR1:
        switch (SENSOR_ID_CHARTYPE(charID))
        {
        case SENSOR_VALUE:
            LEDService_getParameter(charID, &value);
            if (value)
            {
              GPIO_write(Board_LED2, Board_LED_ON);
            }
            else
            {
              GPIO_write(Board_LED2, Board_LED_OFF);
            }
            break;

        default:
            // Should not receive other types
            break;
        }
        break;
    default:
        // Other Characteristics not writable
        break;
    }
}

static void ProjectZero_processLEDServicecccdCB(uint8_t charID, uint16_t value)
{
  charID = charID;
  value = value;
}

/*
 * Callbacks for the Button Service
 */
static void ProjectZero_processButtonServiceCB(uint8_t charID)
{
  charID = charID;
}

static void ProjectZero_processButtonServicecccdCB(uint8_t charID, uint16_t value)
{
  charID = charID;
  value = value;
}

/*
 * Callbacks for the Data Service
 */
static void ProjectZero_processDataServiceCB(uint8_t charID)
{
  charID = charID;
}

static void ProjectZero_processDataServicecccdCB(uint8_t charID, uint16_t value)
{
  charID = charID;
  value = value;
}
