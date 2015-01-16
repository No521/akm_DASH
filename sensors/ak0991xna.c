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

#define LOG_TAG "DASH - ak0991xna"

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
#include "sensors_wrapper.h"

#define SETTING_FILE_NAME  "/data/misc/akm_set.bin"

#include "libs/libakm/AKL_APIs.h"
#include "libs/libakm/AKL_DASH_Ext.h"
#include "libs/libakm/AKM_CustomerSpec.h"

#if defined(AK0991X)
#include "libs/libakm/linux/ak0991x.h"
#elif defined(AK8963)
#include "libs/libakm/linux/ak8963.h"
#else
#error No AKM chip is defined
#endif

/* Orientation */
#define EVENT_CODE_MAGV_X       MSC_RX
#define EVENT_CODE_MAGV_Y       MSC_RY
#define EVENT_CODE_MAGV_Z       MSC_RZ
#define EVENT_CODE_MAGV_STATUS  MSC_ST2

#define AKM_MAX_INTERVAL        INT_MAX
#define AKM_USE_CONTINUOUS      1

static int ak0991x_init(
    struct sensor_api_t *s_api
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

static void *ak0991x_read(
    void *arg
);

static struct sensor_desc ak0991xna_magnetic = {
    .sensor = {
        name: AKM_CHIP_NAME " Magnetic Field (base)",
        vendor: "Asahi Kasei Corp.",
        version: sizeof(sensors_event_t),
        handle: SENSOR_MAGNETIC_FIELD_HANDLE,
        type: SENSOR_TYPE_MAGNETIC_FIELD,
        maxRange: AKM_CHIP_MAXRANGE,
        resolution: AKM_CHIP_RESOLUTION,
        power: AKM_CHIP_POWER,
        minDelay: 10000,
        /*TODO: for Lollipop*/
        /* uint32_t     fifoReservedEventCount;
         * uint32_t     fifoMaxEventCount;
         * const char*  stringType;
         * const char*  requiredPermission;
         * int32/64_t   maxDelay;
         * uint64_t     flags;
         */
    },
    .api = {
        init: ak0991x_init,
        activate: ak0991x_activate,
        set_delay: ak0991x_delay,
        close: ak0991x_close,
    },
    .input_name = AK0991X_NAME,
    .applied_delay_ms = 0,
};

static int ak0991x_start(void)
{
    uint16_t            sz;
    size_t              rsz;
    int16_t             ret = -1;
    int16_t             needpdc = 0;
    uint8_t             *buf = NULL;
    FILE                *fp = NULL;
    struct AKL_SCL_PRMS *mem;

    /* open setting file for read. */
    fp = fopen(SETTING_FILE_NAME, "rb");

    if (fp == NULL) {
        ALOGW("%s: file '%s' cannot open (%s)",
              __func__, SETTING_FILE_NAME, strerror(errno));
        /* When AKL_StartMeasurement is called with buf=NULL, */
        /*  default parameter will be set */
        needpdc = 1;
    } else {
        /* malloc parameter data area */
        sz = AKL_GetNVdataSize(AKM_CUSTOM_NUM_FORM);
        buf = (uint8_t *)malloc(sz);

        if (!buf) {
            ALOGE("%s: malloc failed.", __func__);
            goto exit;
        }

        /* read setting */
        rsz = fread(buf, sizeof(uint8_t), sz, fp);

        if (rsz != sz) {
            ALOGE("%s: Request %d bytes, but actually %d bytes read.",
                  __func__, sz, rsz);
            goto exit;
        }
    }

    /* library API */
    mem = AKL_DASH_Lock();

    if (AKL_StartMeasurement(mem, buf) != AKM_SUCCESS) {
        ALOGE("%s: failed to start library.", __func__);
        AKL_DASH_Unlock();
        goto exit;
    }

    AKL_DASH_Unlock();

    /* set PDC */
    if (needpdc) {
        /* TODO: read pdc parameter */
        /* AKL_SetPDC() */
    }

    /* Success */
    ret = 0;
    ALOGI("%s: finished successfully.", __func__);

exit:

    if (fp) {
        fclose(fp);
    }

    if (!buf) {
        free(buf);
    }

    return ret;
}

static int ak0991x_stop(void)
{
    uint16_t            sz;
    size_t              wsz;
    int16_t             ret = -1;
    uint8_t             *buf = NULL;
    FILE                *fp = NULL;
    struct AKL_SCL_PRMS *mem;

    /* malloc parameter data area */
    sz = AKL_GetNVdataSize(AKM_CUSTOM_NUM_FORM);
    buf = (uint8_t *)malloc(sz);

    if (!buf) {
        ALOGE("%s: malloc failed.", __func__);
        return -1;
    }

    /* call stop */
    mem = AKL_DASH_Lock();

    if (AKL_StopMeasurement(mem, buf) != AKM_SUCCESS) {
        ALOGE("%s: failed to stop library.", __func__);
        AKL_DASH_Unlock();
        goto exit;
    }

    AKL_DASH_Unlock();

    /* open setting file for write. */
    fp = fopen(SETTING_FILE_NAME, "wb");

    if (fp == NULL) {
        ALOGE("%s: file '%s' cannot open (%s)",
              __func__, SETTING_FILE_NAME, strerror(errno));
        goto exit;
    }

    /* write setting */
    wsz = fwrite(buf, sizeof(uint8_t), sz, fp);

    if (wsz != sz) {
        ALOGE("%s: Request %d bytes, but actually %d bytes wrote.",
              __func__, sz, wsz);
        goto exit;
    }

    /* Success */
    ret = 0;
    ALOGI("%s: finished successfully.", __func__);

exit:

    if (fp) {
        fclose(fp);
    }

    if (!buf) {
        free(buf);
    }

    return ret;
}

static int ak0991x_set_interval(
    struct sensor_desc *d,
    int                interval)
{
    ALOGD("%s: interval=%d.", __func__, interval);

    if (interval > AKM_MAX_INTERVAL) {
        interval = AKM_MAX_INTERVAL;
    }

    if (interval < 0) {
        interval = -1;
    }

#if AKM_USE_CONTINUOUS
    return d->sysfs.write_int(&d->sysfs, "continuous", interval);
#else
    return d->sysfs.write_int(&d->sysfs, "interval", interval);
#endif
}

static int ak0991x_init(struct sensor_api_t *s_api)
{
    struct sensor_desc *d = container_of(s_api, struct sensor_desc,
                                         api);
    int                           ret = 0;
    int                           fd;
    struct AKL_SCL_PRMS           *mem;

    /* check for availablity */
    fd = open_input_dev_by_name(d->input_name, O_RDONLY | O_NONBLOCK);

    if (fd < 0) {
        ALOGE("%s: failed to open input dev %s, error: %s\n",
              __func__, d->input_name, strerror(errno));
        ret = -1;
        goto err0;
    }

    close(fd);

    /* AKM library */
    ret = AKL_DASH_Init(AKM_CUSTOM_NUM_FORM);

    if (ret != AKM_SUCCESS) {
        ret = -1;
        goto err0;
    }

    /* NOTE: The second argument should be a pointer to
     * AKL_CERTIFICATION_DASH struct.
     */
    mem = AKL_DASH_Lock();
    ret = AKL_Init(
            mem,
            NULL,
            AKM_CUSTOM_NUM_FORM);
    AKL_DASH_Unlock();

    if (ret != AKM_SUCCESS) {
        ALOGE("%s: AKL_Init Error !", __func__);
        ret = -1;
        goto err1;
    }

    sensors_sysfs_init(&d->sysfs, d->input_name, SYSFS_TYPE_INPUT_DEV);
    sensors_select_init(&d->select_worker, ak0991x_read, d, -1);

    return ret;

err1:
    AKL_DASH_Deinit();
err0:
    return ret;
}

static int ak0991x_activate(
    struct sensor_api_t *s,
    int                 enable)
{
    struct sensor_desc *d = container_of(s, struct sensor_desc, api);
    int                ret = 0;
    int                fd;

    fd = d->select_worker.get_fd(&d->select_worker);

    ALOGD("%s: called by '%s' en=%d, fd=%d",
          __func__, d->sensor.name, enable, fd);

    if (enable && (fd < 0)) {
        fd = open_input_dev_by_name(d->input_name,
                                    O_RDONLY | O_NONBLOCK);

        if (fd < 0) {
            ALOGE("%s: failed to open input dev %s, error: %s\n",
                  __func__, d->input_name, strerror(errno));
            ret = -1;
            goto exit;
        }

        /* start AKM library */
        ret = ak0991x_start();

        if (ret < 0) {
            ret = -1;
            goto exit;
        }

        d->select_worker.set_fd(&d->select_worker, fd);
        d->select_worker.resume(&d->select_worker);
    } else if (!enable && (fd > 0)) {
        d->select_worker.suspend(&d->select_worker);
        d->select_worker.set_fd(&d->select_worker, -1);

        ret = ak0991x_set_interval(d, -1);

        if (ret < 0) {
            ALOGE("%s: ecompass: failed to set"
                  " interval - ret = %d\n", __func__, ret);
            ret = -1;
            goto exit;
        }

        ret = ak0991x_stop();

        if (ret < 0) {
            ret = -1;
            goto exit;
        }
    }

exit:
    return ret;
}

static int ak0991x_delay(
    struct sensor_api_t *s,
    int64_t             ns)
{
    struct sensor_desc *d = container_of(s, struct sensor_desc, api);
    int                err;

    ALOGD("%s: called by '%s' ns=%lld",
          __func__, d->sensor.name, ns);

    if (ns < 0) {
        d->applied_delay_ms = -1;
    } else {
        if (ns < d->sensor.minDelay * 1000) {
            ns = d->sensor.minDelay * 1000;
        }

        d->applied_delay_ms = ns / (1000 * 1000); /* ms */
    }

    err = ak0991x_set_interval(d, d->applied_delay_ms);

    if (err < 0) {
        ALOGE("%s: ecompass: failed to set interval "
              "- err = %d\n", __func__, err);
        return -1;
    }

    return 0;
}

static void ak0991x_close(struct sensor_api_t *s)
{
    struct sensor_desc *d = container_of(s, struct sensor_desc, api);

    d->select_worker.destroy(&d->select_worker);
    AKL_DASH_Deinit();
}

static void *ak0991x_read(void *arg)
{
    struct input_event     evbuf[10];
    struct input_event     *event;
    struct sensor_desc     *d = arg;
    int                    fd = d->select_worker.get_fd(&d->select_worker);
    int                    n;
    int                    i;
    struct sensor_data_t   sd;
    static int             status = SENSOR_STATUS_ACCURACY_HIGH;
    struct AKM_SENSOR_DATA akm_data;
    struct AKL_SCL_PRMS    *mem;
    int                    err;

    n = read(fd, evbuf, sizeof(evbuf));

    if (n < 0) {
        ALOGE("%s: read error from fd %d, errno %d", __func__, fd, errno);
        goto exit;
    }

    n = n / sizeof(evbuf[0]);

    for (i = 0; i < n; i++) {
        event = evbuf + i;

        if (event->type == EV_SYN) {
            memset(&sd, 0, sizeof(sd));
            sd.sensor = &d->sensor;
            sd.data = d->data;
            sd.scale = 1.0f / 65536.0f;
            sd.status = status;
            sd.timestamp = get_current_nano_time();
            sd.delay = d->applied_delay_ms;

            akm_data.u.s.x = sd.data[AXIS_X];
            akm_data.u.s.y = sd.data[AXIS_Y];
            akm_data.u.s.z = sd.data[AXIS_Z];
            akm_data.stype = AKM_ST_MAG;
            akm_data.time_us = sd.timestamp / 1000;
            /* ST1 value is not reported from driver. Use dummy data. */
            akm_data.status[0] = 1;
            akm_data.status[1] = sd.status;
            /* set data to library */
            mem = AKL_DASH_Lock();
            err = AKL_SetVector(mem, &akm_data, 1);
            AKL_DASH_Unlock();

            if (err && (AKM_ERR_NOT_SUPPORT != err)) {
                ALOGE("%s,%d: AKL_SetVector Error (%d)!",
                      __func__, __LINE__, err);
            }

            sensors_wrapper_data(&sd);
        }

        if (event->type != EV_MSC) {
            continue;
        }

        switch (event->code) {
        case EVENT_CODE_MAGV_STATUS:
            status = event->value;
            break;

        case EVENT_CODE_MAGV_X:
            d->data[0] = event->value;
            break;

        case EVENT_CODE_MAGV_Y:
            d->data[1] = event->value;
            break;

        case EVENT_CODE_MAGV_Z:
            d->data[2] = event->value;
            break;
        }
    }

exit:
    return NULL;
}

list_constructor(ak0991x_init_driver);
void ak0991x_init_driver()
{
    sensors_wrapper_register(
        &ak0991xna_magnetic.sensor,
        &ak0991xna_magnetic.api,
        &ak0991xna_magnetic.entry);
}
