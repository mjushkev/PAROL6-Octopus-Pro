#include "safe_core.hpp"

namespace parol6::safe_core {

const char* state_name(const ControllerState state) noexcept {
  switch (state) {
    case ControllerState::boot_self_test:
      return "boot_self_test";
    case ControllerState::not_commissioned:
      return "not_commissioned";
    case ControllerState::disarmed:
      return "disarmed";
    case ControllerState::protective_stop:
      return "protective_stop";
    case ControllerState::fault_latched:
      return "fault_latched";
  }
  return "unknown";
}

const char* fault_name(const FaultCode fault) noexcept {
  switch (fault) {
    case FaultCode::none:
      return "none";
    case FaultCode::config_invalid:
      return "config_invalid";
    case FaultCode::link_stale:
      return "link_stale";
    case FaultCode::protocol_violation:
      return "protocol_violation";
  }
  return "unknown";
}

namespace {

constexpr bool test_config_selection() {
  const auto old_slot = make_safe_config(10U);
  const auto new_slot = make_safe_config(11U);
  const auto selected = select_config(old_slot, new_slot);
  if (!selected.valid || selected.slot != 1U || selected.sequence != 11U) {
    return false;
  }
  auto corrupt = new_slot;
  corrupt.crc32c ^= 1U;
  const auto fallback = select_config(old_slot, corrupt);
  if (!fallback.valid || fallback.slot != 0U || fallback.sequence != 10U) {
    return false;
  }
  auto unsafe = old_slot;
  unsafe.hardware_outputs_enabled = 1U;
  unsafe.crc32c = config_crc32c(unsafe);
  return !config_valid(unsafe);
}

constexpr bool test_event_log_bounds() {
  EventLog log;
  for (std::size_t index = 0; index < kEventCapacity + 3U; ++index) {
    log.push({static_cast<std::uint32_t>(index), EventCode::boot,
              static_cast<std::uint16_t>(index)});
  }
  return log.size() == kEventCapacity && log.total() == kEventCapacity + 3U &&
         log.oldest(0).timestamp_ms == 3U &&
         log.oldest(kEventCapacity - 1U).timestamp_ms == kEventCapacity + 2U;
}

constexpr bool test_watchdog_fail_closed() {
  SafeCore core;
  const auto safe = make_safe_config(1U);
  core.boot(safe, ConfigRecord{}, 0U);
  core.begin_control_session(100U);
  if (core.poll(400U)) {
    return false;
  }
  if (!core.poll(401U)) {
    return false;
  }
  return core.state() == ControllerState::protective_stop &&
         core.fault() == FaultCode::link_stale && core.outputs().all_safe();
}

static_assert(test_config_selection());
static_assert(test_event_log_bounds());
static_assert(test_watchdog_fail_closed());

}  // namespace
}  // namespace parol6::safe_core
