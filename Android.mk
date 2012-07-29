LOCAL_PATH := $(call my-dir)

common_src_files := \
    hotplug.c 
#
# Executable
#

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    $(common_src_files)

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libusb

LOCAL_STATIC_LIBRARIES := 

LOCAL_MODULE := hotplugd

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
