/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_BK7259_PLATFORM_H_
#define MYBOT_BK7259_PLATFORM_H_

#include <stdbool.h>
#include <stdint.h>

/* Build-time language selection shared by the AP lifecycle and prompt
 * implementation. The service endpoint and embedded asset tag stay coupled. */
#if defined(CONFIG_MYBOT_LANGUAGE_EN_US) && CONFIG_MYBOT_LANGUAGE_EN_US
#define MYBOT_LANGUAGE_TAG "en-US"
#define MYBOT_SERVER_BASE "https://botstation.agora.io/api"
#else
#define MYBOT_LANGUAGE_TAG "zh-CN"
/* The /api prefix is part of the device service route. */
#define MYBOT_SERVER_BASE "https://botstation.shengwang.cn/api"
#endif

#define MYBOT_ASSETS_DIR "mybot/assets"

#ifdef __cplusplus
extern "C" {
#endif

int mybot_bk7259_platform_prepare(void);
int mybot_bk7259_platform_register(void);
/* Returns 1 when APSTA was required, 0 for an existing connection, -1 on
 * failure, or BK7259_NETWORK_CANCELLED (bk7259_factory_reset.h) for reset. */
int mybot_bk7259_ensure_network(const char *device_id);
int mybot_bk7259_provision_wifi(const char *device_id);
bool mybot_bk7259_wait_provision_request(uint32_t timeout_ms);
void mybot_bk7259_platform_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_BK7259_PLATFORM_H_ */
