/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RESET_OWNER_TEST_HOST_H_
#define RESET_OWNER_TEST_HOST_H_
#include <stddef.h>
#include <stdint.h>
#define BK_OK 0
#define BEKEN_DEFAULT_WORKER_PRIORITY 5
typedef void *beken_thread_arg_t;
typedef void *beken_thread_t;
typedef void (*beken_thread_function_t)(beken_thread_arg_t);
int bk_init(void);
int bk_uid_get_data(uint8_t *uid);
int media_service_init(void);
void bk_reboot(void);
int rtos_create_psram_thread(beken_thread_t *thread, uint8_t priority, const char *name,
                             beken_thread_function_t function, uint32_t stack_size,
                             beken_thread_arg_t arg);
int rtos_delete_thread(beken_thread_t *thread);
int rtos_delay_milliseconds(uint32_t milliseconds);
void aosl_ctor(void);
void aosl_dtor(void);
void aosl_set_log_level(int level);
void test_log(const char *format, ...);
#define AOSL_LOG_NOTICE 5
#define BK_LOGI(tag, ...) test_log(__VA_ARGS__)
#define BK_LOGW(tag, ...) test_log(__VA_ARGS__)
#define BK_LOGE(tag, ...) test_log(__VA_ARGS__)
#endif
