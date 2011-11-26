/* -*- Mode: c; c-basic-offset: 2; tab-width: 2; indent-tabs-mode: t -*- */
/**
******************************************************************************
* @addtogroup OpenPilotModules OpenPilot Modules
* @{
* @addtogroup TransmitterControls Copter Control TransmitterControls Estimation
* @brief Acquires sensor data and computes attitude estimate
* Specifically updates the the @ref TransmitterControlsActual "TransmitterControlsActual" and @ref TransmitterControlsRaw "TransmitterControlsRaw" settings objects
* @{
*
* @file				radio.c
* @author			The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief			Module to handle all comms to the AHRS on a periodic basis.
*
* @see				The GNU Public License (GPL) Version 3
*
******************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/**
 * Input objects: None, takes sensor data via pios
 * Output objects: @ref TransmitterControlsRaw @ref TransmitterControlsActual
 *
 * This module computes an attitude estimate from the sensor data
 *
 * The module executes in its own thread.
 *
 * UAVObjects are automatically generated by the UAVObjectGenerator from
 * the object definition XML file.
 *
 * Modules have no API, all communication to other modules is done through UAVObjects.
 * However modules may use the API exposed by shared libraries.
 * See the OpenPilot wiki for more details.
 * http://www.openpilot.org/OpenPilot_Application_Architecture
 *
 */

#include "pios.h"
#include "manualcontrolcommand.h"
#include "transmittercontrols.h"
#include "manualcontrolsettings.h"
#include "gcsreceiver.h"

// Private constants
#define STACK_SIZE_BYTES 540
//#define STACK_SIZE_BYTES 320
#define TASK_PRIORITY (tskIDLE_PRIORITY+3)

#define UPDATE_RATE	 2.0f

#define REQ_TIMEOUT_MS 250
#define MAX_RETRIES 2

#define STATS_UPDATE_PERIOD_MS 10

// Private types
struct RouterCommsStruct {
	uint8_t num;
	uint32_t port;
	xQueueHandle txqueue;
	xSemaphoreHandle sem;
	UAVTalkConnection com;
	xTaskHandle txTaskHandle;
	xTaskHandle rxTaskHandle;
	struct RouterCommsStruct *relay_comm;
};
typedef struct RouterCommsStruct RouterComms;
typedef RouterComms *RouterCommsHandle;

// Private variables
static xTaskHandle taskHandle;
static xQueueHandle adc_queue;
static uint32_t txErrors;
static uint32_t txRetries;
static RouterComms comms[2];

// Private functions
static void transmitterControlsTask(void *parameters);
static void transmitterTxTask(void *parameters);
static void transmitterRxTask(void *parameters);
//static void processObjEvent(UAVObjEvent * ev);
static int32_t transmitData1(uint8_t * data, int32_t length);
static int32_t transmitData2(uint8_t * data, int32_t length);


