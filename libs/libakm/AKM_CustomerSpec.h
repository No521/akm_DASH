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
#ifndef __AKM_CUSTOMERSPEC_H__
#define __AKM_CUSTOMERSPEC_H__


/*! \defgroup CUSTOMER_SPEC Customer specific parameters.
 * SmartCompass library parameters. <b> Please change these parameters
 * according to the directions from AKM.</b>
 @{*/
/*! A string of licenser. Don't change this string. */
#define AKM_CUSTOM_LICENSER  "ASAHIKASEI"
/*! A string of licensee. This string should be changed. */
#define AKM_CUSTOM_LICENSEE  "ECOMPASS"

/*! The number of formation. */
#define AKM_CUSTOM_NUM_FORM  1

/*! Measurement frequency */
#define AKM_CUSTOM_FREQUENCY  20

/*! \defgroup CSPEC_AXIS The axis conversion
 * Axis conversion parameters.
 @{*/
#define AKM_CUSTOM_MAG_AXIS_ORDER_X  0
#define AKM_CUSTOM_MAG_AXIS_ORDER_Y  1
#define AKM_CUSTOM_MAG_AXIS_ORDER_Z  2
#define AKM_CUSTOM_MAG_AXIS_SIGN_X   0
#define AKM_CUSTOM_MAG_AXIS_SIGN_Y   0
#define AKM_CUSTOM_MAG_AXIS_SIGN_Z   0

#define AKM_CUSTOM_ACC_AXIS_ORDER_X  0
#define AKM_CUSTOM_ACC_AXIS_ORDER_Y  1
#define AKM_CUSTOM_ACC_AXIS_ORDER_Z  2
#define AKM_CUSTOM_ACC_AXIS_SIGN_X   0
#define AKM_CUSTOM_ACC_AXIS_SIGN_Y   0
#define AKM_CUSTOM_ACC_AXIS_SIGN_Z   0

#define AKM_CUSTOM_GYR_AXIS_ORDER_X  0
#define AKM_CUSTOM_GYR_AXIS_ORDER_Y  1
#define AKM_CUSTOM_GYR_AXIS_ORDER_Z  2
#define AKM_CUSTOM_GYR_AXIS_SIGN_X   0
#define AKM_CUSTOM_GYR_AXIS_SIGN_Y   0
#define AKM_CUSTOM_GYR_AXIS_SIGN_Z   0
/*@}*/

/*! If you want to use continuous measurement mode, uncomment the below line */
/* #define AKM_CUSTOM_CONTINUOUS_MEASURE */

/*@}*/
#endif /* __AKM_CUSTOMERSPEC_H__ */
