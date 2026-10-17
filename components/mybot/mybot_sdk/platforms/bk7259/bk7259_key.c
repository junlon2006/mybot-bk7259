/* SPDX-License-Identifier: Apache-2.0 */
#include "bk7259_ops.h"
#include "mybot_bk7259_platform.h"

#include <adc_key_main.h>
#include <common/bk_err.h>
#include "bk7259_platform_log.h"
#include <mybot/mybot.h>
#include <os/os.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAG "mybot_key"

/* Four logical keys. Robot V2 wires them as two resistor ladders: S2 and S3
 * share ADC channel 1, S4 and S5 share ADC channel 2. The fifth button on the
 * board is wired to the SoC CEN pin and resets the chip in hardware, so the
 * firmware never sees it. */
#define MYBOT_KEY_ITEM_MAX 4

/* A physical key carries a hardware description (channel plus voltage window)
 * and a function identifier. The function is selected in Kconfig so a board
 * revision can remap a button without touching this file. */
enum {
    KEY_FUNC_NONE = 0,
    KEY_FUNC_CONVERSATION,
    KEY_FUNC_VOLUME_UP,
    KEY_FUNC_VOLUME_DOWN,
    KEY_FUNC_FACTORY_RESET,
    KEY_FUNC_RESERVED,
    KEY_FUNC_COUNT,
};

/* Kconfig models each key's function as a single-choice list, so it emits one
 * CONFIG_MYBOT_KEY_<KEY>_<FUNC> symbol for the selection and leaves the others
 * undefined. Fold the selection back into the id the dispatch table indexes on;
 * the #error catches a key whose choice block lost its default. */
#if defined(CONFIG_MYBOT_KEY_S2_NONE)
#define MYBOT_KEY_FUNC_S2 KEY_FUNC_NONE
#elif defined(CONFIG_MYBOT_KEY_S2_CONVERSATION)
#define MYBOT_KEY_FUNC_S2 KEY_FUNC_CONVERSATION
#elif defined(CONFIG_MYBOT_KEY_S2_VOLUME_UP)
#define MYBOT_KEY_FUNC_S2 KEY_FUNC_VOLUME_UP
#elif defined(CONFIG_MYBOT_KEY_S2_VOLUME_DOWN)
#define MYBOT_KEY_FUNC_S2 KEY_FUNC_VOLUME_DOWN
#elif defined(CONFIG_MYBOT_KEY_S2_FACTORY_RESET)
#define MYBOT_KEY_FUNC_S2 KEY_FUNC_FACTORY_RESET
#elif defined(CONFIG_MYBOT_KEY_S2_RESERVED)
#define MYBOT_KEY_FUNC_S2 KEY_FUNC_RESERVED
#else
#error "MyBot S2 has no function selected"
#endif

#if defined(CONFIG_MYBOT_KEY_S3_NONE)
#define MYBOT_KEY_FUNC_S3 KEY_FUNC_NONE
#elif defined(CONFIG_MYBOT_KEY_S3_CONVERSATION)
#define MYBOT_KEY_FUNC_S3 KEY_FUNC_CONVERSATION
#elif defined(CONFIG_MYBOT_KEY_S3_VOLUME_UP)
#define MYBOT_KEY_FUNC_S3 KEY_FUNC_VOLUME_UP
#elif defined(CONFIG_MYBOT_KEY_S3_VOLUME_DOWN)
#define MYBOT_KEY_FUNC_S3 KEY_FUNC_VOLUME_DOWN
#elif defined(CONFIG_MYBOT_KEY_S3_FACTORY_RESET)
#define MYBOT_KEY_FUNC_S3 KEY_FUNC_FACTORY_RESET
#elif defined(CONFIG_MYBOT_KEY_S3_RESERVED)
#define MYBOT_KEY_FUNC_S3 KEY_FUNC_RESERVED
#else
#error "MyBot S3 has no function selected"
#endif

#if defined(CONFIG_MYBOT_KEY_S4_NONE)
#define MYBOT_KEY_FUNC_S4 KEY_FUNC_NONE
#elif defined(CONFIG_MYBOT_KEY_S4_CONVERSATION)
#define MYBOT_KEY_FUNC_S4 KEY_FUNC_CONVERSATION
#elif defined(CONFIG_MYBOT_KEY_S4_VOLUME_UP)
#define MYBOT_KEY_FUNC_S4 KEY_FUNC_VOLUME_UP
#elif defined(CONFIG_MYBOT_KEY_S4_VOLUME_DOWN)
#define MYBOT_KEY_FUNC_S4 KEY_FUNC_VOLUME_DOWN
#elif defined(CONFIG_MYBOT_KEY_S4_FACTORY_RESET)
#define MYBOT_KEY_FUNC_S4 KEY_FUNC_FACTORY_RESET
#elif defined(CONFIG_MYBOT_KEY_S4_RESERVED)
#define MYBOT_KEY_FUNC_S4 KEY_FUNC_RESERVED
#else
#error "MyBot S4 has no function selected"
#endif

