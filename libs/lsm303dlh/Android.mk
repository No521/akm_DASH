ifeq ($(SOMC_CFG_SENSORS_COMPASS_LSM303DLH),yes)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= sensors_compass_API_dummy.c
LOCAL_MODULE := libLSM303DLH
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)
endif
