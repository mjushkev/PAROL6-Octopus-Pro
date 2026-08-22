#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace parol6::safe_core {

inline constexpr std::uint32_t kConfigMagic = 0x364C5250U;  // "PRL6"
inline constexpr std::uint16_t kConfigSchemaVersion = 1;
inline constexpr std::uint32_t kControlHeartbeatTimeoutMs = 300;
inline constexpr std::size_t kEventCapacity = 32;

enum class ControllerState : std::uint8_t {
  boot_self_test,
  not_commissioned,
  disarmed,
  protective_stop,
  fault_latched,
};

enum class FaultCode : std::uint8_t {
  none,
  config_invalid,
  link_stale,
  protocol_violation,
};

enum class EventCode : std::uint8_t {
  boot,
  config_selected,
  config_rejected,
  heartbeat_timeout,
  command_rejected,
};

struct OutputInterlocks final {
  bool contactor_request{false};
  bool step_outputs_enabled{false};
  bool driver_enables_active{false};
  bool gripper_pwm_enabled{false};

  constexpr void force_safe() noexcept {
    contactor_request = false;
    step_outputs_enabled = false;
    driver_enables_active = false;
    gripper_pwm_enabled = false;
  }

  [[nodiscard]] constexpr bool all_safe() const noexcept {
    return !contactor_request && !step_outputs_enabled &&
           !driver_enables_active && !gripper_pwm_enabled;
  }
};

struct ConfigRecord final {
  std::uint32_t magic{kConfigMagic};
  std::uint16_t schema_version{kConfigSchemaVersion};
  std::uint16_t payload_bytes{8};
  std::uint32_t sequence{0};
  std::uint8_t commissioned{0};
  std::uint8_t hardware_outputs_enabled{0};
  std::array<std::uint8_t, 2> reserved{};
  std::uint32_t crc32c{0};
};

struct ConfigSelection final {
  bool valid{false};
  std::uint8_t slot{0xFFU};
  std::uint32_t sequence{0};
};

[[nodiscard]] constexpr std::uint32_t config_crc32c(
    const ConfigRecord& record) noexcept;
[[nodiscard]] constexpr bool config_valid(const ConfigRecord& record) noexcept;
[[nodiscard]] constexpr ConfigSelection select_config(
    const ConfigRecord& slot_a, const ConfigRecord& slot_b) noexcept;
[[nodiscard]] constexpr ConfigRecord make_safe_config(
    std::uint32_t sequence) noexcept;

struct Event final {
  std::uint32_t timestamp_ms{0};
  EventCode code{EventCode::boot};
  std::uint16_t detail{0};
};

class EventLog final {
 public:
  constexpr void push(const Event event) noexcept {
    events_[next_] = event;
    next_ = (next_ + 1U) % kEventCapacity;
    if (size_ < kEventCapacity) {
      ++size_;
    }
    ++total_;
  }

  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr std::uint32_t total() const noexcept { return total_; }

  [[nodiscard]] constexpr Event oldest(std::size_t index) const noexcept {
    if (index >= size_) {
      return {};
    }
    const std::size_t first =
        size_ == kEventCapacity ? next_ : (next_ + kEventCapacity - size_) % kEventCapacity;
    return events_[(first + index) % kEventCapacity];
  }

 private:
  std::array<Event, kEventCapacity> events_{};
  std::size_t next_{0};
  std::size_t size_{0};
  std::uint32_t total_{0};
};

class SafeCore final {
 public:
  constexpr void boot(const ConfigRecord& slot_a, const ConfigRecord& slot_b,
                      std::uint32_t now_ms) noexcept {
    outputs_.force_safe();
    state_ = ControllerState::boot_self_test;
    fault_ = FaultCode::none;
    control_session_active_ = false;
    accepted_commands_ = 0;
    rejected_commands_ = 0;
    log_ = EventLog{};
    log_.push({now_ms, EventCode::boot, 0});
    config_ = select_config(slot_a, slot_b);
    if (!config_.valid) {
      fault_ = FaultCode::config_invalid;
      log_.push({now_ms, EventCode::config_rejected, 0});
    } else {
      log_.push({now_ms, EventCode::config_selected, config_.slot});
    }
    // This firmware is deliberately compiled without any output capability.
    // A valid calibration therefore still remains NOT_COMMISSIONED.
    state_ = ControllerState::not_commissioned;
  }

  constexpr void accept_safe_command() noexcept { ++accepted_commands_; }

  constexpr void reject_command(std::uint32_t now_ms) noexcept {
    ++rejected_commands_;
    fault_ = FaultCode::protocol_violation;
    log_.push({now_ms, EventCode::command_rejected, 0});
    outputs_.force_safe();
  }