#if defined(CONFIG_MYBOT_KEY_S5_NONE)
#define MYBOT_KEY_FUNC_S5 KEY_FUNC_NONE
#elif defined(CONFIG_MYBOT_KEY_S5_CONVERSATION)
#define MYBOT_KEY_FUNC_S5 KEY_FUNC_CONVERSATION
#elif defined(CONFIG_MYBOT_KEY_S5_VOLUME_UP)
#define MYBOT_KEY_FUNC_S5 KEY_FUNC_VOLUME_UP
#elif defined(CONFIG_MYBOT_KEY_S5_VOLUME_DOWN)
#define MYBOT_KEY_FUNC_S5 KEY_FUNC_VOLUME_DOWN
#elif defined(CONFIG_MYBOT_KEY_S5_FACTORY_RESET)
#define MYBOT_KEY_FUNC_S5 KEY_FUNC_FACTORY_RESET
#elif defined(CONFIG_MYBOT_KEY_S5_RESERVED)
#define MYBOT_KEY_FUNC_S5 KEY_FUNC_RESERVED
#else
#error "MyBot S5 has no function selected"
#endif

typedef struct {
    const char *name;
    adc_key_id_t id;
    adc_chan_t chan;
    uint16_t mv_low;
    uint16_t mv_high;
    uint8_t function;
} key_item_t;

typedef struct {
    beken_mutex_t lock;
    beken_semaphore_t callback_idle;
    beken_semaphore_t provision_request;
    beken_semaphore_t reset_request;
    mybot_key_event_handler_t emit;
    void *user_data;
    unsigned int in_flight;
    adc_key_handle_t handles[MYBOT_KEY_ITEM_MAX];
    size_t handle_count;
    bool prepared;
    bool adc_key_owned;
    bool sdk_attached;
    bool detaching;
} key_state_t;

static key_state_t s_key;

static void key_conversation_short_press(void);
static void key_conversation_long_press(void);
static void key_volume_up_short_press(void);
static void key_volume_down_short_press(void);
static void key_factory_reset_long_press(void);
static void key_reserved_short_press(void);

/* Dispatch table indexed by the Kconfig function id. The two keys that consume
 * a long press are the conversation key, which requests reprovisioning, and the
 * factory-reset key. On the others a long press stays NULL so holding the
 * button does nothing beyond the driver's own logging. */
static const struct {
    void (*short_press)(void);
    void (*long_press)(void);
} s_key_functions[KEY_FUNC_COUNT] = {
    [KEY_FUNC_NONE] = { NULL, NULL },
    [KEY_FUNC_CONVERSATION] = { key_conversation_short_press,
                                key_conversation_long_press },
    [KEY_FUNC_VOLUME_UP] = { key_volume_up_short_press, NULL },
    [KEY_FUNC_VOLUME_DOWN] = { key_volume_down_short_press, NULL },
    /* Long press only: an erase must not be reachable by a single touch. */
    [KEY_FUNC_FACTORY_RESET] = { NULL, key_factory_reset_long_press },
    [KEY_FUNC_RESERVED] = { key_reserved_short_press, NULL },
};

/* key_id is the driver's identifier and doubles as the "key=N" field in the
 * adc_key component's own press log, which is what makes a raw voltage reading
 * traceable back to a button name while calibrating the windows. */
