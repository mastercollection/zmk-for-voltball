/* SPDX-License-Identifier: MIT */
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/pm/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ZMK's deep sleep skips wakeup-enabled devices when suspending. The sensor then
 * keeps its own sleep mode and level-low motion IRQ, which the nRF52 GPIO SENSE
 * retains in System OFF. Without this the driver powers the sensor down and
 * disconnects the pin, so only keys can wake the half.
 */
static int trackball_wake_init(void) {
    const struct device *sensor = DEVICE_DT_GET(DT_NODELABEL(trackball_central));
    if (!device_is_ready(sensor) || !pm_device_wakeup_enable(sensor, true)) {
        LOG_WRN("Trackball wake not enabled");
    }
    return 0;
}
SYS_INIT(trackball_wake_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
