/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_bk7259_platform.h"
#include "bk7259_ops.h"

#include "bk7259_platform_log.h"
#include <components/system.h>
#include <easyflash.h>
#include <mybot/platform/mybot_platform.h>
#include <os/os.h>

#define TAG "mybot_platform"

static bool s_prepared;

void mybot_bk7259_factory_reset(void) {
    /* Every persisted MyBot record (Wi-Fi credentials, device credentials,
     * volume) lives in the EasyFlash environment, so resetting the partition
     * is the whole factory reset. The AP environment has no vendor calibration
     * data. The caller must have stopped the SDK first. */
    MYBOT_LOGW(TAG, "factory reset: erasing persisted state");
    if (ef_env_set_default() != EF_NO_ERR) {
        MYBOT_LOGE(TAG, "factory reset: EasyFlash environment erase failed");
    }
    MYBOT_LOGW(TAG, "factory reset: rebooting");
    bk_reboot();
    for (;;) {
        (void)rtos_delay_milliseconds(1000);
    }
}

int mybot_bk7259_platform_prepare(void) {
    if (s_prepared) {
        return 0;
    }
    if (bk7259_lcd_prepare() < 0) {
        MYBOT_LOGE(TAG, "LCD preparation failed");
        return -1;
    }
    if (bk7259_wifi_prepare() < 0) {
        MYBOT_LOGE(TAG, "Wi-Fi preparation failed");
        bk7259_lcd_shutdown();
        return -1;
    }
    if (bk7259_key_prepare() < 0) {
        MYBOT_LOGE(TAG, "key preparation failed");
        bk7259_wifi_shutdown();
        bk7259_lcd_shutdown();
        return -1;
    }
    s_prepared = true;
    MYBOT_LOGI(TAG, "platform prepared");
    return 0;
}

int mybot_bk7259_platform_register(void) {
    static const mybot_platform_descriptor_t descriptor = {
        .wifi = &g_mybot_bk7259_wifi_ops,
        .kv_store = &g_mybot_bk7259_kv_ops,
        .key = &g_mybot_bk7259_key_ops,
        .audio_capture = &g_mybot_bk7259_capture_ops,
        .audio_playback = &g_mybot_bk7259_playback_ops,
        .audio_volume = &g_mybot_bk7259_volume_ops,
        .https = &g_mybot_bk7259_https_ops,
        .lcd = &g_mybot_bk7259_lcd_ops,
        .announce = &g_mybot_bk7259_announce_ops,
#if defined(CONFIG_MYBOT_VIDEO) && CONFIG_MYBOT_VIDEO
        .video = &g_mybot_bk7259_video_ops,
#else
        .video = NULL,
#endif
        .wake_words = NULL,
    };

    int result = mybot_platform_register(&descriptor);
    if (result < 0) {
        MYBOT_LOGE(TAG, "platform descriptor registration failed");
        return result;
    }
    MYBOT_LOGI(TAG, "platform descriptor registered");
    return 0;
}

void mybot_bk7259_platform_shutdown(void) {
    if (!s_prepared) {
        return;
    }
    MYBOT_LOGI(TAG, "platform shutting down");
    bk7259_key_shutdown();
    bk7259_wifi_shutdown();
    bk7259_lcd_shutdown();
    s_prepared = false;
    MYBOT_LOGI(TAG, "platform shut down");
}
