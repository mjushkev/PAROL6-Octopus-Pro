#pragma once

#include <cstddef>
#include <cstdint>

#include "safe_core.hpp"

namespace parol6::storage {

inline constexpr std::uint32_t kApplicationOrigin = 0x08020000U;
inline constexpr std::uint32_t kApplicationLimit = 0x08040000U;
inline constexpr std::uint32_t kSlotAAddress = 0x08040000U;
inline constexpr std::uint32_t kSlotBAddress = 0x08060000U;
inline constexpr std::uint32_t kStorageEnd = 0x08080000U;
inline constexpr std::uint32_t kSectorBytes = 0x00020000U;
inline constexpr std::size_t kFlashWordBytes = 32U;
inline constexpr std::uint32_t kConfigMagic = 0x43464736U;  // "CFG6"
inline constexpr std::uint32_t kEventMagic = 0x45565436U;   // "EVT6"

struct alignas(kFlashWordBytes) ConfigRecord final {
  std::uint32_t magic{kConfigMagic};
  std::uint16_t schema_version{safe_core::kConfigSchemaVersion};
  std::uint16_t payload_bytes{12U};
  std::uint32_t sequence{0};
  std::uint8_t commissioned{0};
  std::uint8_t hardware_outputs_enabled{0};
  std::uint16_t flags{0};
  std::uint16_t protocol_version{0x0100U};
  std::uint16_t reserved0{0};
  std::uint32_t reserved1{0};
  std::uint32_t reserved2{0};
  std::uint32_t crc32c{0};
};

struct alignas(kFlashWordBytes) EventRecord final {
  std::uint32_t magic{kEventMagic};
  std::uint32_t sequence{0};
  std::uint32_t timestamp_ms{0};
  std::uint16_t code{0};
  std::uint16_t detail{0};
  std::uint32_t argument0{0};
  std::uint32_t argument1{0};
  std::uint32_t reserved{0};
  std::uint32_t crc32c{0};
};

enum class StoreStatus : std::uint8_t {
  factory_fallback = 0,
  flash_selected = 1,
  io_error = 2,
};

struct Selection final {
  bool valid{false};
  std::uint8_t slot{0xFFU};
  std::uint32_t sequence{0};
};

[[nodiscard]] constexpr std::uint32_t config_crc32c(
    const ConfigRecord& record) noexcept;
[[nodiscard]] constexpr std::uint32_t event_crc32c(
    const EventRecord& record) noexcept;
[[nodiscard]] constexpr bool config_valid(const ConfigRecord& record) noexcept;
[[nodiscard]] constexpr bool event_valid(const EventRecord& record) noexcept;
[[nodiscard]] constexpr ConfigRecord make_safe_config(
    std::uint32_t sequence) noexcept;
[[nodiscard]] constexpr EventRecord make_event(
    std::uint32_t sequence, std::uint32_t timestamp_ms, std::uint16_t code,
    std::uint16_t detail, std::uint32_t argument0,
    std::uint32_t argument1) noexcept;
[[nodiscard]] constexpr Selection select_config(const ConfigRecord& slot_a,
                                                const ConfigRecord& slot_b) noexcept;

using WatchdogFeed = void (*)();

class PersistentStore final {
 public:
  StoreStatus begin(WatchdogFeed watchdog_feed) noexcept;
  bool append_event(std::uint32_t timestamp_ms, std::uint16_t code,
                    std::uint16_t detail = 0, std::uint32_t argument0 = 0,
                    std::uint32_t argument1 = 0) noexcept;
  bool write_safe_config(const ConfigRecord& requested,
                         std::uint32_t timestamp_ms) noexcept;

  [[nodiscard]] StoreStatus status() const noexcept { return status_; }
  [[nodiscard]] Selection selection() const noexcept { return selection_; }
  [[nodiscard]] std::uint16_t event_count() const noexcept {
    return event_count_;
  }
  [[nodiscard]] std::uint32_t latest_event_sequence() const noexcept {
    return latest_event_sequence_;
  }

 private:
  bool initialize_factory_slot() noexcept;
  bool scan_active_events() noexcept;
  bool rotate_to_inactive() noexcept;
  bool program_flashword(std::uint32_t address, const void* record) noexcept;
  bool erase_sector(std::uint32_t address) noexcept;
  void feed_watchdog() const noexcept;

