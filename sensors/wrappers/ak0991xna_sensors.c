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

#define LOG_TAG "DASH - ak0991xna - wrapper"

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
    MAGNETIC,
    MAGNETIC_UNCALIB,
    NUMSENSORS
};

struct akm3d_t {
    /* ak0991x internal sensor */
    unsigned int        enable_mask;
    int                 init;
    int                 init_ret;
    struct wrapper_desc ak0991x;
    /* android sensors */
    struct wrapper_desc magnetic;
    struct wrapper_desc magnetic_uncalib;
    int64_t             delay_requests[NUMSENSORS];
};

static int ak0991x_init(
    struct sensor_api_t *s
);

static int ak0991x_activate(
    struct sensor_api_t *s,
    int                 enable
);

static int ak0991x_delay(
    struct sensor_api_t *s,
    int64_t             ns
);

static void ak0991x_close(
    struct sensor_api_t *s
);

static void ak0991xna_compass_data(
    struct sensor_api_t  *s,
    struct sensor_data_t *sd
);

struct akm3d_t akm3d = {
    .enable_mask = 0x00, /* Start with all Android sensors deactivated */
    .init = 0,
    .init_ret = SENSOR_UNREGISTER,
    .ak0991x = {
        .sensor = {
            name: AKM_CHIP_NAME,
            vendor: "Asahi Kasei Corp.",
            version: sizeof(sensors_event_t),
            handle: SENSOR_INTERNAL_HANDLE_MIN,
        },
        .api = {
            .init = ak0991x_init,
            .activate = ak0991x_activate,
            .set_delay = ak0991x_delay,
            .close = ak0991x_close,
            .data = ak0991xna_compass_data,
        },
        .access = {
            .match = {
                SENSOR_TYPE_MAGNETIC_FIELD,
            },
            .m_nr = 1,
        },
    },
    .magnetic = {
        .sensor = {
            name: AKM_CHIP_NAME " Magnetic Field",
            vendor: "Asahi Kasei Corp.",
            version: sizeof(sensors_event_t),
            handle: SENSOR_MAGNETIC_FIELD_HANDLE,
            type: SENSOR_TYPE_MAGNETIC_FIELD,
            maxRange: AKM_CHIP_MAXRANGE,
            resolution: AKM_CHIP_RESOLUTION,
            power: AKM_CHIP_POWER,
            minDelay: 5000,
        },
        .api = {
            .init = ak0991x_init,
            .activate = ak0991x_activate,
            .set_delay = ak0991x_delay,
            .close = ak0991x_close,
        },
    },
    .magnetic_uncalib = {
        .sensor = {
            name: AKM_CHIP_NAME " Magnetic Field Uncalibrated",
            vendor: "Asahi Kasei Corp.",
            version: sizeof(sensors_event_t),
            handle: SENSOR_MAGNETIC_FIELD_UNCALIBRATED_HANDLE,
            type: SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
            maxRange: AKM_CHIP_MAXRANGE,
            resolution: AKM_CHIP_RESOLUTION,
            power: AKM_CHIP_POWER,
            minDelay: 5000,
        },
        .api = {
            .init = ak0991x_init,
            .activate = ak0991x_activate,
            .set_delay = ak0991x_delay,
            .close = ak0991x_close,
        },
    },
    .delay_requests = {
        CLIENT_DELAY_UNUSED,
        CLIENT_DELAY_UNUSED,
    },
};

static int ak0991x_init(struct sensor_api_t *s)
{
    uint16_t sz;

    ALOGD("%s: called.", __func__);

    if (!akm3d.init) {
        akm3d.init = 1;
        akm3d.init_ret = sensors_wrapper_init(&akm3d.ak0991x.api);

        if (akm3d.init_ret < 0) {
            ALOGE("%s: init failed", __func__);
            return akm3d.init_ret;
        }

        ALOGI("%s: setup finished.", __func__);
    }

    return akm3d.init_ret;
}

static int ak0991x_activate(
    struct sensor_api_t *s,
    int                 enable)
{
    struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
    int                 ret;
    int                 prev_en;
    int                 sensor = -1;

    ALOGD("%s: called by '%s' en=%d, mask=%d",
          __func__, d->sensor.name, enable, akm3d.enable_mask);

    switch (d->sensor.handle) {
    case SENSOR_MAGNETIC_FIELD_HANDLE:
        sensor = MAGNETIC;
        break;

    case SENSOR_MAGNETIC_FIELD_UNCALIBRATED_HANDLE:
        sensor = MAGNETIC_UNCALIB;
        break;

    default:
        ALOGE("%s: Should not reach here.", __func__);
        return -1;
    }

    if (enable) {
        /* reserve previous value */
        prev_en = akm3d.enable_mask;
        /* set flag */
        akm3d.enable_mask |= (1 << sensor);

        /* If other sensor was already activated, skip wrapper */
        if (prev_en > 0) {
            return 0;
        }
    } else {
        ret = ak0991x_delay(s, CLIENT_DELAY_UNUSED);

        if (ret) {
            ALOGE("%s: Error %s delay failed", __func__, d->sensor.name);
        }

        /* unset flag */
        akm3d.enable_mask &= ~(1 << sensor);

        /* if other sensor is still active, skip wrapper */
        if (akm3d.enable_mask != 0) {
            return 0;
        }
    }

    return sensors_wrapper_activate(&akm3d.ak0991x.api, enable);
}

