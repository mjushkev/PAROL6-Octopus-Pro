#include "p6b1_protocol.hpp"

#include <cstring>

namespace parol6::p6b1 {

std::uint16_t read_u16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>(data[0]) |
         (static_cast<std::uint16_t>(data[1]) << 8U);
}

std::uint32_t read_u32(const std::uint8_t* data) {
  return static_cast<std::uint32_t>(data[0]) |
         (static_cast<std::uint32_t>(data[1]) << 8U) |
         (static_cast<std::uint32_t>(data[2]) << 16U) |
         (static_cast<std::uint32_t>(data[3]) << 24U);
}

std::int32_t read_i32(const std::uint8_t* data) {
  return static_cast<std::int32_t>(read_u32(data));
}

void write_u16(std::uint8_t* data, std::uint16_t value) {
  data[0] = static_cast<std::uint8_t>(value);
  data[1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(std::uint8_t* data, std::uint32_t value) {
  data[0] = static_cast<std::uint8_t>(value);
  data[1] = static_cast<std::uint8_t>(value >> 8U);
  data[2] = static_cast<std::uint8_t>(value >> 16U);
  data[3] = static_cast<std::uint8_t>(value >> 24U);
}

void write_i32(std::uint8_t* data, std::int32_t value) {
  write_u32(data, static_cast<std::uint32_t>(value));
}

std::uint32_t calculate_crc32c(const std::uint8_t* data, std::size_t size) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = 0U; index < size; ++index) {
    crc ^= data[index];
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc >> 1U) ^ ((crc & 1U) ? 0x82F63B78U : 0U);
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

void FrameParser::reset_with(std::uint8_t byte) {
  size_ = 0U;
  expected_ = 0U;
  if (byte == kMagic[0]) bytes_[size_++] = byte;
}

bool FrameParser::push(std::uint8_t byte, PacketView& packet) {
  if (size_ < kMagic.size()) {
    if (byte == kMagic[size_]) {
      bytes_[size_++] = byte;
    } else {
      reset_with(byte);
    }
    return false;
  }
  if (size_ >= bytes_.size()) {
    ++bad_frames_;
    reset_with(byte);
    return false;
  }
  bytes_[size_++] = byte;
  if (size_ == kHeaderSize) {
    const std::uint16_t payload_size = read_u16(bytes_.data() + 12U);
    if (payload_size > kMaximumPayload || bytes_[4] != kVersion) {
      ++bad_frames_;
      reset_with(byte);
      return false;
    }
    expected_ = kHeaderSize + payload_size + kCrcSize;
  }
  if (expected_ == 0U || size_ < expected_) return false;

  const std::uint32_t expected_crc = read_u32(bytes_.data() + expected_ - kCrcSize);
  const std::uint32_t actual_crc = calculate_crc32c(bytes_.data(), expected_ - kCrcSize);
  if (expected_crc != actual_crc) {
    ++bad_frames_;
    reset_with(byte);
    return false;
  }
  packet.type = static_cast<MessageType>(bytes_[5]);
  packet.flags = read_u16(bytes_.data() + 6U);
  packet.sequence = read_u32(bytes_.data() + 8U);
  packet.payload_size = read_u16(bytes_.data() + 12U);
  packet.payload = bytes_.data() + kHeaderSize;
  size_ = 0U;
  expected_ = 0U;
  return true;
}

bool decode_setpoint(const std::uint8_t* data, std::size_t size, Setpoint& point) {
  if (size != kSetpointSize) return false;
  for (std::size_t axis = 0U; axis < kJointCount; ++axis) {
    point.positions_steps[axis] = read_i32(data + axis * 4U);
    point.speeds_steps_s[axis] = read_u32(data + 24U + axis * 4U);
  }
  point.io_bits = read_u16(data + 48U);
  point.command = data[50U];
  return true;
}

bool send_packet(Print& output, MessageType type, std::uint32_t sequence,
                 const std::uint8_t* payload, std::uint16_t payload_size,
                 std::uint16_t flags) {
  if (payload_size > kMaximumPayload) return false;
  static std::array<std::uint8_t, kMaximumFrame> frame{};
  std::memcpy(frame.data(), kMagic.data(), kMagic.size());
  frame[4] = kVersion;
  frame[5] = static_cast<std::uint8_t>(type);
  write_u16(frame.data() + 6U, flags);
  write_u32(frame.data() + 8U, sequence);
  write_u16(frame.data() + 12U, payload_size);
  if (payload_size > 0U && payload != nullptr) {
    std::memcpy(frame.data() + kHeaderSize, payload, payload_size);
  }
  const std::size_t without_crc = kHeaderSize + payload_size;
  write_u32(frame.data() + without_crc, calculate_crc32c(frame.data(), without_crc));
  return output.write(frame.data(), without_crc + kCrcSize) == without_crc + kCrcSize;
}

}  // namespace parol6::p6b1
