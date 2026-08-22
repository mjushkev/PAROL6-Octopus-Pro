#include "persistent_store.hpp"

#include <stm32h7xx_hal.h>

#include <cstring>

namespace parol6::storage {
namespace {

template <typename T>
T read_record(const std::uint32_t address) noexcept {
  T record{};
  std::memcpy(&record, reinterpret_cast<const void*>(address), sizeof(T));
  return record;
}

bool flashword_is_erased(const std::uint32_t address) noexcept {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(address);
  for (std::size_t index = 0; index < kFlashWordBytes; ++index) {
    if (bytes[index] != 0xFFU) {
      return false;
    }
  }
  return true;
}

std::uint32_t sector_index(const std::uint32_t address) noexcept {
  return (address - 0x08000000U) / kSectorBytes;
}

constexpr std::uint32_t increment_nonzero(const std::uint32_t value) noexcept {
  const auto next = value + 1U;
  return next == 0U ? 1U : next;
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

constexpr bool storage_algorithm_tests() {
  const auto first = make_safe_config(1U);
  const auto second = make_safe_config(2U);
  const auto selected = select_config(first, second);
  if (!selected.valid || selected.slot != 1U || selected.sequence != 2U) {
    return false;
  }
  auto corrupt = second;
  corrupt.crc32c ^= 0x1U;
  const auto fallback = select_config(first, corrupt);
  if (!fallback.valid || fallback.slot != 0U) {
    return false;
  }
  auto unsafe = first;
  unsafe.hardware_outputs_enabled = 1U;
  unsafe.crc32c = config_crc32c(unsafe);
  if (config_valid(unsafe)) {
    return false;
  }
  auto event = make_event(1U, 123U, 4U, 5U, 6U, 7U);
  if (!event_valid(event)) {
    return false;
  }
  event.argument1 ^= 1U;
  return !event_valid(event) && increment_nonzero(7U) == 8U &&
         increment_nonzero(0xFFFFFFFFU) == 1U;
}

static_assert(storage_algorithm_tests());

}  // namespace

StoreStatus PersistentStore::begin(const WatchdogFeed watchdog_feed) noexcept {
  watchdog_feed_ = watchdog_feed;
  const auto slot_a = read_record<ConfigRecord>(kSlotAAddress);
  const auto slot_b = read_record<ConfigRecord>(kSlotBAddress);
  selection_ = select_config(slot_a, slot_b);
  if (!selection_.valid) {
    if (!initialize_factory_slot()) {
      status_ = StoreStatus::factory_fallback;
      active_config_ = make_safe_config(1U);
      return status_;
    }
  } else {
    active_address_ = selection_.slot == 0U ? kSlotAAddress : kSlotBAddress;
    active_config_ = selection_.slot == 0U ? slot_a : slot_b;
    status_ = StoreStatus::flash_selected;
  }

  if (!scan_active_events()) {
    status_ = StoreStatus::io_error;
  }
  return status_;
}

bool PersistentStore::initialize_factory_slot() noexcept {
  std::uint32_t target = 0;
  if (flashword_is_erased(kSlotAAddress)) {
    target = kSlotAAddress;
  } else if (flashword_is_erased(kSlotBAddress)) {
    target = kSlotBAddress;
  } else {
    return false;
  }
  const auto record = make_safe_config(1U);
  if (!program_flashword(target, &record)) {
    return false;
  }
  active_address_ = target;
  active_config_ = record;
  selection_ = {true, static_cast<std::uint8_t>(
                          target == kSlotAAddress ? 0U : 1U),
                record.sequence};
  status_ = StoreStatus::flash_selected;
  return true;
}

bool PersistentStore::scan_active_events() noexcept {
  event_count_ = 0;
  latest_event_sequence_ = 0;
  next_event_address_ = active_address_ + kFlashWordBytes;
  const auto sector_end = active_address_ + kSectorBytes;
  while (next_event_address_ < sector_end) {
    if (flashword_is_erased(next_event_address_)) {
      return true;
    }
    const auto event = read_record<EventRecord>(next_event_address_);
    if (!event_valid(event)) {
      next_event_address_ = sector_end;
      return true;
    }
    latest_event_sequence_ = event.sequence;
    if (event_count_ != 0xFFFFU) {
      ++event_count_;
    }
    next_event_address_ += kFlashWordBytes;
  }
  return true;
}

bool PersistentStore::append_event(
    const std::uint32_t timestamp_ms, const std::uint16_t code,
    const std::uint16_t detail, const std::uint32_t argument0,
    const std::uint32_t argument1) noexcept {
  if (status_ != StoreStatus::flash_selected) {
    return false;
  }
  if (next_event_address_ >= active_address_ + kSectorBytes &&
      !rotate_to_inactive()) {
    status_ = StoreStatus::io_error;
    return false;
  }
  const auto event = make_event(increment_nonzero(latest_event_sequence_),
                                timestamp_ms, code, detail, argument0,
                                argument1);
  if (!program_flashword(next_event_address_, &event)) {
    status_ = StoreStatus::io_error;
    return false;
  }
  latest_event_sequence_ = event.sequence;
  next_event_address_ += kFlashWordBytes;
  if (event_count_ != 0xFFFFU) {
    ++event_count_;
  }
  return true;
}

bool PersistentStore::write_safe_config(const ConfigRecord& requested,
                                        const std::uint32_t timestamp_ms) noexcept {
  if (status_ != StoreStatus::flash_selected) {
    return false;
  }
  ConfigRecord next = requested;
  next.sequence = increment_nonzero(active_config_.sequence);
  next.crc32c = config_crc32c(next);
  if (!config_valid(next)) {
    return false;
  }
  const auto target =
      active_address_ == kSlotAAddress ? kSlotBAddress : kSlotAAddress;
  if (!erase_sector(target) || !program_flashword(target, &next)) {
    status_ = StoreStatus::io_error;
    return false;
  }
  active_address_ = target;
  active_config_ = next;
  selection_ = {true, static_cast<std::uint8_t>(
                          target == kSlotAAddress ? 0U : 1U),
                next.sequence};
  next_event_address_ = target + kFlashWordBytes;
  event_count_ = 0;
  return append_event(timestamp_ms, 6U, 0U, next.sequence, 0U);
}

bool PersistentStore::rotate_to_inactive() noexcept {
  const auto target =
      active_address_ == kSlotAAddress ? kSlotBAddress : kSlotAAddress;
  if (!erase_sector(target)) {
    return false;
  }
  const auto next_config =
      make_safe_config(increment_nonzero(active_config_.sequence));
  if (!program_flashword(target, &next_config)) {
    return false;
  }
  active_address_ = target;
  active_config_ = next_config;
  selection_ = {true, static_cast<std::uint8_t>(
                          target == kSlotAAddress ? 0U : 1U),
                next_config.sequence};
  next_event_address_ = target + kFlashWordBytes;
  event_count_ = 0;
  return true;
}

bool PersistentStore::program_flashword(const std::uint32_t address,
                                        const void* record) noexcept {
  if (address < kSlotAAddress || address >= kStorageEnd ||
      (address % kFlashWordBytes) != 0U) {
    return false;
  }
  feed_watchdog();
  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK1);
  const auto result = HAL_FLASH_Program(
      FLASH_TYPEPROGRAM_FLASHWORD, address,
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(record)));
  HAL_FLASH_Lock();
  feed_watchdog();
  __DSB();
  __ISB();
  if (result != HAL_OK) {
    return false;
  }
  invalidate_flash_cache(address, kFlashWordBytes);
  return std::memcmp(reinterpret_cast<const void*>(address), record,
                     kFlashWordBytes) == 0;
}

bool PersistentStore::erase_sector(const std::uint32_t address) noexcept {
  if (address != kSlotAAddress && address != kSlotBAddress) {
    return false;
  }
  feed_watchdog();
  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK1);
  FLASH_EraseInitTypeDef erase{};
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_1;
  erase.Sector = sector_index(address);
  erase.NbSectors = 1U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  std::uint32_t sector_error = 0;
  const auto result = HAL_FLASHEx_Erase(&erase, &sector_error);
  HAL_FLASH_Lock();
  feed_watchdog();
  __DSB();
  __ISB();
  invalidate_flash_cache(address, kSectorBytes);
  return result == HAL_OK && sector_error == 0xFFFFFFFFU &&
         flashword_is_erased(address);
}

void PersistentStore::feed_watchdog() const noexcept {
  if (watchdog_feed_ != nullptr) {
    watchdog_feed_();
  }
}

}  // namespace parol6::storage
