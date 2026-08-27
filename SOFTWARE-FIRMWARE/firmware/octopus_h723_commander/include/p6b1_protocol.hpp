#pragma once

#include <Arduino.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace parol6::p6b1 {

constexpr std::array<std::uint8_t, 4> kMagic = {'P', '6', 'B', '1'};
constexpr std::uint8_t kVersion = 1U;
constexpr std::size_t kHeaderSize = 14U;
constexpr std::size_t kCrcSize = 4U;
constexpr std::size_t kMaximumPayload = 4096U;
constexpr std::size_t kMaximumFrame = kHeaderSize + kMaximumPayload + kCrcSize;
constexpr std::size_t kJointCount = 6U;
constexpr std::size_t kSetpointSize = 52U;

enum class MessageType : std::uint8_t {
  hello = 1U,
  hello_ack = 2U,
  enqueue = 3U,
  ack = 4U,
  status = 5U,
  start = 6U,
  stop = 7U,
  clear = 8U,
  home = 9U,
  set_j1_home_mode = 10U,
  io = 11U,
  finish = 12U,
  error = 255U,
};

enum Capability : std::uint32_t {
  buffered_absolute_steps = 1U << 0U,
  crc32c = 1U << 1U,
  priority_stop = 1U << 2U,
  queue_watchdog = 1U << 3U,
  owner_profile = 1U << 4U,
  j1_manual_auto_home = 1U << 5U,
  graceful_finish_hold = 1U << 6U,
};

constexpr std::uint32_t kRequiredCapabilities =
    buffered_absolute_steps | crc32c | priority_stop | queue_watchdog |
    owner_profile | j1_manual_auto_home | graceful_finish_hold;

enum ControllerState : std::uint32_t {
  idle = 1U << 0U,
  armed = 1U << 1U,
  running = 1U << 2U,
  stopped = 1U << 3U,
  fault = 1U << 4U,
  homed = 1U << 5U,
  motors_enabled = 1U << 6U,
};

enum Fault : std::uint32_t {
  no_fault = 0U,
  bad_frame = 1U << 0U,
  bad_version = 1U << 1U,
  capability_mismatch = 1U << 2U,
  replay = 1U << 3U,
  queue_overflow = 1U << 4U,
  queue_underrun = 1U << 5U,
  watchdog = 1U << 6U,
  limit = 1U << 7U,
  estop = 1U << 8U,
};

enum class ErrorCode : std::uint16_t {
  bad_frame = 1U,
  bad_version = 2U,
  capability_mismatch = 3U,
  replay = 4U,
  queue_overflow = 5U,
  not_armed = 6U,
  invalid_payload = 7U,
  fault_latched = 8U,
};

struct PacketView {
  MessageType type = MessageType::error;
  std::uint16_t flags = 0U;
  std::uint32_t sequence = 0U;
  const std::uint8_t* payload = nullptr;
  std::uint16_t payload_size = 0U;
};

struct Setpoint {
  std::array<std::int32_t, kJointCount> positions_steps{};
  std::array<std::uint32_t, kJointCount> speeds_steps_s{};
  std::uint16_t io_bits = 0U;
  std::uint8_t command = 0U;
};

class FrameParser {
 public:
  bool push(std::uint8_t byte, PacketView& packet);
  std::uint32_t bad_frames() const { return bad_frames_; }

 private:
  void reset_with(std::uint8_t byte);
  std::array<std::uint8_t, kMaximumFrame> bytes_{};
  std::size_t size_ = 0U;
  std::size_t expected_ = 0U;
  std::uint32_t bad_frames_ = 0U;
};

template <std::size_t Capacity>
class SetpointQueue {
 public:
  bool push(const Setpoint& value) {
    if (size_ == Capacity) return false;
    values_[head_] = value;
    head_ = (head_ + 1U) % Capacity;
    ++size_;
    return true;
  }

  bool pop(Setpoint& value) {
    if (size_ == 0U) return false;
    value = values_[tail_];
    tail_ = (tail_ + 1U) % Capacity;
    --size_;
    return true;
  }

  void clear() { head_ = tail_ = size_ = 0U; }
  std::size_t size() const { return size_; }
  constexpr std::size_t capacity() const { return Capacity; }

 private:
  std::array<Setpoint, Capacity> values_{};
  std::size_t head_ = 0U;
  std::size_t tail_ = 0U;
  std::size_t size_ = 0U;
};

std::uint16_t read_u16(const std::uint8_t* data);
std::uint32_t read_u32(const std::uint8_t* data);
std::int32_t read_i32(const std::uint8_t* data);
void write_u16(std::uint8_t* data, std::uint16_t value);
void write_u32(std::uint8_t* data, std::uint32_t value);
void write_i32(std::uint8_t* data, std::int32_t value);
std::uint32_t calculate_crc32c(const std::uint8_t* data, std::size_t size);
bool decode_setpoint(const std::uint8_t* data, std::size_t size, Setpoint& point);
bool send_packet(Print& output, MessageType type, std::uint32_t sequence,
                 const std::uint8_t* payload, std::uint16_t payload_size,
                 std::uint16_t flags = 0U);

}  // namespace parol6::p6b1
