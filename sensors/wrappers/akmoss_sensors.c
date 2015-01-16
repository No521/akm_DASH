/*
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
 */

#define LOG_TAG "DASH - akm6d - wrapper"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "sensor_util.h"
#include "sensor_xyz.h"
#include "sensors_config.h"
#include "sensors_fifo.h"
#include "sensors_id.h"
#include "sensors_list.h"
#include "sensors_log.h"
#include "sensors_select.h"
#include "sensors_sysfs.h"
#include "sensors_wrapper.h"

#include "libs/libakm/AKL_APIs.h"
#include "libs/libakm/AKL_DASH_Ext.h"
#include "libs/libakm/linux/ak0991x.h"

#define CLIENT_DELAY_UNUSED  NO_RATE
#define GRAV_Q16             642688

enum {
    ORIENTATION,
    NUMSENSORS
};

struct akm6d_t {
    /* akm6d internal sensor */
    unsigned int        enable_mask;
    int                 init;
    int                 init_ret;
    struct wrapper_desc akm6d;
    /* android sensors */
    struct wrapper_desc orientation;
    int64_t             delay_requests[NUMSENSORS];
};

static int akm6d_init(
    struct sensor_api_t *s
);

static int akm6d_activate(
    struct sensor_api_t *s,
    int                 enable
);

static int akm6d_delay(
    struct sensor_api_t *s,
    int64_t             ns
);

static void akm6d_close(
    struct sensor_api_t *s
);

static void akm6d_sensors_data(
    struct sensor_api_t  *s,
    struct sensor_data_t *sd
);

struct akm6d_t akm6d = {
    .enable_mask = 0x00, /* Start with all Android sensors deactivated */
    .init = 0,
    .init_ret = SENSOR_UNREGISTER,
    .akm6d = {
        .sensor = {
            name: "AKM6D",
            vendor: "Asahi Kasei Corp.",
            version: sizeof(sensors_event_t),
            handle: SENSOR_INTERNAL_HANDLE_MIN,
        },
        .api = {
            .init = akm6d_init,
            .activate = akm6d_activate,
            .set_delay = akm6d_delay,
            .close = akm6d_close,
            .data = akm6d_sensors_data,
        },
        .access = {
            .match = {
                SENSOR_TYPE_ACCELEROMETER,
                SENSOR_TYPE_MAGNETIC_FIELD,
            },
            .m_nr = 2,
        },
    },
    .orientation = {
        .sensor = {
            name: "AKM OSS Compass",
            vendor: "Asahi Kasei Corp.",
            version: sizeof(sensors_event_t),
            handle: SENSOR_ORIENTATION_HANDLE,
            type: SENSOR_TYPE_ORIENTATION,
            maxRange: 360,
            resolution: 100,
            power: 0.8,
            minDelay: 5000,
        },
        .api = {
            .init = akm6d_init,
            .activate = akm6d_activate,
            .set_delay = akm6d_delay,
            .close = akm6d_close,
        },
    },
    .delay_requests = {
        CLIENT_DELAY_UNUSED,
        CLIENT_DELAY_UNUSED,
    },
};

static int akm6d_init(struct sensor_api_t *s)
{
    uint16_t                      sz;

    ALOGD("%s: called.", __func__);

    if (!akm6d.init) {
        akm6d.init = 1;
        akm6d.init_ret = sensors_wrapper_init(&akm6d.akm6d.api);

        if (akm6d.init_ret < 0) {
            ALOGE("%s: init failed", __func__);
            return akm6d.init_ret;
        }

        ALOGI("%s: setup finished.", __func__);
    }

    return akm6d.init_ret;
}

static int akm6d_activate(
    struct sensor_api_t *s,
    int                 enable)
{
    struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
    int                 ret;
    int                 prev_en;
    int                 sensor = -1;

    ALOGD("%s: called by '%s' en=%d, mask=%d",
          __func__, d->sensor.name, enable, akm6d.enable_mask);

    switch (d->sensor.handle) {
    case SENSOR_ORIENTATION_HANDLE:
        sensor = ORIENTATION;
        break;

    default:
        ALOGE("%s: Should not reach here.", __func__);
        return -1;
    }

    if (enable) {
        /* reserve previous value */
        prev_en = akm6d.enable_mask;
        /* set flag */
        akm6d.enable_mask |= (1 << sensor);

        /* If other sensor was already activated, skip wrapper */
        if (prev_en > 0) {
            return 0;
        }
    } else {
        ret = akm6d_delay(s, CLIENT_DELAY_UNUSED);

        if (ret) {
            ALOGE("%s: Error %s delay failed", __func__, d->sensor.name);
        }

        /* unset flag */
        akm6d.enable_mask &= ~(1 << sensor);

        /* if other sensor is still active, skip wrapper */
        if (akm6d.enable_mask != 0) {
            return 0;
        }
    }

    return sensors_wrapper_activate(&akm6d.akm6d.api, enable);
}

