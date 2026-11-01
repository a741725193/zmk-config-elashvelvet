#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

LOG_MODULE_REGISTER(velvet_display_power, CONFIG_ZMK_LOG_LEVEL);

#define DISP_BL_NODE DT_NODELABEL(disp_bl)
#define DISP_BL_LED DT_NODE_CHILD_IDX(DISP_BL_NODE)

#ifdef CONFIG_PROSPECTOR_FIXED_BRIGHTNESS
#define DISP_BL_ON_BRIGHTNESS CONFIG_PROSPECTOR_FIXED_BRIGHTNESS
#else
#define DISP_BL_ON_BRIGHTNESS 80
#endif

static const struct device *const disp_bl_leds = DEVICE_DT_GET_ONE(pwm_leds);

static int velvet_display_activity_cb(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    int rc;

    if (ev == NULL || !device_is_ready(disp_bl_leds)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE:
        rc = led_set_brightness(disp_bl_leds, DISP_BL_LED, DISP_BL_ON_BRIGHTNESS);
        break;
    case ZMK_ACTIVITY_IDLE:
    case ZMK_ACTIVITY_SLEEP:
        rc = led_set_brightness(disp_bl_leds, DISP_BL_LED, 0);
        break;
    default:
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (rc < 0) {
        LOG_WRN("Failed to set display backlight (%d)", rc);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(velvet_display_power, velvet_display_activity_cb);
ZMK_SUBSCRIPTION(velvet_display_power, zmk_activity_state_changed);

#if CONFIG_VELVET_MIN_IDLE_TIMEOUT_MS > 0
static void velvet_idle_timeout_work_cb(struct k_work *work) {
    uint32_t current = zmk_activity_get_idle_ms();

    if (current > 0 && current < CONFIG_VELVET_MIN_IDLE_TIMEOUT_MS) {
        LOG_INF("Idle timeout %u -> %u ms", current,
                (uint32_t)CONFIG_VELVET_MIN_IDLE_TIMEOUT_MS);
        zmk_activity_set_idle_ms(CONFIG_VELVET_MIN_IDLE_TIMEOUT_MS);
    }
}

static K_WORK_DELAYABLE_DEFINE(velvet_idle_timeout_work, velvet_idle_timeout_work_cb);

static int velvet_idle_timeout_init(void) {
    k_work_schedule(&velvet_idle_timeout_work, K_SECONDS(2));
    return 0;
}

SYS_INIT(velvet_idle_timeout_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
#endif
