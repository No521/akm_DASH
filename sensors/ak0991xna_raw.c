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

#define LOG_TAG "DASH - ak0991xna_raw"

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
#define AKM_USE_CONTINUOUS      0

static void *ak0991x_read(
    void *arg
);

static int ak0991x_set_interval(
    struct sensor_desc *d,
    int                interval
);

static int ak0991x_set_delay(
    struct sensor_api_t *s,
    int64_t             ns)
{
    struct sensor_desc *d = container_of(s, struct sensor_desc, api);
    int                err;

    if (ns < d->sensor.minDelay * 1000) {
        ns = d->sensor.minDelay * 1000;
    }

    d->applied_delay_ms = ns / (1000 * 1000); /* ms */

    err = ak0991x_set_interval(d, d->applied_delay_ms);

    if (err < 0) {
        ALOGE("%s: ecompass: failed to set interval "
              "- err = %d\n", __func__, err);
        return -1;
    }

    return 0;
}

static int ak0991x_set_interval(
    struct sensor_desc *d,
    int                interval)
{
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
    struct sensor_desc *d = container_of(s_api, struct sensor_desc, api);
    int                ret = 0;
    int                fd;

    /* check for availablity */
    fd = open_input_dev_by_name(d->input_name, O_RDONLY | O_NONBLOCK);

    if (fd < 0) {
        ALOGE("%s: failed to open input dev %s, error: %s\n",
              __func__, d->input_name, strerror(errno));
        ret = -1;
        goto exit;
    }

    close(fd);

    sensors_sysfs_init(&d->sysfs, d->input_name, SYSFS_TYPE_INPUT_DEV);
    sensors_select_init(&d->select_worker, ak0991x_read, d, -1);

exit:
    return ret;
}

static void ak0991x_close(struct sensor_api_t *s)
{
    struct sensor_desc *d = container_of(s, struct sensor_desc, api);

    d->select_worker.destroy(&d->select_worker);
}

static int ak0991x_activate(
    struct sensor_api_t *s,
    int                 enable)
{
    struct sensor_desc *d = container_of(s, struct sensor_desc, api);
    int                ret = 0;
    int                fd;

    fd = d->select_worker.get_fd(&d->select_worker);

    if (enable && (fd < 0)) {
        fd = open_input_dev_by_name(d->input_name,
                                    O_RDONLY | O_NONBLOCK);

        if (fd < 0) {
            ALOGE("%s: failed to open input dev %s, error: %s\n",
                  __func__, d->input_name, strerror(errno));
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
        }
    }

exit:
    return ret;
}

static void *ak0991x_read(void *arg)
{
    struct input_event     evbuf[10];
    struct input_event     *event;
    struct sensor_desc     *d = arg;
    int                    fd = d->select_worker.get_fd(&d->select_worker);
    int                    n;
    int                    i;
    struct sensors_event_t ev;
    static int             status = SENSOR_STATUS_ACCURACY_HIGH;

    n = read(fd, evbuf, sizeof(evbuf));

    if (n < 0) {
        ALOGE("%s: read error from fd %d, errno %d", __func__, fd, errno);
        goto exit;
    }

    n = n / sizeof(evbuf[0]);

    for (i = 0; i < n; i++) {
        event = evbuf + i;

        if (event->type == EV_SYN) {
            memset(&ev, 0, sizeof(ev));
            ev.timestamp = get_current_nano_time();
            ev.version = 0;
            ev.sensor = SENSOR_MAGNETIC_FIELD_HANDLE;
            ev.type = SENSOR_TYPE_MAGNETIC_FIELD;
            ev.magnetic.x = d->data[0] / 65536.f;
            ev.magnetic.y = d->data[1] / 65536.f;
            ev.magnetic.z = d->data[2] / 65536.f;
            ev.magnetic.status = status;
            sensors_fifo_put(&ev);
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

static struct sensor_desc ak0991xna_magnetic = {
    .sensor = {
        name: AKM_CHIP_NAME " Magnetic Field",
        vendor: "Asahi Kasei Corp.",
        version: sizeof(sensors_event_t),
        handle: SENSOR_MAGNETIC_FIELD_HANDLE,
        type: SENSOR_TYPE_MAGNETIC_FIELD,
        maxRange: AKM_CHIP_MAXRANGE,
        resolution: AKM_CHIP_RESOLUTION,
        power: AKM_CHIP_POWER,
        minDelay: 10000,
    },
    .api = {
        init: ak0991x_init,
        activate: ak0991x_activate,
        set_delay: ak0991x_set_delay,
        close: ak0991x_close,
    },
    .map_prefix = "ak0991xmagnetic",
    .input_name = AK0991X_NAME,
    .applied_delay_ms = 0,
};

list_constructor(ak0991x_init_driver);
void ak0991x_init_driver()
{
    sensors_list_register(&ak0991xna_magnetic.sensor, &ak0991xna_magnetic.api);
}
