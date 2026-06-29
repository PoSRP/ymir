#include "ymir/metadata.hpp"
#include "ymir/crc32.hpp"
#include "ymir/flash.hpp"
#include "ymir/flash_hw.hpp"

#include <cstddef>
#include <cstring>
#include <optional>

namespace ymir::metadata {

static constexpr uint32_t META_SECTOR = 3;
static constexpr size_t   META_SIZE   = 16u * 1024u;
static constexpr size_t   NUM_ENTRIES = META_SIZE / sizeof(metadata_t);
static constexpr uint32_t META_MAGIC  = 0xBAADF00Du;

static const metadata_t* meta_entries() { return reinterpret_cast<const metadata_t*>(meta_base()); }

static uint32_t entry_crc(const metadata_t& m)
{
    return crc32_mpeg2({reinterpret_cast<const uint8_t*>(&m), offsetof(metadata_t, crc32)});
}

static bool entry_valid(const metadata_t& m)
{
    return m.magic == META_MAGIC && m.crc32 == entry_crc(m);
}

std::optional<metadata_t> read()
{
    const metadata_t* entries = meta_entries();
    const metadata_t* best    = nullptr;
    for (size_t i = 0; i < NUM_ENTRIES; i++) {
        if (entry_valid(entries[i])) {
            if (!best || entries[i].generation > best->generation) {
                best = &entries[i];
            }
        }
    }
    if (!best) {
        return std::nullopt;
    }
    return *best;
}

void write(const metadata_t& in)
{
    const metadata_t* entries = meta_entries();

    size_t next = NUM_ENTRIES;
    for (size_t i = 0; i < NUM_ENTRIES; i++) {
        const auto* raw    = reinterpret_cast<const uint8_t*>(&entries[i]);
        bool        erased = true;
        for (size_t b = 0; b < sizeof(metadata_t); b++) {
            if (raw[b] != 0xFF) {
                erased = false;
                break;
            }
        }
        if (erased) {
            next = i;
            break;
        }
    }

    if (next == NUM_ENTRIES) {
        flash::erase_sector(META_SECTOR);
        next = 0;
    }

    metadata_t m = in;
    m.crc32      = entry_crc(m);
    flash::write(meta_base() + next * sizeof(metadata_t),
                 {reinterpret_cast<const std::byte*>(&m), sizeof(metadata_t)});
}

} // namespace ymir::metadata