static const key_item_t s_key_items[] = {
    {
        .name = "S2",
        .id = 2U,
        .chan = (adc_chan_t)CONFIG_MYBOT_KEY_ADC_CH1_CHAN,
        .mv_low = (uint16_t)CONFIG_MYBOT_KEY_S2_MV_LOW,
        .mv_high = (uint16_t)CONFIG_MYBOT_KEY_S2_MV_HIGH,
        .function = (uint8_t)MYBOT_KEY_FUNC_S2,
    },
    {
        .name = "S3",
        .id = 3U,
        .chan = (adc_chan_t)CONFIG_MYBOT_KEY_ADC_CH1_CHAN,
        .mv_low = (uint16_t)CONFIG_MYBOT_KEY_S3_MV_LOW,
        .mv_high = (uint16_t)CONFIG_MYBOT_KEY_S3_MV_HIGH,
        .function = (uint8_t)MYBOT_KEY_FUNC_S3,
    },
    {
        .name = "S4",
        .id = 4U,
        .chan = (adc_chan_t)CONFIG_MYBOT_KEY_ADC_CH2_CHAN,
        .mv_low = (uint16_t)CONFIG_MYBOT_KEY_S4_MV_LOW,
        .mv_high = (uint16_t)CONFIG_MYBOT_KEY_S4_MV_HIGH,
        .function = (uint8_t)MYBOT_KEY_FUNC_S4,
    },
    {
        .name = "S5",
        .id = 5U,
        .chan = (adc_chan_t)CONFIG_MYBOT_KEY_ADC_CH2_CHAN,
        .mv_low = (uint16_t)CONFIG_MYBOT_KEY_S5_MV_LOW,
        .mv_high = (uint16_t)CONFIG_MYBOT_KEY_S5_MV_HIGH,
        .function = (uint8_t)MYBOT_KEY_FUNC_S5,
    },
};

#define KEY_ITEM_COUNT (sizeof(s_key_items) / sizeof(s_key_items[0]))

static const char *key_function_name(uint8_t function) {
    static const char *const names[KEY_FUNC_COUNT] = {
        "none", "conversation", "volume up", "volume down", "factory reset",
        "reserved",
    };

    return function < KEY_FUNC_COUNT ? names[function] : "invalid";
}

static void emit_key_event(mybot_key_event_t event) {
    mybot_key_event_handler_t emit;
    void *user_data;

    rtos_lock_mutex(&s_key.lock);
    if (!s_key.sdk_attached || s_key.detaching) {
        rtos_unlock_mutex(&s_key.lock);
        return;
    }
    s_key.in_flight++;
    emit = s_key.emit;
    user_data = s_key.user_data;
    rtos_unlock_mutex(&s_key.lock);

    emit(event, user_data);

    rtos_lock_mutex(&s_key.lock);
    s_key.in_flight--;
    if (s_key.detaching && s_key.in_flight == 0) {
        (void)rtos_set_semaphore(&s_key.callback_idle);
    }
    rtos_unlock_mutex(&s_key.lock);
}

static bool key_is_attached(void) {
    bool attached;

    rtos_lock_mutex(&s_key.lock);
    attached = s_key.sdk_attached && !s_key.detaching;
    rtos_unlock_mutex(&s_key.lock);
    return attached;
}

static void key_conversation_short_press(void) {
    if (!key_is_attached()) {
        return;
    }
    switch (mybot_get_state()) {
    case MYBOT_STATE_READY:
        MYBOT_LOGI(TAG, "conversation key short press: start");
        emit_key_event(MYBOT_KEY_EVENT_CONVERSATION_START);
        break;
    case MYBOT_STATE_IN_CONVERSATION:
        MYBOT_LOGI(TAG, "conversation key short press: stop");
        emit_key_event(MYBOT_KEY_EVENT_CONVERSATION_STOP);
        break;
    default:
        MYBOT_LOGI(TAG, "conversation key short press ignored");
        break;
    }
}

/* The SDK has to be stopped before Wi-Fi is handed to APSTA, so the callback
 * only posts a request and the application loop starts the portal after
 * mybot_stop() has returned. */
static void key_conversation_long_press(void) {
    if (rtos_set_semaphore(&s_key.provision_request) != BK_OK) {
        MYBOT_LOGW(TAG, "conversation key long press: failed to post provisioning "
                     "request");
        return;
    }
    MYBOT_LOGI(TAG, "conversation key long press: provisioning requested");
}

static void key_volume_up_short_press(void) {
    if (!key_is_attached()) {
        return;
    }
    MYBOT_LOGI(TAG, "volume up key short press");
    emit_key_event(MYBOT_KEY_EVENT_VOLUME_UP);
}

static void key_volume_down_short_press(void) {
    if (!key_is_attached()) {
        return;
    }
    MYBOT_LOGI(TAG, "volume down key short press");
    emit_key_event(MYBOT_KEY_EVENT_VOLUME_DOWN);
}

/* The erase has to run with the SDK stopped, so the callback only posts a
 * request and the application loop performs it after mybot_stop(). */
