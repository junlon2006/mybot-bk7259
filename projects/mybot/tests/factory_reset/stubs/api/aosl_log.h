/* SPDX-License-Identifier: Apache-2.0 */
#ifndef FACTORY_RESET_TEST_AOSL_LOG_H_
#define FACTORY_RESET_TEST_AOSL_LOG_H_
#define AOSL_LOG_NOTICE 5
#define AOSL_LOG_WARNING 4
#define AOSL_LOG_ERROR 3
#define AOSL_LOG_DEBUG 7
void aosl_log(int level, const char *format, ...);
#endif
