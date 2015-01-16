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
#define LOG_TAG  "AKL - dash_ext"

#include <cutils/log.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "AKL_APIs.h"
#include "AKL_DASH_Ext.h"
#if defined(AKM3D)
#include "lib3d/akl_smart_compass.h"
#elif defined(AKM6D)
#include "lib6d/akl_smart_compass.h"
#elif defined(AKM9D)
#include "lib6d/akl_smart_compass.h"
#elif defined(AKMPG)
#include "libpg/akl_smart_compass.h"
#elif defined(AKMOSS)
#include "oss/akfs_compass.h"
#else
#error No library defined.
#endif

#define INFO_DATA_LEN  7

/* AKM library may be called from many modules. */
/* Therefore save memory handle */
static struct AKL_SCL_PRMS *handle = NULL;
static pthread_mutex_t     akl_mutex;

int AKL_DASH_Init(const uint8_t max_form)
{
    uint16_t sz;

    ALOGI("%s: called.", __func__);

    if (NULL == handle) {
        sz = AKL_GetParameterSize(max_form);
        ALOGI("%s: sz=%d", __func__, sz);
        handle = malloc(sz);

        if (NULL == handle) {
            ALOGE("%s: malloc failed", __func__);
            return AKM_ERROR;
        }

        /* init mutex */
        if (pthread_mutex_init(&akl_mutex, NULL)) {
            free(handle);
            handle = NULL;
            return AKM_ERROR;
        }
    }

    return AKM_SUCCESS;
}

void AKL_DASH_Deinit(void)
{
    ALOGI("%s: called.", __func__);

    if (NULL != handle) {
        free(handle);
        handle = NULL;
        /* deinit mutex */
        pthread_mutex_destroy(&akl_mutex);
    }
}

struct AKL_SCL_PRMS *AKL_DASH_Lock(void)
{
    pthread_mutex_lock(&akl_mutex);
    return handle;
}

void AKL_DASH_Unlock(void)
{
    pthread_mutex_unlock(&akl_mutex);
}