  constexpr void begin_control_session(std::uint32_t now_ms) noexcept {
    control_session_active_ = true;
    last_heartbeat_ms_ = now_ms;
  }

  constexpr void heartbeat(std::uint32_t now_ms) noexcept {
    last_heartbeat_ms_ = now_ms;
  }

  constexpr bool poll(std::uint32_t now_ms) noexcept {
    if (!control_session_active_) {
      return false;
    }
    if (static_cast<std::uint32_t>(now_ms - last_heartbeat_ms_) <=
        kControlHeartbeatTimeoutMs) {
      return false;
    }
    control_session_active_ = false;
    fault_ = FaultCode::link_stale;
    state_ = ControllerState::protective_stop;
    outputs_.force_safe();
    log_.push({now_ms, EventCode::heartbeat_timeout, 0});
    return true;
  }

  [[nodiscard]] constexpr const OutputInterlocks& outputs() const noexcept {
    return outputs_;
  }
  [[nodiscard]] constexpr ControllerState state() const noexcept { return state_; }
  [[nodiscard]] constexpr FaultCode fault() const noexcept { return fault_; }
  [[nodiscard]] constexpr ConfigSelection config() const noexcept { return config_; }
  [[nodiscard]] constexpr const EventLog& events() const noexcept { return log_; }
  [[nodiscard]] constexpr std::uint32_t accepted_commands() const noexcept {
    return accepted_commands_;
  }
  [[nodiscard]] constexpr std::uint32_t rejected_commands() const noexcept {
    return rejected_commands_;
  }

 private:
  OutputInterlocks outputs_{};
  ControllerState state_{ControllerState::boot_self_test};
  FaultCode fault_{FaultCode::none};
  ConfigSelection config_{};
  EventLog log_{};
  bool control_session_active_{false};
  std::uint32_t last_heartbeat_ms_{0};
  std::uint32_t accepted_commands_{0};
  std::uint32_t rejected_commands_{0};
};

constexpr std::uint32_t crc32c_byte(std::uint32_t crc,
                                    std::uint8_t value) noexcept {
  crc ^= value;
  for (std::uint8_t bit = 0; bit < 8; ++bit) {
    crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0x82F63B78U : 0U);
  }
  return crc;
}

template <typename T>
constexpr std::uint32_t crc32c_le(std::uint32_t crc, T value) noexcept {
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    crc = crc32c_byte(crc,
                      static_cast<std::uint8_t>(value >> (index * 8U)));
  }
  return crc;
}

constexpr std::uint32_t config_crc32c(const ConfigRecord& record) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  crc = crc32c_le(crc, record.magic);
  crc = crc32c_le(crc, record.schema_version);
  crc = crc32c_le(crc, record.payload_bytes);
  crc = crc32c_le(crc, record.sequence);
  crc = crc32c_byte(crc, record.commissioned);
  crc = crc32c_byte(crc, record.hardware_outputs_enabled);
  crc = crc32c_byte(crc, record.reserved[0]);
  crc = crc32c_byte(crc, record.reserved[1]);
  return crc ^ 0xFFFFFFFFU;
}

constexpr bool config_valid(const ConfigRecord& record) noexcept {
  return record.magic == kConfigMagic &&
         record.schema_version == kConfigSchemaVersion &&
         record.payload_bytes == 8U && record.sequence != 0U &&
         record.commissioned == 0U && record.hardware_outputs_enabled == 0U &&
         record.reserved[0] == 0U && record.reserved[1] == 0U &&
         record.crc32c == config_crc32c(record);
}

constexpr bool sequence_newer(std::uint32_t candidate,
                              std::uint32_t reference) noexcept {
  return candidate != reference &&
         static_cast<std::uint32_t>(candidate - reference) < 0x80000000U;
}

constexpr ConfigSelection select_config(const ConfigRecord& slot_a,
                                        const ConfigRecord& slot_b) noexcept {
  const bool a_valid = config_valid(slot_a);
  const bool b_valid = config_valid(slot_b);
  if (!a_valid && !b_valid) {
    return {};
  }
  if (a_valid && (!b_valid || !sequence_newer(slot_b.sequence, slot_a.sequence))) {
    return {true, 0U, slot_a.sequence};
  }
  return {true, 1U, slot_b.sequence};
}

constexpr ConfigRecord make_safe_config(std::uint32_t sequence) noexcept {
  ConfigRecord record{};
  record.sequence = sequence;
  record.crc32c = config_crc32c(record);
  return record;
}

[[nodiscard]] const char* state_name(ControllerState state) noexcept;
[[nodiscard]] const char* fault_name(FaultCode fault) noexcept;

}  // namespace parol6::safe_core