static int akm6d_delay(
    struct sensor_api_t *s,
    int64_t             ns)
{
    struct sensor_desc *d = container_of(s, struct sensor_desc, api);
    int                i, sensor = -1;
    int64_t            delay_tmp = CLIENT_DELAY_UNUSED;

    ALOGD("%s: called by '%s' ns=%lld, mask=%d",
          __func__, d->sensor.name, ns, akm6d.enable_mask);

    switch (d->sensor.handle) {
    case SENSOR_ORIENTATION_HANDLE:
        sensor = ORIENTATION;
        break;

    default:
        return sensor;
    }

    akm6d.delay_requests[sensor] = ns;

    for (i = 0; i < NUMSENSORS; i++) {
        if ((akm6d.enable_mask & (1 << i)) &&
            (akm6d.delay_requests[i] != CLIENT_DELAY_UNUSED)) {
            if ((delay_tmp == CLIENT_DELAY_UNUSED) ||
                (delay_tmp > akm6d.delay_requests[i])) {
                delay_tmp = akm6d.delay_requests[i];
            }
        }
    }

    return sensors_wrapper_set_delay(&akm6d.akm6d.api, delay_tmp);
}

static void akm6d_close(struct sensor_api_t *s)
{
    struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);

    ALOGD("%s: called by '%s' mask=%d",
          __func__, d->sensor.name, akm6d.enable_mask);

    if (akm6d.enable_mask == 0) {
        sensors_wrapper_close(&akm6d.akm6d.api);
    }
}

static void akm6d_sensors_data(
    struct sensor_api_t  *s,
    struct sensor_data_t *sd)
{
    /* update flag */
    static int             up_a = 0;
    static int             up_m = 0;
    struct wrapper_desc    *d = container_of(s, struct wrapper_desc, api);
    sensors_event_t        data;
    int                    err;
    struct AKM_SENSOR_DATA akm_data;
    int32_t                vec[6];
    int32_t                st;
    struct AKL_SCL_PRMS    *mem;

    memset(&data, 0, sizeof(data));

    mem = AKL_DASH_Lock();

    if (sd->sensor->type == SENSOR_TYPE_ACCELEROMETER) {
        akm_data.u.s.x = (sd->data[AXIS_X] * sd->scale) * GRAV_Q16;
        akm_data.u.s.y = (sd->data[AXIS_Y] * sd->scale) * GRAV_Q16;
        akm_data.u.s.z = (sd->data[AXIS_Z] * sd->scale) * GRAV_Q16;
        akm_data.stype = AKM_ST_ACC;
        akm_data.time_us = sd->timestamp / 1000;
        err = AKL_SetVector(mem, &akm_data, 1);

        if (err && (AKM_ERR_NOT_SUPPORT != err)) {
            ALOGE("%s,%d: AKL_SetVector Error (%d)!",
                  __func__, __LINE__, err);
        } else {
            up_a = 1;
        }
    }

    if (sd->sensor->type == SENSOR_TYPE_MAGNETIC_FIELD) {
        up_m = 1;
    }

    /* Calculation */
    err = AKL_CalcFusion(mem);

    if (err != AKM_SUCCESS) {
        ALOGE("AKL_CalcFusion failed (%d).", err);
    }

    /* fusion sensors */
    if (up_a && up_m) {
        data.timestamp = sd->timestamp;

        if (akm6d.enable_mask & (1 << ORIENTATION)) {
            err = AKL_GetVector(AKM_VT_ORI, mem, vec, 3, &st);

            if (err) {
                ALOGE("%s,%d: AKL_GetVector Error (%d)!",
                      __func__, __LINE__, err);
            } else {
                data.magnetic.azimuth = vec[0] / 65536.0f;
                data.magnetic.pitch = vec[1] / 65536.0f;
                data.magnetic.roll = vec[2] / 65536.0f;
                data.version = akm6d.orientation.sensor.version;
                data.sensor = akm6d.orientation.sensor.handle;
                data.type = akm6d.orientation.sensor.type;
                sensors_fifo_put(&data);
            }
        }

        up_a = up_m = 0;
    }

exit:
    AKL_DASH_Unlock();
}

list_constructor(akm6d_register);
void akm6d_register()
{
    (void)sensors_list_register(
        &akm6d.orientation.sensor,
        &akm6d.orientation.api);
}
