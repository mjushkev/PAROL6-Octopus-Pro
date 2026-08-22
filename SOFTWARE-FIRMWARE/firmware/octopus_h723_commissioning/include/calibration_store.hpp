#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace parol6::calibration {

inline constexpr std::uint32_t kSlotAAddress = 0x08040000U;
inline constexpr std::uint32_t kSlotBAddress = 0x08060000U;
inline constexpr std::uint32_t kStorageEnd = 0x08080000U;
inline constexpr std::uint32_t kSectorBytes = 0x00020000U;
inline constexpr std::size_t kFlashWordBytes = 32U;
inline constexpr std::uint32_t kCalibrationMagic = 0x43414C36U;  // "CAL6"
inline constexpr std::uint16_t kSchemaVersion = 1U;
inline constexpr std::int32_t kAbsoluteAngleCeilingMilliDegrees = 360000;

inline constexpr std::array<std::uint16_t, 6> kHardwarePulsesPerDegree = {
    114U, 356U, 161U, 36U, 36U, 89U};

enum JointFlags : std::uint8_t {
  kConfigured = 1U << 0U,
  kHomeRawPositive = 1U << 1U,
  kPositiveRawPositive = 1U << 2U,
  kSensorActiveHigh = 1U << 3U,
  kMinimumSet = 1U << 4U,
  kMaximumSet = 1U << 5U,
};

inline constexpr std::uint8_t kKnownJointFlags =
    kConfigured | kHomeRawPositive | kPositiveRawPositive |
    kSensorActiveHigh | kMinimumSet | kMaximumSet;

struct JointRecord final {
  std::int32_t minimum_millidegrees{0};
  std::int32_t maximum_millidegrees{0};
  std::uint16_t pulses_per_degree{0};
  std::uint8_t flags{0};
  std::uint8_t reserved{0};
};

struct alignas(kFlashWordBytes) CalibrationRecord final {
  std::uint32_t magic{kCalibrationMagic};
  std::uint16_t schema_version{kSchemaVersion};
  std::uint16_t payload_bytes{sizeof(JointRecord) * 6U};
  std::uint32_t sequence{0};
  std::array<JointRecord, 6> joints{};
  std::array<std::uint32_t, 10> reserved{};
  std::uint32_t crc32c{0};
};

enum class StoreStatus : std::uint8_t {
  factory_fallback = 0,
  flash_selected = 1,
  io_error = 2,
};

using WatchdogFeed = void (*)();

[[nodiscard]] CalibrationRecord make_factory_record() noexcept;
[[nodiscard]] std::uint32_t record_crc32c(
    const CalibrationRecord& record) noexcept;
[[nodiscard]] bool record_valid(const CalibrationRecord& record) noexcept;

class Store final {
 public:
  StoreStatus begin(WatchdogFeed watchdog_feed) noexcept;
  bool save(const CalibrationRecord& requested) noexcept;

  [[nodiscard]] const CalibrationRecord& record() const noexcept {
    return record_;
  }
  [[nodiscard]] StoreStatus status() const noexcept { return status_; }
  [[nodiscard]] bool persistent() const noexcept {
    return status_ == StoreStatus::flash_selected;
  }
  [[nodiscard]] std::uint8_t active_slot() const noexcept {
    return active_slot_;
  }

 private:
  bool program_flashword(std::uint32_t address, const void* data) noexcept;
  bool erase_sector(std::uint32_t address) noexcept;
  void feed_watchdog() const noexcept;

  WatchdogFeed watchdog_feed_{nullptr};
  StoreStatus status_{StoreStatus::factory_fallback};
  CalibrationRecord record_{};
  std::uint8_t active_slot_{0xFFU};
};

static_assert(sizeof(JointRecord) == 12U);
static_assert(sizeof(CalibrationRecord) == 128U);
static_assert(alignof(CalibrationRecord) == kFlashWordBytes);
static_assert(kSlotAAddress + kSectorBytes == kSlotBAddress);
static_assert(kSlotBAddress + kSectorBytes == kStorageEnd);

}  // namespace parol6::calibration
