#ifndef YMIR_PROTOCOL_HPP
#define YMIR_PROTOCOL_HPP

#include "ymir/transport.hpp"

namespace ymir {

/**
 * Runs the firmware update protocol over the given transport.
 * Returns the slot number to boot (0 or 1) when CMD_BOOT is received,
 * or -1 if the session is aborted.
 */
int protocol_run(transport_t& t);

} // namespace ymir

#endif // YMIR_PROTOCOL_HPP