  WatchdogFeed watchdog_feed_{nullptr};
  StoreStatus status_{StoreStatus::factory_fallback};
  Selection selection_{};
  ConfigRecord active_config_{};
  std::uint32_t active_address_{0};
  std::uint32_t next_event_address_{0};
  std::uint16_t event_count_{0};
  std::uint32_t latest_event_sequence_{0};
};

constexpr std::uint32_t config_crc32c(const ConfigRecord& record) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  crc = safe_core::crc32c_le(crc, record.magic);
  crc = safe_core::crc32c_le(crc, record.schema_version);
  crc = safe_core::crc32c_le(crc, record.payload_bytes);
  crc = safe_core::crc32c_le(crc, record.sequence);
  crc = safe_core::crc32c_byte(crc, record.commissioned);
  crc = safe_core::crc32c_byte(crc, record.hardware_outputs_enabled);
  crc = safe_core::crc32c_le(crc, record.flags);
  crc = safe_core::crc32c_le(crc, record.protocol_version);
  crc = safe_core::crc32c_le(crc, record.reserved0);
  crc = safe_core::crc32c_le(crc, record.reserved1);
  crc = safe_core::crc32c_le(crc, record.reserved2);
  return crc ^ 0xFFFFFFFFU;
}

constexpr std::uint32_t event_crc32c(const EventRecord& record) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  crc = safe_core::crc32c_le(crc, record.magic);
  crc = safe_core::crc32c_le(crc, record.sequence);
  crc = safe_core::crc32c_le(crc, record.timestamp_ms);
  crc = safe_core::crc32c_le(crc, record.code);
  crc = safe_core::crc32c_le(crc, record.detail);
  crc = safe_core::crc32c_le(crc, record.argument0);
  crc = safe_core::crc32c_le(crc, record.argument1);
  crc = safe_core::crc32c_le(crc, record.reserved);
  return crc ^ 0xFFFFFFFFU;
}

constexpr bool config_valid(const ConfigRecord& record) noexcept {
  return record.magic == kConfigMagic &&
         record.schema_version == safe_core::kConfigSchemaVersion &&
         record.payload_bytes == 12U && record.sequence != 0U &&
         record.commissioned == 0U && record.hardware_outputs_enabled == 0U &&
         record.flags == 0U && record.protocol_version == 0x0100U &&
         record.reserved0 == 0U && record.reserved1 == 0U &&
         record.reserved2 == 0U && record.crc32c == config_crc32c(record);
}

constexpr bool event_valid(const EventRecord& record) noexcept {
  return record.magic == kEventMagic && record.sequence != 0U &&
         record.reserved == 0U && record.crc32c == event_crc32c(record);
}

constexpr ConfigRecord make_safe_config(const std::uint32_t sequence) noexcept {
  ConfigRecord record{};
  record.sequence = sequence;
  record.crc32c = config_crc32c(record);
  return record;
}

constexpr EventRecord make_event(
    const std::uint32_t sequence, const std::uint32_t timestamp_ms,
    const std::uint16_t code, const std::uint16_t detail,
    const std::uint32_t argument0, const std::uint32_t argument1) noexcept {
  EventRecord record{};
  record.sequence = sequence;
  record.timestamp_ms = timestamp_ms;
  record.code = code;
  record.detail = detail;
  record.argument0 = argument0;
  record.argument1 = argument1;
  record.crc32c = event_crc32c(record);
  return record;
}

constexpr Selection select_config(const ConfigRecord& slot_a,
                                  const ConfigRecord& slot_b) noexcept {
  const bool a_valid = config_valid(slot_a);
  const bool b_valid = config_valid(slot_b);
  if (!a_valid && !b_valid) {
    return {};
  }
  if (a_valid &&
      (!b_valid ||
       !safe_core::sequence_newer(slot_b.sequence, slot_a.sequence))) {
    return {true, 0U, slot_a.sequence};
  }
  return {true, 1U, slot_b.sequence};
}

static_assert(sizeof(ConfigRecord) == kFlashWordBytes);
static_assert(sizeof(EventRecord) == kFlashWordBytes);
static_assert(alignof(ConfigRecord) == kFlashWordBytes);
static_assert(alignof(EventRecord) == kFlashWordBytes);
static_assert(kSlotAAddress + kSectorBytes == kSlotBAddress);
static_assert(kSlotBAddress + kSectorBytes == kStorageEnd);

}  // namespace parol6::storage
