
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= synergyclient.cpp

LOCAL_MODULE:= synergyclient
LOCAL_MODULE_TAGS:=optional
LOCAL_CFLAGS += -DFORANDROID

#LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../include external/wpa_supplicant_8
#LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libklaatu_wifi libhardware libhardware_legacy libnetutils

include $(BUILD_EXECUTABLE)

# Normally optional modules are not installed unless they show
# up in the PRODUCT_PACKAGES list

ALL_DEFAULT_INSTALLED_MODULES += $(TARGET_OUT)/bin/synergyclient

