/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{
 * @addtogroup Attitude Copter Control Attitude Estimation
 * @brief Acquires sensor data and computes attitude estimate 
 * Specifically updates the the @ref AttitudeActual "AttitudeActual" and @ref AttitudeRaw "AttitudeRaw" settings objects
 * @{
 *
 * @file       attitude.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      Module to handle all comms to the AHRS on a periodic basis.
 *
 * @see        The GNU Public License (GPL) Version 3
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
 * Output objects: @ref AttitudeRaw @ref AttitudeActual
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
#include "attitude.h"
#include "attituderaw.h"
#include "attitudeactual.h"
#include "attitudesettings.h"
#include "flightstatus.h"
#include "CoordinateConversions.h"
#include "pios_flash_w25x.h"
#if defined (PIOS_INCLUDE_MARG_MAHONY)
#include "MahonyAHRS.h"
extern float integralFBx,integralFBy,integralFBz,sample_dT;
#endif
#if defined(PIOS_INCLUDE_AK8974)
#include "pios_ak8974.h"
#endif

// Private constants
#define STACK_SIZE_BYTES 580
#define TASK_PRIORITY (tskIDLE_PRIORITY+3)

#define RAD2DEG (180.0/M_PI)
#define DEG2RAD (M_PI/180.0)

#define UPDATE_RATE  2.0f

// neutral position for LPR425AL (1.5V)
#define GYRO_NEUTRAL 1861

#define PI_MOD(x) (fmod(x + M_PI, M_PI * 2) - M_PI)
// Private types

// Private variables
static xTaskHandle taskHandle;

// Private functions
static void AttitudeTask(void *parameters);

static xQueueHandle sensor_queue;

static void updateSensors(AttitudeRawData *);
static void updateAttitude(AttitudeRawData *);
static void settingsUpdatedCb(UAVObjEvent * objEv);

static float accelKi = 0;
static float accelKp = 0;
static float yawBiasRate = 0;
// assumed data for LPR425AL
static float gyroGain = 1.42;
static int16_t accelbias[3],magbias[3];

static float R[3][3];
static int8_t rotate = 0;
static bool zero_during_arming = false;

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t AttitudeMARGInitialize(void)
{
	// Initialize quaternion
	AttitudeActualData attitude;
	AttitudeActualGet(&attitude);
	attitude.q1 = 1;
	attitude.q2 = 0;
	attitude.q3 = 0;
	attitude.q4 = 0;
	AttitudeActualSet(&attitude);
	
	// Create queue for passing gyro data, allow 2 back samples in case
	sensor_queue = xQueueCreate(1, sizeof(float) * PIOS_ADC_NUM_PINS);
	if(sensor_queue == NULL)
		return -1;
	PIOS_ADC_SetQueue(sensor_queue);
	
	AttitudeSettingsConnectCallback(&settingsUpdatedCb);
	
	// Start main task
	xTaskCreate(AttitudeTask, (signed char *)"AttitudeMARG", STACK_SIZE_BYTES/4, NULL, TASK_PRIORITY, &taskHandle);
	TaskMonitorAdd(TASKINFO_RUNNING_ATTITUDE, taskHandle);
	PIOS_WDG_RegisterFlag(PIOS_WDG_ATTITUDE);
	return 0;
}

/**
 * Module thread, should not return.
 */
static void AttitudeTask(void *parameters)
{

	uint8_t init = 0;
	volatile uint16_t loopcount=0;
	AlarmsClear(SYSTEMALARMS_ALARM_ATTITUDE);

	PIOS_ADC_Config((PIOS_ADC_RATE / 1000.0f) * UPDATE_RATE);

	// Keep flash CS pin high while talking accel
	PIOS_FLASH_DISABLE;		

	zero_during_arming = false;
	// Main task loop
	while (1) {
		
		FlightStatusData flightStatus;
		FlightStatusGet(&flightStatus);

		if(xTaskGetTickCount() < 7000) {
			// Force settings update to make sure rotation loaded
			settingsUpdatedCb(AttitudeSettingsHandle());
			// For first 7 seconds use accels to get gyro bias
			accelKp = 500;
			accelKi = 250;
			yawBiasRate = 0.1;
			init = 0;
		} 	
		else if (zero_during_arming && (flightStatus.Armed == FLIGHTSTATUS_ARMED_ARMING)) {
			accelKp = 500;
			accelKi = 250;
			yawBiasRate = 0.1;
			init = 0;			
		} else if (init == 0) {
			settingsUpdatedCb(AttitudeSettingsHandle());
			init = 1;
		}						
			
		PIOS_WDG_UpdateFlag(PIOS_WDG_ATTITUDE);
		
		AttitudeRawData attitudeRaw;
		AttitudeRawGet(&attitudeRaw);		
		updateSensors(&attitudeRaw);

		loopcount++;
		twoKi=accelKi;
		twoKp=accelKp;
//		beta=accelKp;
		updateAttitude(&attitudeRaw);

		AttitudeRawSet(&attitudeRaw); 	

	}
}

static void updateSensors(AttitudeRawData * attitudeRaw) 
{	
	// use temp. values instead of messing up UAVOjects during scaling & rotation
	float accel[3],gyro[3];

#if defined(PIOS_INCLUDE_KXSC4)
	float sensors[PIOS_ADC_NUM_PINS];

#endif
	// Aquire sensor data and rotate into board coordinate frame
	// Only wait the time for two nominal updates before setting an alarm
	if(xQueueReceive(sensor_queue, (void * const) sensors, UPDATE_RATE * 2) == errQUEUE_EMPTY) {
		AlarmsSet(SYSTEMALARMS_ALARM_ATTITUDE, SYSTEMALARMS_ALARM_ERROR);
		return;
	}
	
#if defined(PIOS_INCLUDE_KXSC4)
	accel[0] = sensors[0];
	accel[1] = sensors[1];
	accel[2] = -sensors[2];
#endif

	gyro[0] = -(sensors[4] - GYRO_NEUTRAL);
	gyro[1] =   sensors[5] - GYRO_NEUTRAL;
	gyro[2] = -(sensors[6] - GYRO_NEUTRAL);

#if defined(PIOS_INCLUDE_AK8974)
	bool mag_update;
	int16_t magbuffer[3];
	float mag[3];

	// experimental magnetometer support for MoveCopter
	mag_update = PIOS_AK8974_NewDataAvailable();
	if (mag_update) {
		PIOS_AK8974_ReadMag (magbuffer);
		mag[0] =  (float)magbuffer[1];
		mag[1] = -(float)magbuffer[0];
		mag[2] =  (float)magbuffer[2];
	}
#endif
	// Rotate into airframe coordinate frame
	if(rotate) {
		// TODO: rotate sensors too so stabilization is well behaved
		float vec_out[3];
		rot_mult(R, accel, vec_out);
		accel[0] = vec_out[0];
		accel[1] = vec_out[1];
		accel[2] = vec_out[2];
		rot_mult(R, gyro, vec_out);
		gyro[0] = vec_out[0];
		gyro[1] = vec_out[1];
		gyro[2] = vec_out[2];
#if defined(PIOS_INCLUDE_AK8974)
		if (mag_update) {
			rot_mult(R, mag, vec_out);
			mag[0] = vec_out[0];
			mag[1] = vec_out[1];
			mag[2] = vec_out[2];
		}
#endif
	}
	
	// Scale and correct bias
	attitudeRaw->accels[ATTITUDERAW_ACCELS_X] = (accel[0] - accelbias[0]) * 0.00366f * 9.81f;
	attitudeRaw->accels[ATTITUDERAW_ACCELS_Y] = (accel[1] - accelbias[1]) * 0.00366f * 9.81f;
	attitudeRaw->accels[ATTITUDERAW_ACCELS_Z] = (accel[2] - accelbias[2]) * 0.00366f * 9.81f;

	attitudeRaw->gyros[ATTITUDERAW_GYROS_X] = gyro[0] * gyroGain + integralFBx*RAD2DEG;
	attitudeRaw->gyros[ATTITUDERAW_GYROS_Y] = gyro[1] * gyroGain + integralFBy*RAD2DEG;
	attitudeRaw->gyros[ATTITUDERAW_GYROS_Z] = gyro[2] * gyroGain + integralFBz*RAD2DEG;

#if defined(PIOS_INCLUDE_AK8974)
	if (mag_update) {
		attitudeRaw->magnetometers[ATTITUDERAW_MAGNETOMETERS_X] =  mag[0]-magbias[0];
		attitudeRaw->magnetometers[ATTITUDERAW_MAGNETOMETERS_Y] =  mag[1]-magbias[1];
		attitudeRaw->magnetometers[ATTITUDERAW_MAGNETOMETERS_Z] =  mag[2]-magbias[2];
	}
#else
	// need to use yawBiasRate only if we don't have mags.
	integralFBz += -attitudeRaw->gyros[ATTITUDERAW_GYROS_Z]*DEG2RAD * yawBiasRate;
#endif

}

static void updateAttitude(AttitudeRawData * attitudeRaw)
{
	static portTickType lastSysTime = 0;
	static portTickType thisSysTime;

//	static float dT = 0;
	static uint8_t dT_loop = 0;

	// enhance dT resolution
	if (dT_loop == 0)
	{
		thisSysTime = xTaskGetTickCount();
		if(thisSysTime > lastSysTime) // reuse dt in case of wraparound
			sample_dT = (thisSysTime - lastSysTime) / portTICK_RATE_MS / 100000.0f;
		lastSysTime = thisSysTime;
		dT_loop = 100;
	}
	else
	{
		dT_loop--;
	}

#if defined (PIOS_INCLUDE_MARG_MAHONY)
#if defined(PIOS_INCLUDE_AK8974)
	MahonyAHRSupdate(
			attitudeRaw->gyros[ATTITUDERAW_GYROS_X]*DEG2RAD,
			attitudeRaw->gyros[ATTITUDERAW_GYROS_Y]*DEG2RAD,
			attitudeRaw->gyros[ATTITUDERAW_GYROS_Z]*DEG2RAD,
			-attitudeRaw->accels[ATTITUDERAW_ACCELS_X],
			-attitudeRaw->accels[ATTITUDERAW_ACCELS_Y],
			-attitudeRaw->accels[ATTITUDERAW_ACCELS_Z],
			attitudeRaw->magnetometers[ATTITUDERAW_MAGNETOMETERS_X],
			attitudeRaw->magnetometers[ATTITUDERAW_MAGNETOMETERS_Y],
			attitudeRaw->magnetometers[ATTITUDERAW_MAGNETOMETERS_Z]
		);
#else
	MahonyAHRSupdateIMU(
			attitudeRaw->gyros[ATTITUDERAW_GYROS_X]*DEG2RAD,
			attitudeRaw->gyros[ATTITUDERAW_GYROS_Y]*DEG2RAD,
			attitudeRaw->gyros[ATTITUDERAW_GYROS_Z]*DEG2RAD,
			-attitudeRaw->accels[ATTITUDERAW_ACCELS_X],
			-attitudeRaw->accels[ATTITUDERAW_ACCELS_Y],
			-attitudeRaw->accels[ATTITUDERAW_ACCELS_Z]
		);
#endif // PIOS_INCLUDE_AK8974
#endif

	AttitudeActualData attitudeActual;
	AttitudeActualGet(&attitudeActual);

	attitudeActual.q1=q0;
	attitudeActual.q2=q1;
	attitudeActual.q3=q2;
	attitudeActual.q4=q3;

	// Convert into eueler degrees (makes assumptions about RPY order)
	Quaternion2RPY(&attitudeActual.q1,&attitudeActual.Roll);

	AttitudeActualSet(&attitudeActual);
}

static void settingsUpdatedCb(UAVObjEvent * objEv) {
	AttitudeSettingsData attitudeSettings;
	AttitudeSettingsGet(&attitudeSettings);
	
	
	accelKp = attitudeSettings.AccelKp;
	accelKi = attitudeSettings.AccelKi;		
	yawBiasRate = attitudeSettings.YawBiasRate;
	gyroGain = attitudeSettings.GyroGain;
	
	zero_during_arming = attitudeSettings.ZeroDuringArming == ATTITUDESETTINGS_ZERODURINGARMING_TRUE;
	
	accelbias[0] = attitudeSettings.AccelBias[ATTITUDESETTINGS_ACCELBIAS_X];
	accelbias[1] = attitudeSettings.AccelBias[ATTITUDESETTINGS_ACCELBIAS_Y];
	accelbias[2] = attitudeSettings.AccelBias[ATTITUDESETTINGS_ACCELBIAS_Z];
	
	magbias[0] = attitudeSettings.MagBias[ATTITUDESETTINGS_MAGBIAS_X];
	magbias[1] = attitudeSettings.MagBias[ATTITUDESETTINGS_MAGBIAS_Y];
	magbias[2] = attitudeSettings.MagBias[ATTITUDESETTINGS_MAGBIAS_Z];


	// Indicates not to expend cycles on rotation
	if(attitudeSettings.BoardRotation[0] == 0 && attitudeSettings.BoardRotation[1] == 0 &&
	   attitudeSettings.BoardRotation[2] == 0) {
		rotate = 0;
		
		// Shouldn't be used but to be safe
		float rotationQuat[4] = {1,0,0,0};
		Quaternion2R(rotationQuat, R);
	} else {
		float rotationQuat[4];
		const float rpy[3] = {attitudeSettings.BoardRotation[ATTITUDESETTINGS_BOARDROTATION_ROLL], 
			attitudeSettings.BoardRotation[ATTITUDESETTINGS_BOARDROTATION_PITCH], 
			attitudeSettings.BoardRotation[ATTITUDESETTINGS_BOARDROTATION_YAW]};
		RPY2Quaternion(rpy, rotationQuat);
		Quaternion2R(rotationQuat, R);
		rotate = 1;
	}		
}
/**
  * @}
  * @}
  */
