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
#include "AKL_APIs.h"     /* API decleration */
#include "akfs_compass.h" /* Definition of struct */
#include "akfs_measure.h"

/*! Identify the nv data. */
#define AKL_NV_MAGIC_NUMBER  (uint32_t)(0xcafecafe)
/*! 1G (= 9.8 m/s^2) in Q16 format. i.e. (9.80665f * 65536) */
#define ACC_1G_IN_Q16        (642689)

/*! Convert from Q16 to floating format. */
#define Q16_TO_FLOAT(x)  (AKFLOAT)((x) / 65536.0f)
/*! Convert from floating format to Q16. */
#define FLOAT_TO_Q16(x)  (int32_t)((x) * 65536)

/*! Convert from AKSC to SI unit (m/s^2) in Q16 format. */
#define ACC_CONVERT_TO_Q16(x)    (int32_t)(((x) * ACC_1G_IN_Q16) / 720)
/*! Convert from SI unit (m/s^2) to AKSC format. */
#define ACC_CONVERT_FROM_Q16(x)  (int32_t)(((x) * 720) / ACC_1G_IN_Q16)

/******************************************************************************/
/***** AKM static functions ***************************************************/
static uint16_t byte_allign(const int32_t sz)
{
    if (0 >= sz) {
        return (uint16_t)0;
    }

    /* Another method.
     int32_t rem = sz % 4;
     return (rem ? (sz + (4 - rem)) : (sz));
     */
    return ((sz & 0x3) ? ((sz & ~(0x3)) + 0x4) : sz);
}

