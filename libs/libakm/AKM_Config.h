/******************************************************************************
 *
 * Copyright (C) 2015 Asahi Kasei Microdevices Corporation, Japan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/
#ifndef __AKM_CONFIG_H__
#define __AKM_CONFIG_H__

/*** Define one of the definition in your make file. ***/
/* #define AKM_MAGNETOMETER_AK8963 */
/* #define AKM_MAGNETOMETER_AK09911 */
/* #define AKM_MAGNETOMETER_AK09912 */
/* #define AKM_ACCELEROMETER_ADXL34X */
/* #define AKM_ACCELEROMETER_DUMMY */
/* #define AKM_GYROSCOPE_L3G4200D */
/* #define AKM_GYROSCOPE_DUMMY */


#if defined(AKM_MAGNETOMETER_AK8963)
#define AKM_MAGNETOMETER_AK89XX
#elif defined(AKM_MAGNETOMETER_AK09911)
#define AKM_MAGNETOMETER_AK099XX
#elif defined(AKM_MAGNETOMETER_AK09912)
#define AKM_MAGNETOMETER_AK099XX
#endif

/* Enable PDC */
/* #define AKM_ENABLE_PDC */

/* Disable DOEPlus function */
/*#define AKM_DISABLE_DOEPLUS*/
#endif /* __AKM_CONFIG_H__ */
