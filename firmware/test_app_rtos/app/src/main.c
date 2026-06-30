#include "main.h"
#include "cmsis_os.h"
#include "iwdg.h"

#include "ymir/api.h"

#include <string.h>
#include <stdint.h>

#define RX_BUF_SIZE 512U

static volatile uint8_t  rx_buf[RX_BUF_SIZE];
static volatile uint32_t rx_head = 0;
static volatile uint32_t rx_tail = 0;

// Called from USB CDC receive callback (interrupt context)
void app_usb_rx(const uint8_t* buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint32_t next = (rx_head + 1U) % RX_BUF_SIZE;
        if (next != rx_tail) {
            rx_buf[rx_head] = buf[i];
            __DMB();
            rx_head = next;
        }
    }
}

static int rx_read(uint8_t* out)
{
    if (rx_tail == rx_head) {
        return 0;
    }
    *out = rx_buf[rx_tail];
    __DMB();
    rx_tail = (rx_tail + 1U) % RX_BUF_SIZE;
    return 1;
}

// Called from StartDefaultTask after USB is initialized
void app_run(void)
{
    ymir_confirm_boot();

    // Slot 1 blinks at 250 ms, slot 2 at 500 ms
    uint32_t blink_ms = (uint32_t)ymir_current_slot() * 250U;
    if (blink_ms == 0U) {
        blink_ms = 500U;
    }

    uint8_t  dfu_buf[8];
    uint32_t dfu_pos = 0;

    for (;;) {
        HAL_GPIO_TogglePin(USER_LED_GPIO_Port, USER_LED_Pin);
        HAL_IWDG_Refresh(&hiwdg);
        osDelay(blink_ms);

        uint8_t b;
        while (rx_read(&b)) {
            dfu_buf[dfu_pos++] = b;
            if (dfu_pos == 8U) {
                if (ymir_is_enter_update_request(dfu_buf)) {
                    ymir_enter_update();
                }
                memmove(dfu_buf, dfu_buf + 1, 7);
                dfu_pos = 7U;
            }
        }
    }
}