/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t TransmitterControlsStart(void) {

	// Start main task
	xTaskCreate(transmitterControlsTask, (signed char *)"TransmitterControls", STACK_SIZE_BYTES/4, NULL, TASK_PRIORITY, &taskHandle);
	//TaskMonitorAdd(TASKINFO_RUNNING_TRANSMITTERCONTROLS, taskHandle);
	PIOS_WDG_RegisterFlag(PIOS_WDG_ATTITUDE);

	// Start the Tx and Rx tasks
	xTaskCreate(transmitterTxTask, (signed char *)"TransmitterTx1", STACK_SIZE_BYTES/4, (void*)comms, TASK_PRIORITY, &(comms[0].txTaskHandle));
	//TaskMonitorAdd(TASKINFO_RUNNING_TRANSMITTERTX, txTaskHandle);
	xTaskCreate(transmitterRxTask, (signed char *)"TransmitterRx1", STACK_SIZE_BYTES/4, (void*)comms, TASK_PRIORITY, &(comms[0].rxTaskHandle));
	//TaskMonitorAdd(TASKINFO_RUNNING_TRANSMITTERRX, rxTaskHandle);
	xTaskCreate(transmitterTxTask, (signed char *)"TransmitterTx2", STACK_SIZE_BYTES/4, (void*)(comms + 1), TASK_PRIORITY, &(comms[1].txTaskHandle));
	//TaskMonitorAdd(TASKINFO_RUNNING_TRANSMITTERTX, txTaskHandle);
	xTaskCreate(transmitterRxTask, (signed char *)"TransmitterRx2", STACK_SIZE_BYTES/4, (void*)(comms + 1), TASK_PRIORITY, &(comms[1].rxTaskHandle));
	//TaskMonitorAdd(TASKINFO_RUNNING_TRANSMITTERRX, rxTaskHandle);

	return 0;
}

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t TransmitterControlsInitialize(void) {

	// Initialize the GCSReceiver object.
	GCSReceiverInitialize();

	// Set the comm number
	comms[0].num = 0;
	comms[1].num = 1;

	// Initialize the telemetry ports
	comms[0].port = PIOS_COM_TELEM_GCS;
	comms[1].port = PIOS_COM_TELEM_OUT;

	// Create the semaphores
	comms[0].sem = xSemaphoreCreateRecursiveMutex();
	comms[1].sem = xSemaphoreCreateRecursiveMutex();

	// Point each comm to it's relay comm
	comms[0].relay_comm = comms + 1;
	comms[1].relay_comm = comms;

	// Create object queues
	comms[0].txqueue = xQueueCreate(TELEM_QUEUE_SIZE, sizeof(UAVObjEvent));
	comms[1].txqueue = xQueueCreate(TELEM_QUEUE_SIZE, sizeof(UAVObjEvent));

	// Initialise UAVTalk
	comms[0].com = UAVTalkInitialize(&transmitData1, 256);
	comms[1].com = UAVTalkInitialize(&transmitData2, 256);

	//TransmitterControlsSettingsConnectCallback(&settingsUpdatedCb);

	// Create periodic event that will be used to send transmitter state.
	UAVObjEvent ev;
	memset(&ev, 0, sizeof(UAVObjEvent));
	EventPeriodicQueueCreate(&ev, comms[1].txqueue, STATS_UPDATE_PERIOD_MS);

	// Create queue for reading ADC values.
	adc_queue = xQueueCreate(1, sizeof(float) * 7);
	if(adc_queue == NULL)
		return -1;
	PIOS_ADC_SetQueue(adc_queue);

	/*
	// Add object for periodic updates
	ev.obj = GCSReceiverHandle();
	ev.instId = UAVOBJ_ALL_INSTANCES;
	ev.event = EV_UPDATED_MANUAL;
	return EventPeriodicQueueCreate(&ev, comms[1].txqueue, 0);
	*/

	return 0;
}

MODULE_INITCALL(TransmitterControlsInitialize, TransmitterControlsStart)

/**
 * Module thread, should not return.
 */
static void transmitterControlsTask(void *parameters)
{
	//ManualControlCommandData mcc;
	/*
    uint8_t Connected;
    float Roll;
    float Pitch;
    float Yaw;
    float Throttle;
    uint16_t Channel[8];
	*/

	AlarmsClear(SYSTEMALARMS_ALARM_ATTITUDE);

	PIOS_ADC_Config((PIOS_ADC_RATE / 1000.0f) * UPDATE_RATE);

	// Main task loop
	uint16_t cntr = 0;
	while (1) {
		PIOS_WDG_UpdateFlag(PIOS_WDG_ATTITUDE);
		++cntr;

		// Only wait the time for two nominal updates before setting an alarm
		float gyro[PIOS_ADC_NUM_CHANNELS];
		if(xQueueReceive(adc_queue, (void * const) gyro, UPDATE_RATE * 2) == errQUEUE_EMPTY)
			AlarmsSet(SYSTEMALARMS_ALARM_ATTITUDE, SYSTEMALARMS_ALARM_ERROR);
		else {
			//ManualControlCommandGet(&mcc);
			//mcc.Channel[0] = gyro[1];
			//mcc.Channel[1] = gyro[2];
			//mcc.Channel[2] = gyro[3];
			//mcc.Channel[3] = gyro[4];
			//ManualControlCommandSet(&mcc);
			AlarmsClear(SYSTEMALARMS_ALARM_ATTITUDE);
		}

		if((cntr % 1000) == 0) {

			/*
			char buf[15];
			int i;
			PIOS_COM_SendString(PIOS_COM_DEBUG, "ACD: ");
			for(i = 0; i < PIOS_ADC_NUM_CHANNELS; ++i) {
				sprintf(buf, "%x ", (unsigned int)gyro[i]);
				PIOS_COM_SendString(PIOS_COM_DEBUG, buf);
			}
			PIOS_COM_SendString(PIOS_COM_DEBUG, "\n\r");
			PIOS_COM_SendString(PIOS_COM_DEBUG, "ACD Read: ");
			for(i = 0; i < PIOS_ADC_NUM_CHANNELS; ++i) {
				sprintf(buf, "%x ", (unsigned int)PIOS_ADC_PinGet(i));
				PIOS_COM_SendString(PIOS_COM_DEBUG, buf);
			}
			PIOS_COM_SendString(PIOS_COM_DEBUG, "\n\r");
			*/

			cntr = 0;
		}
	}
}

/**
 * Processes queue events
 */
