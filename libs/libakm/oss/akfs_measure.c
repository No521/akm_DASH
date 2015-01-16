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
#include "akfs_measure.h"

/*****************************************************************************/
void AKFS_InitPRMS(struct AKL_SCL_PRMS *prms)
{
    /* Sensitivity */
    prms->fv_hs.u.x = AKFS_MAG_SENSE;
    prms->fv_hs.u.y = AKFS_MAG_SENSE;
    prms->fv_hs.u.z = AKFS_MAG_SENSE;
    prms->fv_as.u.x = AKFS_ACC_SENSE;
    prms->fv_as.u.y = AKFS_ACC_SENSE;
    prms->fv_as.u.z = AKFS_ACC_SENSE;
}

/*****************************************************************************/
void AKFS_SetDefaultNV(struct AKL_NV_PRMS *nv)
{
    nv->fv_hsuc_ho.u.x = 0;
    nv->fv_hsuc_ho.u.y = 0;
    nv->fv_hsuc_ho.u.z = 0;
}

/*****************************************************************************/
int16_t AKFS_InitMeasure(struct AKL_SCL_PRMS *prms)
{
    /* Restore the value */
    prms->fv_ho = prms->ps_nv->fv_hsuc_ho;

    /* Initialize buffer */
    AKFS_InitBuffer(AKFS_HDATA_SIZE, prms->fva_hdata);
    AKFS_InitBuffer(AKFS_HDATA_SIZE, prms->fva_hvbuf);
    AKFS_InitBuffer(AKFS_ADATA_SIZE, prms->fva_avbuf);

    /* Initialize for AOC */
    AKFS_InitAOC(&prms->s_aocv);

    return AKM_SUCCESS;
}

/******************************************************************************/
int16_t AKFS_Set_MAGNETIC_FIELD(
    struct AKL_SCL_PRMS *prms,
    const AKFLOAT       mag[3])
{
    int16_t akret;
    int16_t aocret;
    AKFLOAT radius;

    /* Make a spare area for new data */
    akret = AKFS_BufShift(
            AKFS_ADATA_SIZE,
            1,
            prms->fva_avbuf
        );

    if (akret == AKFS_ERROR) {
        return AKM_ERROR;
    }

    /* put new data */
    /* mag[in]: Android coordinate, sensitivity adjusted. */
    prms->fva_hdata[0].v[0] = mag[0];
    prms->fva_hdata[0].v[1] = mag[1];
    prms->fva_hdata[0].v[2] = mag[2];

    /* Offset calculation is done in this function */
    /* hdata[in] : Android coordinate, sensitivity adjusted. */
    /* ho   [out]: Android coordinate, sensitivity adjusted. */
    aocret = AKFS_AOC(
            &prms->s_aocv,
            prms->fva_hdata,
            &prms->fv_ho
        );

    /* Subtract offset, then put the vector to another buffer. */
    /* hdata, ho[in] : Android coordinate, sensitivity adjusted. */
    /* hvbuf    [out]: Android coordinate, sensitivity adjusted, */
    /*		           offset subtracted. */
    akret = AKFS_VbNorm(
            AKFS_HDATA_SIZE,
            prms->fva_hdata,
            1,
            &prms->fv_ho,
            &prms->fv_hs,
            AKFS_MAG_SENSE,
            AKFS_HDATA_SIZE,
            prms->fva_hvbuf
        );

    if (akret == AKFS_ERROR) {
        return AKM_ERROR;
    }

    /* Averaging */
    akret = AKFS_VbAve(
            AKFS_HDATA_SIZE,
            prms->fva_hvbuf,
            AKFS_HNAVE_V,
            &prms->fv_hvec
        );

    if (akret == AKFS_ERROR) {
        return AKM_ERROR;
    }

    /* Check the size of magnetic vector */
    radius = AKFS_SQRT(
            (prms->fv_hvec.u.x * prms->fv_hvec.u.x) +
            (prms->fv_hvec.u.y * prms->fv_hvec.u.y) +
            (prms->fv_hvec.u.z * prms->fv_hvec.u.z));

    if (radius > AKFS_GEOMAG_MAX) {
        prms->i16_hstatus = 0;
    } else {
        if (aocret == AKFS_SUCCESS) {
            prms->i16_hstatus = 3;
        }
    }

    return AKM_SUCCESS;
}

/******************************************************************************/
int16_t AKFS_Set_ACCELEROMETER(
    struct AKL_SCL_PRMS *prms,
    const AKFLOAT       acc[3])
{
    int16_t akret;


    /* Make a spare area for new data */
    akret = AKFS_BufShift(
            AKFS_ADATA_SIZE,
            1,
            prms->fva_avbuf
        );

    if (akret == AKFS_ERROR) {
        return AKM_ERROR;
    }

    /* put new data */
    /* acc [in]: Android coordinate, sensitivity adjusted (SI unit), */
    /*			 offset subtracted. */
    prms->fva_avbuf[0].v[0] = acc[0];
    prms->fva_avbuf[0].v[1] = acc[1];
    prms->fva_avbuf[0].v[2] = acc[2];

    /* Averaging */
    /* avbuf[in] : Android coordinate, sensitivity adjusted, */
    /*			   offset subtracted. */
    /* avec [out]: Android coordinate, sensitivity adjusted, */
    /*			   offset subtracted, averaged. */
    akret = AKFS_VbAve(
            AKFS_ADATA_SIZE,
            prms->fva_avbuf,
            AKFS_ANAVE_V,
            &prms->fv_avec
        );

    if (akret == AKFS_ERROR) {
        return AKM_ERROR;
    }

    return AKM_SUCCESS;
}
