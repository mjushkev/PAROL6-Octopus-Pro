#include "calibration_store.hpp"

#include <stm32h7xx_hal.h>

#include <cstring>

namespace parol6::calibration {
namespace {

inline constexpr std::uint32_t kCompatibilityMagic = 0x43464736U;  // "CFG6"
inline constexpr std::uint16_t kCompatibilitySchema = 1U;
inline constexpr std::uint32_t kCalibrationOffset = kFlashWordBytes;

struct alignas(kFlashWordBytes) CompatibilityConfigRecord final {
  std::uint32_t magic{kCompatibilityMagic};
  std::uint16_t schema_version{kCompatibilitySchema};
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

static_assert(sizeof(CompatibilityConfigRecord) == kFlashWordBytes);

std::uint32_t crc32c_bytes(const std::uint8_t* data,
                           const std::size_t bytes) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = 0; index < bytes; ++index) {
    crc ^= data[index];
    for (std::uint8_t bit = 0; bit < 8U; ++bit) {
      crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0x82F63B78U : 0U);
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

template <typename T>
T read_record(const std::uint32_t address) noexcept {
  T record{};
  std::memcpy(&record, reinterpret_cast<const void*>(address), sizeof(T));
  return record;
}

bool sequence_newer(const std::uint32_t candidate,
                    const std::uint32_t reference) noexcept {
  return static_cast<std::int32_t>(candidate - reference) > 0;
}

std::uint32_t increment_nonzero(const std::uint32_t value) noexcept {
  const auto next = value + 1U;
  return next == 0U ? 1U : next;
}

bool flashword_is_erased(const std::uint32_t address) noexcept {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(address);
  for (std::size_t index = 0; index < kFlashWordBytes; ++index) {
    if (bytes[index] != 0xFFU) return false;
  }
  return true;
}

std::uint32_t sector_index(const std::uint32_t address) noexcept {
  return (address - 0x08000000U) / kSectorBytes;
}

void invalidate_flash_cache(const std::uint32_t address,
                            const std::uint32_t bytes) noexcept {
#if (__DCACHE_PRESENT == 1U)
  SCB_InvalidateDCache_by_Addr(reinterpret_cast<void*>(address),
                              static_cast<std::int32_t>(bytes));
#else
  (void)address;
  (void)bytes;
#endif
}

CompatibilityConfigRecord make_compatibility_record(
    const std::uint32_t sequence) noexcept {
  CompatibilityConfigRecord record{};
  record.sequence = sequence;
  record.crc32c = crc32c_bytes(
      reinterpret_cast<const std::uint8_t*>(&record),
      offsetof(CompatibilityConfigRecord, crc32c));
  return record;
}

}  // namespace

CalibrationRecord make_factory_record() noexcept {
  CalibrationRecord record{};
  record.sequence = 0U;
  for (std::size_t axis = 0; axis < record.joints.size(); ++axis) {
    record.joints[axis].pulses_per_degree = kHardwarePulsesPerDegree[axis];
  }
  record.crc32c = record_crc32c(record);
  return record;
}

std::uint32_t record_crc32c(const CalibrationRecord& record) noexcept {
  return crc32c_bytes(reinterpret_cast<const std::uint8_t*>(&record),
                      offsetof(CalibrationRecord, crc32c));
}

bool record_valid(const CalibrationRecord& record) noexcept {
  if (record.magic != kCalibrationMagic ||
      record.schema_version != kSchemaVersion ||
      record.payload_bytes != sizeof(JointRecord) * record.joints.size() ||
      record.sequence == 0U || record.crc32c != record_crc32c(record)) {
    return false;
  }
  for (const auto value : record.reserved) {
    if (value != 0U) return false;
  }
  for (std::size_t axis = 0; axis < record.joints.size(); ++axis) {
    const auto& joint = record.joints[axis];
    if (joint.pulses_per_degree != kHardwarePulsesPerDegree[axis] ||
        joint.reserved != 0U || (joint.flags & ~kKnownJointFlags) != 0U) {
      return false;
    }
    const bool configured = (joint.flags & kConfigured) != 0U;
    const bool minimum_set = (joint.flags & kMinimumSet) != 0U;
    const bool maximum_set = (joint.flags & kMaximumSet) != 0U;
    if ((minimum_set || maximum_set) && !configured) return false;
    if (minimum_set &&
        (joint.minimum_millidegrees < -kAbsoluteAngleCeilingMilliDegrees ||
         joint.minimum_millidegrees > 0)) {
      return false;
    }
    if (maximum_set &&
        (joint.maximum_millidegrees < 0 ||
         joint.maximum_millidegrees > kAbsoluteAngleCeilingMilliDegrees)) {
      return false;
    }
    if (minimum_set && maximum_set &&
        joint.minimum_millidegrees >= joint.maximum_millidegrees) {
      return false;
    }
  }
  return true;
}

StoreStatus Store::begin(const WatchdogFeed watchdog_feed) noexcept {
  watchdog_feed_ = watchdog_feed;
  const auto slot_a =
      read_record<CalibrationRecord>(kSlotAAddress + kCalibrationOffset);
  const auto slot_b =
      read_record<CalibrationRecord>(kSlotBAddress + kCalibrationOffset);
  const bool a_valid = record_valid(slot_a);
  const bool b_valid = record_valid(slot_b);
  if (!a_valid && !b_valid) {
    record_ = make_factory_record();
    active_slot_ = 0xFFU;
    status_ = StoreStatus::factory_fallback;
    return status_;
  }
  if (a_valid && (!b_valid || !sequence_newer(slot_b.sequence, slot_a.sequence))) {
    record_ = slot_a;
    active_slot_ = 0U;
  } else {
    record_ = slot_b;
    active_slot_ = 1U;
  }
  status_ = StoreStatus::flash_selected;
  return status_;
}

bool Store::save(const CalibrationRecord& requested) noexcept {
  CalibrationRecord next = requested;
  next.magic = kCalibrationMagic;
  next.schema_version = kSchemaVersion;
  next.payload_bytes = sizeof(JointRecord) * next.joints.size();
  next.sequence = increment_nonzero(record_.sequence);
  next.reserved.fill(0U);
  for (std::size_t axis = 0; axis < next.joints.size(); ++axis) {
    next.joints[axis].pulses_per_degree = kHardwarePulsesPerDegree[axis];
    next.joints[axis].reserved = 0U;
  }
  next.crc32c = record_crc32c(next);
  if (!record_valid(next)) return false;

  const std::uint8_t target_slot = active_slot_ == 0U ? 1U : 0U;
  const std::uint32_t target = target_slot == 0U ? kSlotAAddress : kSlotBAddress;
  const auto compatibility = make_compatibility_record(next.sequence);
  if (!erase_sector(target) || !program_flashword(target, &compatibility)) {
    status_ = StoreStatus::io_error;
    return false;
  }
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&next);
  for (std::size_t offset = 0; offset < sizeof(next); offset += kFlashWordBytes) {
    if (!program_flashword(target + kCalibrationOffset + offset,
                           bytes + offset)) {
      status_ = StoreStatus::io_error;
      return false;
    }
  }
  const auto verified =
      read_record<CalibrationRecord>(target + kCalibrationOffset);
  if (std::memcmp(&verified, &next, sizeof(next)) != 0 ||
      !record_valid(verified)) {
    status_ = StoreStatus::io_error;
    return false;
  }
  record_ = verified;
  active_slot_ = target_slot;
  status_ = StoreStatus::flash_selected;
  return true;
}

bool Store::program_flashword(const std::uint32_t address,
                              const void* data) noexcept {
  if (address < kSlotAAddress || address >= kStorageEnd ||
      (address % kFlashWordBytes) != 0U) {
    return false;
  }
  feed_watchdog();
  if (HAL_FLASH_Unlock() != HAL_OK) return false;
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK1);
  const auto result = HAL_FLASH_Program(
      FLASH_TYPEPROGRAM_FLASHWORD, address,
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(data)));
  HAL_FLASH_Lock();
  feed_watchdog();
  __DSB();
  __ISB();
  invalidate_flash_cache(address, kFlashWordBytes);
  return result == HAL_OK &&
         std::memcmp(reinterpret_cast<const void*>(address), data,
                     kFlashWordBytes) == 0;
}

bool Store::erase_sector(const std::uint32_t address) noexcept {
  if (address != kSlotAAddress && address != kSlotBAddress) return false;
  feed_watchdog();
  if (HAL_FLASH_Unlock() != HAL_OK) return false;
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK1);
  FLASH_EraseInitTypeDef erase{};
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_1;
  erase.Sector = sector_index(address);
  erase.NbSectors = 1U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  std::uint32_t sector_error = 0U;
  const auto result = HAL_FLASHEx_Erase(&erase, &sector_error);
  HAL_FLASH_Lock();
  feed_watchdog();
  __DSB();
  __ISB();
  invalidate_flash_cache(address, kSectorBytes);
  return result == HAL_OK && sector_error == 0xFFFFFFFFU &&
         flashword_is_erased(address);
}

void Store::feed_watchdog() const noexcept {
  if (watchdog_feed_ != nullptr) watchdog_feed_();
}

}  // namespace parol6::calibration