static int ak0991x_delay(
    struct sensor_api_t *s,
    int64_t             ns)
{
    struct sensor_desc *d = container_of(s, struct sensor_desc, api);
    int                i, sensor = -1;
    int64_t            delay_tmp = CLIENT_DELAY_UNUSED;

    ALOGD("%s: called by '%s' ns=%lld, mask=%d",
          __func__, d->sensor.name, ns, akm3d.enable_mask);

    switch (d->sensor.handle) {
    case SENSOR_MAGNETIC_FIELD_HANDLE:
        sensor = MAGNETIC;
        break;

    case SENSOR_MAGNETIC_FIELD_UNCALIBRATED_HANDLE:
        sensor = MAGNETIC_UNCALIB;
        break;

    default:
        return sensor;
    }

    akm3d.delay_requests[sensor] = ns;

    for (i = 0; i < NUMSENSORS; i++) {
        if ((akm3d.enable_mask & (1 << i)) &&
            (akm3d.delay_requests[i] != CLIENT_DELAY_UNUSED)) {
            if ((delay_tmp == CLIENT_DELAY_UNUSED) ||
                (delay_tmp > akm3d.delay_requests[i])) {
                delay_tmp = akm3d.delay_requests[i];
            }
        }
    }

    return sensors_wrapper_set_delay(&akm3d.ak0991x.api, delay_tmp);
}

static void ak0991x_close(struct sensor_api_t *s)
{
    struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);

    ALOGD("%s: called by '%s' mask=%d",
          __func__, d->sensor.name, akm3d.enable_mask);

    if (akm3d.enable_mask == 0) {
        sensors_wrapper_close(&akm3d.ak0991x.api);
    }
}

static void ak0991xna_compass_data(
    struct sensor_api_t  *s,
    struct sensor_data_t *sd)
{
    /* update flag */
    struct wrapper_desc *d = container_of(s, struct wrapper_desc, api);
    sensors_event_t     data;
    int                 err;
    int32_t             vec[6];
    int32_t             st;
    struct AKL_SCL_PRMS *mem;

    memset(&data, 0, sizeof(data));

    /* SetVector should be done in base module */

    if (sd->sensor->type == SENSOR_TYPE_MAGNETIC_FIELD) {
        /* sensor_data_t data is uncalibrated raw data */
        /* So cannot use here */
        data.timestamp = sd->timestamp;
        /* 0,1,2: magnetic vector,  3,4,5: bias */
        mem = AKL_DASH_Lock();
        err = AKL_GetVector(AKM_VT_MAG, mem, vec, 6, &st);
        AKL_DASH_Unlock();

        if (err) {
            ALOGE("%s,%d: AKL_GetVector Error (%d)!",
                  __func__, __LINE__, err);
        } else {
            if (akm3d.enable_mask & (1 << MAGNETIC)) {
                data.magnetic.x = vec[0] / 65536.0f;
                data.magnetic.y = vec[1] / 65536.0f;
                data.magnetic.z = vec[2] / 65536.0f;
                data.magnetic.status = st;
                data.version = akm3d.magnetic.sensor.version;
                data.sensor = akm3d.magnetic.sensor.handle;
                data.type = akm3d.magnetic.sensor.type;
                sensors_fifo_put(&data);
            }

            if (akm3d.enable_mask & (1 << MAGNETIC_UNCALIB)) {
                data.uncalibrated_magnetic.x_uncalib =
                    (vec[0] + vec[3]) / 65536.0f;
                data.uncalibrated_magnetic.y_uncalib =
                    (vec[1] + vec[4]) / 65536.0f;
                data.uncalibrated_magnetic.z_uncalib =
                    (vec[2] + vec[5]) / 65536.0f;
                data.uncalibrated_magnetic.x_bias = vec[3] / 65536.0f;
                data.uncalibrated_magnetic.y_bias = vec[4] / 65536.0f;
                data.uncalibrated_magnetic.z_bias = vec[5] / 65536.0f;
                data.version = akm3d.magnetic_uncalib.sensor.version;
                data.sensor = akm3d.magnetic_uncalib.sensor.handle;
                data.type = akm3d.magnetic_uncalib.sensor.type;
                sensors_fifo_put(&data);
            }
        }
    }
}

list_constructor(ak0991xna_register);
void ak0991xna_register()
{
    (void)sensors_list_register(
        &akm3d.magnetic.sensor,
        &akm3d.magnetic.api);
    (void)sensors_list_register(
        &akm3d.magnetic_uncalib.sensor,
        &akm3d.magnetic_uncalib.api);
}
