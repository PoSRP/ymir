#ifndef YMIR_API_H
#define YMIR_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Returns 1 if running from slot A, 2 if slot B, 0 if bootloader.
 */
int ymir_current_slot(void);

/**
 * Mark the current boot as successful. Bootloader will not roll back on next boot.
 */
void ymir_confirm_boot(void);

/**
 * Trigger an immediate system reset; bootloader will boot the other slot.
 */
void ymir_request_rollback(void);

/**
 * Request USB firmware update mode. Sets a RAM flag and resets; bootloader
 * enters transfer mode on the next boot instead of jumping to the application.
 */
void ymir_enter_update(void);

/**
 * Returns 1 if 'header' (exactly 8 bytes) is a valid DFU enter-update request.
 * Format: [0xAD magic][0xF0 type][0x00 len_hi][0x00 len_lo][CRC-32/MPEG-2
 * big-endian, 4 bytes] CRC covers bytes 0-3.
 */
int ymir_is_enter_update_request(const uint8_t* header);

#ifdef __cplusplus
}

#include <cstdint>
#include <span>

namespace ymir {

inline int  current_slot() { return ymir_current_slot(); }
inline void confirm_boot() { ymir_confirm_boot(); }
inline void request_rollback() { ymir_request_rollback(); }
inline void enter_update() { ymir_enter_update(); }

inline bool is_enter_update_request(std::span<const uint8_t, 8> header)
{
    return ymir_is_enter_update_request(header.data()) != 0;
}

} // namespace ymir

#endif // __cplusplus

#endif // YMIR_API_H