static void processObjEvent(UAVObjEvent * ev, RouterComms *comm)
{
	UAVObjMetadata metadata;
	int32_t retries;
	int32_t success;

	if (ev->obj == 0) {
		GCSReceiverData rcvr;
		//char buf[15];
		int i;

		// Read the receiver channels.
		//PIOS_COM_SendString(PIOS_COM_DEBUG, "Rcvr: ");
		for (i = 0; i < GCSRECEIVER_CHANNEL_NUMELEM; ++i) {
			extern uint32_t pios_rcvr_group_map[];
			uint32_t val = PIOS_RCVR_Read(pios_rcvr_group_map[MANUALCONTROLSETTINGS_CHANNELGROUPS_PPM],	i+1);
			rcvr.Channel[i] = val;
			//sprintf(buf, "%x ", (unsigned int)val);
			//PIOS_COM_SendString(PIOS_COM_DEBUG, buf);
		}
		//PIOS_COM_SendString(PIOS_COM_DEBUG, "\n\r");

		// Set the GCSReceiverData object.
		GCSReceiverSet(&rcvr);

		// Send update (with retries)
		retries = 0;
		success = -1;
		while (retries < MAX_RETRIES && success == -1) {
			success = UAVTalkSendObject(&(comm->com), GCSReceiverHandle(), 0, 0, REQ_TIMEOUT_MS);
			++retries;
		}
		// Update stats
		txRetries += (retries - 1);

	} else {

		// Get object metadata
		UAVObjGetMetadata(ev->obj, &metadata);
		// Act on event
		retries = 0;
		success = -1;
		if (ev->event == EV_UPDATED || ev->event == EV_UPDATED_MANUAL) {
			PIOS_COM_SendString(PIOS_COM_DEBUG, "trans update\n\r");
			// Send update (with retries)
			while (retries < MAX_RETRIES && success == -1) {
				success = UAVTalkSendObject(&(comm->com), ev->obj, ev->instId, metadata.telemetryAcked, REQ_TIMEOUT_MS);	// call blocks until ack is received or timeout
				++retries;
			}
			// Update stats
			txRetries += (retries - 1);
			if (success == -1) {
				++txErrors;
			}
		} else if (ev->event == EV_UPDATE_REQ) {
			PIOS_COM_SendString(PIOS_COM_DEBUG, "trans req\n\r");
			// Request object update from GCS (with retries)
			while (retries < MAX_RETRIES && success == -1) {
				success = UAVTalkSendObjectRequest(&(comm->com), ev->obj, ev->instId, REQ_TIMEOUT_MS);	// call blocks until update is received or timeout
				++retries;
			}
			// Update stats
			txRetries += (retries - 1);
			if (success == -1) {
				++txErrors;
			}
		}
	}
}

/**
 * Telemetry transmit task, regular priority
 */
static void transmitterTxTask(void *parameters)
{
	RouterComms *comm = (RouterComms*)parameters;
	UAVObjEvent ev;

	// Loop forever
	while (1) {
		// Wait for queue message
		if (xQueueReceive(comm->txqueue, &ev, portMAX_DELAY) == pdTRUE) {
			// Process event
			processObjEvent(&ev, comm);
		}
	}
}

/**
 * Transmitter receive task. Processes queue events and periodic updates.
 */
static void transmitterRxTask(void *parameters)
{
	RouterComms *comm = (RouterComms*)parameters;
	uint32_t inputPort = comm->port;

	// Task loop
	while (1) {
		if (inputPort) {
			uint8_t serial_data[1];
			uint16_t bytes_to_process;

			// Block until data are available
			bytes_to_process = PIOS_COM_ReceiveBuffer(inputPort, serial_data, sizeof(serial_data), 500);
			if (bytes_to_process > 0) {
				for (uint8_t i = 0; i < bytes_to_process; i++) {
					UAVTalkProcessInputStream(&(comm->com), serial_data[i]);
					UAVTalkRxState state = UAVTalkProcessInputStream(comm->com, serial_data[i]);
					if(state == UAVTALK_STATE_COMPLETE)
						UAVTalkRelay(comm->com, comm->relay_comm->com);
				}
			}
		} else {
			vTaskDelay(5);
		}
	}
}

/**
 * Transmit data buffer to the modem or USB port.
 * \param[in] data Data buffer to send
 * \param[in] length Length of buffer
 * \return 0 Success
 */
static int32_t transmitData(RouterComms *comm, uint8_t * data, int32_t length)
{
	uint32_t outputPort = comm->port;

	if (outputPort) {
		return PIOS_COM_SendBufferNonBlocking(outputPort, data, length);
	} else {
		return -1;
	}
	return 0;
}
static int32_t transmitData1(uint8_t * data, int32_t length)
{
	return transmitData(comms, data, length);
}
static int32_t transmitData2(uint8_t * data, int32_t length)
{
	return transmitData(comms + 1, data, length);
}

/**
 * @}
 * @}
 */
