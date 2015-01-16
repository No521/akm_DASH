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
#ifndef __AKFS_INC_COMPASS_H__
#define __AKFS_INC_COMPASS_H__

/* Root file of AKL module. */
#include "AKL_APIs.h"

/* Include files for AKM OSS library */
#include "akfs_aoc.h"
#include "akfs_configure.h"
#include "akfs_device.h"
#include "akfs_direction.h"
#include "akfs_math.h"
#include "akfs_vnorm.h"

/*** Type declaration *********************************************************/
struct AKSENSOR_DATA {
    AKFLOAT x;
    AKFLOAT y;
    AKFLOAT z;
    int8_t  status;
};

struct AKL_NV_PRMS {
    /*! This value is used to identify the data area is AKL_NV_PRMS.
     * This value should be #AKL_NV_MAGIC_NUMBER. */
    uint32_t magic;
    /*! Offset of magnetic vector */
    AKFVEC   fv_hsuc_ho;
};

struct AKL_SCL_PRMS {
    struct AKL_NV_PRMS *ps_nv;

    /* Variables forAOC. */
    AKFVEC             fva_hdata[AKFS_HDATA_SIZE];
    AKFS_AOC_VAR       s_aocv;

    /* Variables for Magnetometer buffer. */
    AKFVEC             fva_hvbuf[AKFS_HDATA_SIZE];
    AKFVEC             fv_ho;
    AKFVEC             fv_hs;

    /* Variables for Accelerometer buffer. */
    AKFVEC             fva_avbuf[AKFS_ADATA_SIZE];
    AKFVEC             fv_ao;
    AKFVEC             fv_as;

    /* Variables for Direction. */
    AKFLOAT            f_azimuth;
    AKFLOAT            f_pitch;
    AKFLOAT            f_roll;

    /* Variables for vector output */
    AKFVEC             fv_hvec;
    AKFVEC             fv_avec;
    int16_t            i16_hstatus;
};
#endif