/*****************************************************************************/
static int16_t akl_setv_mag(
    struct AKL_SCL_PRMS          *mem,
    const struct AKM_SENSOR_DATA *data)
{
    AKFLOAT mag[3];
    int     i;
#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }

    if (data == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
#endif

    for (i = 0; i < 3; i++) {
        mag[i] = Q16_TO_FLOAT(data->u.v[i]);
    }

    return AKFS_Set_MAGNETIC_FIELD(mem, mag);
}

/**************************************/
static int16_t akl_setv_acc(
    struct AKL_SCL_PRMS          *mem,
    const struct AKM_SENSOR_DATA *data)
{
    AKFLOAT acc[3];
    int     i;
#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }

    if (data == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
#endif

    for (i = 0; i < 3; i++) {
        acc[i] = Q16_TO_FLOAT(data->u.v[i]);
    }

    return AKFS_Set_ACCELEROMETER(mem, acc);
}

/**************************************/
static int16_t akl_setv_gyr(
    struct AKL_SCL_PRMS          *mem,
    const struct AKM_SENSOR_DATA *data)
{
    return AKM_ERR_NOT_SUPPORT;
}

/*****************************************************************************/
static int16_t akl_getv_mag(
    struct AKL_SCL_PRMS *mem,
    int32_t             data[6],
    int32_t             *status)
{
    int i;
#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }

    if (data == NULL) {
        return AKM_ERR_INVALID_ARG;
    }

    if (status == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
#endif

    /* Convert from SmartCompass to Q16 */
    for (i = 0; i < 3; i++) {
        data[i] = FLOAT_TO_Q16(mem->fv_hvec.v[i]);
        data[i + 3] = FLOAT_TO_Q16(mem->fv_ho.v[i]);
    }

    *status = (int32_t)mem->i16_hstatus;

    return AKM_SUCCESS;
}

/**************************************/
static int16_t akl_getv_acc(
    struct AKL_SCL_PRMS *mem,
    int32_t             data[3],
    int32_t             *status)
{
    int i;
#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }

    if (data == NULL) {
        return AKM_ERR_INVALID_ARG;
    }

    if (status == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
#endif

    for (i = 0; i < 3; i++) {
        data[i] = FLOAT_TO_Q16(mem->fv_avec.v[i]);
    }

    *status = (int32_t)(3);

    return AKM_SUCCESS;
}

/**************************************/
static int16_t akl_getv_gyr(
    struct AKL_SCL_PRMS *mem,
    int32_t             data[6],
    int32_t             *status)
{
    return AKM_ERR_NOT_SUPPORT;
}

/**************************************/
static int16_t akl_getv_ori(
    struct AKL_SCL_PRMS *mem,
    int32_t             data[3],
    int32_t             *status)
{
    /* Convert unit */
    /* from Q6 to Q16 */
    data[0] = FLOAT_TO_Q16(mem->f_azimuth);
    data[1] = FLOAT_TO_Q16(mem->f_pitch);
    data[2] = FLOAT_TO_Q16(mem->f_roll);

    *status = (int32_t)(3);
    return AKM_SUCCESS;
}

/**************************************/
int16_t akl_getv_quat(
    struct AKL_SCL_PRMS *mem,
    int32_t             data[4],
    int32_t             *status)
{
    return AKM_ERR_NOT_SUPPORT;
}

/******************************************************************************/
/***** AKM public APIs ********************************************************/
/***** Function manual is described in header file. ***************************/
uint16_t AKL_GetParameterSize(const uint8_t max_form)
{
    /* form is not used */
    return byte_allign(sizeof(struct AKL_SCL_PRMS));
}

/*****************************************************************************/
uint16_t AKL_GetNVdataSize(const uint8_t max_form)
{
    /* form is not used */
    return byte_allign(sizeof(struct AKL_NV_PRMS));
}

/*****************************************************************************/
int16_t AKL_Init(
    struct AKL_SCL_PRMS                 *mem,
    const struct AKL_CERTIFICATION_INFO *cert,
    const uint8_t                       max_form)
{
#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
    /* "cert" can be NULL, because this parameter is used
     * in genuine library.
     */
#endif
    /* Clear all data. */
    AKFS_InitPRMS(mem);

    /* Set NV data pointer */
    mem->ps_nv = (struct AKL_NV_PRMS *)(
            (uint64_t)mem
            + byte_allign(sizeof(struct AKL_SCL_PRMS)));

    return AKM_SUCCESS;
}

/*****************************************************************************/
int16_t AKL_StartMeasurement(
    struct AKL_SCL_PRMS *mem,
    const uint8_t       *nv_data)
{
    struct AKL_NV_PRMS *p_nv;
    struct AKL_NV_PRMS *p_pr;

#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
#endif
    p_nv = (struct AKL_NV_PRMS *)nv_data;
    p_pr = mem->ps_nv;

    if (p_nv == NULL) {
        /* If parameters couldn't be got, set default value. */
        AKFS_SetDefaultNV(p_pr);
    } else {
        if (p_nv->magic == AKL_NV_MAGIC_NUMBER) {
            /* Copy NV data to mem struct. */
            *p_pr = *p_nv;
        } else {
            AKFS_SetDefaultNV(p_pr);
        }
    }

    /* Init SmartCompass library functions. */
    return AKFS_InitMeasure(mem);
}

/*****************************************************************************/
int16_t AKL_StopMeasurement(
    struct AKL_SCL_PRMS *mem,
    uint8_t             *nv_data)
{
    struct AKL_NV_PRMS *p_nv;
    struct AKL_NV_PRMS *p_pr;

#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
#endif
    p_nv = (struct AKL_NV_PRMS *)nv_data;
    p_pr = mem->ps_nv;

    if (p_nv != NULL) {
        /* Copy mem data to NV buffer. */
        *p_nv = *p_pr;
        p_nv->magic = AKL_NV_MAGIC_NUMBER;
    }

    return AKM_SUCCESS;
}

/*****************************************************************************/
int16_t AKL_SetVector(
    struct AKL_SCL_PRMS          *mem,
    const struct AKM_SENSOR_DATA *data,
    const uint8_t                num)
{
    uint8_t i;
    int16_t ret;

#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }

    if (data == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
#endif

    for (i = 0; i < num; i++) {
        switch (data[i].stype) {
        case AKM_ST_MAG:
            ret = akl_setv_mag(mem, &data[i]);
            break;

        case AKM_ST_ACC:
            ret = akl_setv_acc(mem, &data[i]);
            break;

        case AKM_ST_GYR:
            ret = akl_setv_gyr(mem, &data[i]);
            break;

        default:
            ret = AKM_ERR_NOT_SUPPORT;
            break;
        }

        if (ret != AKM_SUCCESS) {
            return ret;
        }
    }

    return AKM_SUCCESS;
}

/*****************************************************************************/
int16_t AKL_CalcFusion(struct AKL_SCL_PRMS *mem)
{
    int16_t ret;
#ifdef AKL_ARGUMENT_CHECK
    if (mem == NULL) {
        return AKM_ERR_INVALID_ARG;
    }
#endif
    /* Azimuth calculation */
    /* hvbuf[in] : Android coordinate, sensitivity adjusted, */
    /*			   offset subtracted. */
    /* avbuf[in] : Android coordinate, sensitivity adjusted, */
    /*			   offset subtracted. */
    /* azimuth[out]: Android coordinate and unit (degree). */
    /* pitch  [out]: Android coordinate and unit (degree). */
    /* roll   [out]: Android coordinate and unit (degree). */
    ret = AKFS_Direction(
            AKFS_HDATA_SIZE,
            mem->fva_hvbuf,
            AKFS_HNAVE_D,
            AKFS_ADATA_SIZE,
            mem->fva_avbuf,
            AKFS_ANAVE_D,
            &mem->f_azimuth,
            &mem->f_pitch,
            &mem->f_roll
        );

    if (ret == AKFS_ERROR) {
        return AKM_ERROR;
    }

    return AKM_SUCCESS;
}

/*****************************************************************************/
int16_t AKL_GetVector(
    const AKM_VECTOR_TYPE vtype,
    struct AKL_SCL_PRMS   *mem,
    int32_t               *data,
    uint8_t               size,
    int32_t               *status)
{
    switch (vtype) {
    case AKM_VT_MAG:

        if (AKM_VT_MAG_SIZE > size) {
            return AKM_ERR_INVALID_ARG;
        }

        return akl_getv_mag(mem, data, status);

    case AKM_VT_ACC:

        if (AKM_VT_ACC_SIZE > size) {
            return AKM_ERR_INVALID_ARG;
        }

        return akl_getv_acc(mem, data, status);

    case AKM_VT_GYR:

        if (AKM_VT_GYR_SIZE > size) {
            return AKM_ERR_INVALID_ARG;
        }

        return akl_getv_gyr(mem, data, status);

    case AKM_VT_ORI:

        if (AKM_VT_ORI_SIZE > size) {
            return AKM_ERR_INVALID_ARG;
        }

        return akl_getv_ori(mem, data, status);

    case AKM_VT_QUAT:

        if (AKM_VT_QUAT_SIZE > size) {
            return AKM_ERR_INVALID_ARG;
        }

        return akl_getv_quat(mem, data, status);

    default:
        return AKM_ERR_NOT_SUPPORT;
    }
}

/*****************************************************************************/
void AKL_GetLibraryInfo(struct AKL_LIBRARY_INFO *info)
{
    info->partno = 0;
    info->major = 0;
    info->minor = 0;
    info->variation = 0;
    info->revision = 0;
    info->datecode = 0;
}

/*****************************************************************************/
void AKL_ForceReCalibration(struct AKL_SCL_PRMS *mem)
{
    mem->i16_hstatus = 0;
}

/*****************************************************************************/
int16_t AKL_ChangeFormation(
    struct AKL_SCL_PRMS *mem,
    const uint8_t       formNumber)
{
    return AKM_ERR_NOT_SUPPORT;
}

/*****************************************************************************/
int16_t AKL_SetPDC(
    struct AKL_SCL_PRMS *mem,
    const uint8_t       pdc[AKL_PDC_SIZE],
    const uint8_t       formNumber)
{
    return AKM_ERR_NOT_SUPPORT;
}