static void key_factory_reset_long_press(void) {
    if (rtos_set_semaphore(&s_key.reset_request) != BK_OK) {
        MYBOT_LOGW(TAG, "factory reset key long press: failed to post reset "
                     "request");
        return;
    }
    MYBOT_LOGW(TAG, "factory reset key long press: factory reset requested");
}

static void key_reserved_short_press(void) {
    MYBOT_LOGI(TAG, "reserved key short press (unassigned)");
}

/* Runs on the adc_key component's timer callback while it holds its own
 * manager lock, so this must stay short. The measured voltage for the press
 * comes from the component's "PRESS_DOWN: adc=..mV range=[..] key=N" line,
 * which the key_id in s_key_items[] ties back to a button name. */
static void key_dispatch(const key_item_t *item, bool long_press) {
    void (*handler)(void);

    if (item->function >= KEY_FUNC_COUNT) {
        MYBOT_LOGW(TAG, "%s: unknown function %u", item->name,
                (unsigned)item->function);
        return;
    }
    handler = long_press ? s_key_functions[item->function].long_press
                         : s_key_functions[item->function].short_press;
    if (handler == NULL) {
        MYBOT_LOGI(TAG, "%s: %s press is unassigned", item->name,
                long_press ? "long" : "short");
        return;
    }
    handler();
}

static void key_adc_short_press(void *arg) {
    key_dispatch((const key_item_t *)arg, false);
}

static void key_adc_long_press(void *arg) {
    key_dispatch((const key_item_t *)arg, true);
}

static void key_detach(void) {
    bool wait_for_callback;

    rtos_lock_mutex(&s_key.lock);
    if (!s_key.sdk_attached) {
        rtos_unlock_mutex(&s_key.lock);
        return;
    }
    s_key.sdk_attached = false;
    s_key.detaching = true;
    s_key.emit = NULL;
    s_key.user_data = NULL;
    wait_for_callback = s_key.in_flight != 0;
    rtos_unlock_mutex(&s_key.lock);

    if (wait_for_callback) {
        (void)rtos_get_semaphore(&s_key.callback_idle, BEKEN_WAIT_FOREVER);
    }

    rtos_lock_mutex(&s_key.lock);
    s_key.detaching = false;
    rtos_unlock_mutex(&s_key.lock);
    MYBOT_LOGI(TAG, "key callbacks detached");
}

static void key_release_framework(void) {
    if (s_key.adc_key_owned) {
        if (bk_adc_key_deinit_ex() != BK_OK) {
            MYBOT_LOGW(TAG, "ADC key manager deinitialization failed");
        }
        s_key.adc_key_owned = false;
    }
    s_key.handle_count = 0;
    rtos_deinit_semaphore(&s_key.reset_request);
    rtos_deinit_semaphore(&s_key.provision_request);
    rtos_deinit_semaphore(&s_key.callback_idle);
    rtos_deinit_mutex(&s_key.lock);
}

static int key_register(const key_item_t *item) {
    adc_key_item_config_ex_t config = {
        .size = sizeof(adc_key_item_config_ex_t),
        .version = ADC_KEY_CONFIG_VERSION,
        .key_id = item->id,
        /* The adc_key component never reads gpio_id and never muxes the pin;
         * the ADC pad state comes from usr_gpio_cfg.h at boot. It stays zero
         * rather than repeating a Kconfig value the driver ignores. */
        .gpio_id = (gpio_id_t)0,
        .adc_chan = item->chan,
        .lowest_level = item->mv_low,
        .highest_level = item->mv_high,
        .short_press_cb = key_adc_short_press,
        .double_press_cb = NULL,
        .long_press_cb = key_adc_long_press,
        .hold_press_cb = NULL,
        .user_data = (void *)item,
    };
    adc_key_handle_t handle = NULL;

    if (s_key.handle_count >= MYBOT_KEY_ITEM_MAX) {
        MYBOT_LOGE(TAG, "key table holds at most %u entries",
                (unsigned)MYBOT_KEY_ITEM_MAX);
        return -1;
    }
    if (bk_adc_key_item_configure_ex(&config, &handle) != BK_OK) {
        MYBOT_LOGE(TAG, "key %s configuration failed (chan=%u window=%u..%umV)",
                item->name, (unsigned)item->chan, (unsigned)item->mv_low,
                (unsigned)item->mv_high);
        return -1;
    }
    s_key.handles[s_key.handle_count++] = handle;
    return 0;
}

