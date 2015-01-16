LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := -Wextra -Wall

# Specify device number
ifeq ($(SOMC_CFG_SENSORS_COMPASS_AK09911),yes)
AKM_CFG_DEVICE_NUMBER := 09911
LOCAL_CFLAGS += -DAKM_MAGNETOMETER_AK09911
else ifeq ($(SOMC_CFG_SENSORS_COMPASS_AK09912),yes)
AKM_CFG_DEVICE_NUMBER := 09912
LOCAL_CFLAGS += -DAKM_MAGNETOMETER_AK09912
else ifeq ($(SOMC_CFG_SENSORS_COMPASS_AK8963),yes)
AKM_CFG_DEVICE_NUMBER := 8963
LOCAL_CFLAGS += -DAKM_MAGNETOMETER_AK8963
else
AKM_CFG_DEVICE_NUMBER := 0
endif

ifneq ($(AKM_CFG_DEVICE_NUMBER),0)
# OSS solution
LOCAL_CFLAGS += -DAKMOSS -DAKL_SKIP_HW_CHECK
SCL_LIB_DIR := oss
SCL_LIB_NAME := oss

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SCL_LIB_DIR)
LOCAL_SRC_FILES := common/akl_dash_ext.c
LOCAL_LDFLAGS := -Wl,--version-script,$(LOCAL_PATH)/common/akl_apis.map

LOCAL_SRC_FILES += $(SCL_LIB_DIR)/akfs_aoc.c \
				   $(SCL_LIB_DIR)/akfs_device.c \
				   $(SCL_LIB_DIR)/akfs_direction.c \
				   $(SCL_LIB_DIR)/akfs_measure.c \
				   $(SCL_LIB_DIR)/akfs_vnorm.c \
				   $(SCL_LIB_DIR)/akl_apis.c

LOCAL_MODULE := libsensors_ak$(AKM_CFG_DEVICE_NUMBER)
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := liblog libcutils

## For Debug
LOCAL_CFLAGS += -DDEBUG
# output map file
#LOCAL_LDFLAGS += -Wl,-Map=$(LOCAL_PATH)/$(LOCAL_MODULE).map

include $(BUILD_SHARED_LIBRARY)
endif
