#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../../shared/generated/protocol_v1.hpp"

namespace parol6::binary {

inline constexpr std::uint16_t kProtocolVersion =
    static_cast<std::uint16_t>(protocol::kProtocolMajor) << 8U |
    protocol::kProtocolMinor;
inline constexpr std::size_t kHeaderBytes = 26;
inline constexpr std::size_t kCrcBytes = 4;
inline constexpr std::size_t kMaxBodyBytes =
    kHeaderBytes + protocol::kMaxPayloadBytes + kCrcBytes;
inline constexpr std::size_t kMaxEncodedBytes =
    kMaxBodyBytes + (kMaxBodyBytes / 254U) + 1U;
inline constexpr std::size_t kMaxUartPacketBytes = kMaxEncodedBytes + 1U;

struct FrameHeader final {
  std::uint16_t version{kProtocolVersion};
  protocol::MessageType message_type{protocol::MessageType::NACK};
  std::uint8_t flags{0};
  std::uint16_t payload_length{0};
  std::uint32_t session_id{0};
  std::uint32_t sequence{0};
  std::uint32_t acknowledgement{0};
  std::uint64_t sender_time_us{0};
};

struct Frame final {
  FrameHeader header{};
  std::array<std::uint8_t, protocol::kMaxPayloadBytes> payload{};
};

enum class DecodeResult : std::uint8_t {
  ok,
  empty,
  encoded_too_large,
  malformed_cobs,
  decoded_too_large,
  body_too_short,
  bad_version,
  bad_length,
  bad_crc,
};

enum class FeedResult : std::uint8_t {
  none,
  frame_ready,
  rejected,
};

[[nodiscard]] std::uint32_t crc32c(const std::uint8_t* data,
                                   std::size_t length) noexcept;
[[nodiscard]] DecodeResult decode_body(const std::uint8_t* body,
                                       std::size_t length,
                                       Frame& frame) noexcept;
[[nodiscard]] std::size_t encode_body(
    const Frame& frame, std::array<std::uint8_t, kMaxBodyBytes>& output) noexcept;
[[nodiscard]] std::size_t cobs_encode(
    const std::uint8_t* input, std::size_t length,
    std::array<std::uint8_t, kMaxUartPacketBytes>& output) noexcept;
[[nodiscard]] DecodeResult cobs_decode(
    const std::uint8_t* input, std::size_t length,
    std::array<std::uint8_t, kMaxBodyBytes>& output,
    std::size_t& output_length) noexcept;
[[nodiscard]] std::size_t encode_uart(
    const Frame& frame, std::array<std::uint8_t, kMaxBodyBytes>& body,
    std::array<std::uint8_t, kMaxUartPacketBytes>& output) noexcept;

class StreamDecoder final {
 public:
  FeedResult feed(std::uint8_t byte, Frame& frame) noexcept;
  [[nodiscard]] DecodeResult last_error() const noexcept { return last_error_; }
  void reset() noexcept;

 private:
  std::array<std::uint8_t, kMaxEncodedBytes> encoded_{};
  std::array<std::uint8_t, kMaxBodyBytes> body_{};
  std::size_t encoded_length_{0};
  bool overflow_{false};
  DecodeResult last_error_{DecodeResult::empty};
};

enum class ReplayDecision : std::uint8_t { accept, duplicate, too_old };

class ReplayWindow final {
 public:
  ReplayDecision check_and_mark(std::uint32_t sequence) noexcept;
  void reset() noexcept {
    initialized_ = false;
    highest_ = 0;
    bitmap_ = 0;
  }

 private:
  bool initialized_{false};
  std::uint32_t highest_{0};
  std::uint64_t bitmap_{0};
};

void write_u16(std::uint8_t* output, std::uint16_t value) noexcept;
void write_u32(std::uint8_t* output, std::uint32_t value) noexcept;
[[nodiscard]] bool self_test() noexcept;

}  // namespace parol6::binary