int bk7259_key_prepare(void) {
    static const adc_key_driver_config_t driver_config = {
        .size = sizeof(adc_key_driver_config_t),
        .version = ADC_KEY_CONFIG_VERSION,
        .sample_period_ms = CONFIG_ADC_KEY_SAMPLE_PERIOD_MS,
        .max_channels = 2U,
        .max_items = MYBOT_KEY_ITEM_MAX,
    };

    if (s_key.prepared) {
        return 0;
    }
    if (rtos_init_mutex(&s_key.lock) != BK_OK) {
        MYBOT_LOGE(TAG, "key mutex initialization failed");
        return -1;
    }
    if (rtos_init_semaphore(&s_key.callback_idle, 1) != BK_OK) {
        MYBOT_LOGE(TAG, "key callback barrier initialization failed");
        rtos_deinit_mutex(&s_key.lock);
        return -1;
    }
    if (rtos_init_semaphore(&s_key.provision_request, 1) != BK_OK) {
        MYBOT_LOGE(TAG, "key provisioning semaphore initialization failed");
        rtos_deinit_semaphore(&s_key.callback_idle);
        rtos_deinit_mutex(&s_key.lock);
        return -1;
    }
    if (rtos_init_semaphore(&s_key.reset_request, 1) != BK_OK) {
        MYBOT_LOGE(TAG, "key reset semaphore initialization failed");
        rtos_deinit_semaphore(&s_key.provision_request);
        rtos_deinit_semaphore(&s_key.callback_idle);
        rtos_deinit_mutex(&s_key.lock);
        return -1;
    }

    if (bk_adc_key_init_ex(&driver_config) != BK_OK) {
        MYBOT_LOGE(TAG, "ADC key manager initialization failed; is the SARADC "
                     "sampler running on the CP?");
        goto fail;
    }
    s_key.adc_key_owned = true;

    for (size_t i = 0; i < KEY_ITEM_COUNT; ++i) {
        if (key_register(&s_key_items[i]) < 0) {
            goto fail;
        }
    }

    s_key.prepared = true;
    MYBOT_LOGI(TAG, "key ready: items=%u ch=%u,%u",
            (unsigned)s_key.handle_count,
            (unsigned)CONFIG_MYBOT_KEY_ADC_CH1_CHAN,
            (unsigned)CONFIG_MYBOT_KEY_ADC_CH2_CHAN);
    for (size_t i = 0; i < KEY_ITEM_COUNT; ++i) {
        const key_item_t *item = &s_key_items[i];
        MYBOT_LOGI(TAG, "%s: id=%u chan=%u window=%u..%umV func=%s", item->name,
                (unsigned)item->id, (unsigned)item->chan,
                (unsigned)item->mv_low, (unsigned)item->mv_high,
                key_function_name(item->function));
    }
    return 0;

fail:
    key_release_framework();
    return -1;
}

void bk7259_key_shutdown(void) {
    if (!s_key.prepared) {
        return;
    }

    key_detach();
    key_release_framework();
    s_key.prepared = false;
    MYBOT_LOGI(TAG, "key stopped");
}

bool mybot_bk7259_wait_provision_request(uint32_t timeout_ms) {
    if (!s_key.prepared) {
        return false;
    }
    return rtos_get_semaphore(&s_key.provision_request, timeout_ms) == BK_OK;
}

bool mybot_bk7259_wait_reset_request(uint32_t timeout_ms) {
    if (!s_key.prepared) {
        return false;
    }
    return rtos_get_semaphore(&s_key.reset_request, timeout_ms) == BK_OK;
}

static int key_init(void **out_ctx, mybot_key_event_handler_t emit, void *user_data) {
    if (!out_ctx || !emit || !s_key.prepared) {
        return -1;
    }
    *out_ctx = NULL;

    rtos_lock_mutex(&s_key.lock);
    if (s_key.sdk_attached || s_key.detaching) {
        rtos_unlock_mutex(&s_key.lock);
        return -1;
    }
    while (rtos_get_semaphore(&s_key.callback_idle, 0) == BK_OK) {
    }
    s_key.emit = emit;
    s_key.user_data = user_data;
    *out_ctx = &s_key;
    s_key.sdk_attached = true;
    rtos_unlock_mutex(&s_key.lock);
    MYBOT_LOGI(TAG, "key callbacks attached");
    return 0;
}

static void key_destroy(void *opaque) {
    if (opaque == &s_key && s_key.prepared) {
        key_detach();
    }
}

const mybot_key_ops_t g_mybot_bk7259_key_ops = {
    .init = key_init,
    .destroy = key_destroy,
};
