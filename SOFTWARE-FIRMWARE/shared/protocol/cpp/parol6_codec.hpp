#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "../../generated/protocol_v1.hpp"

namespace parol6::protocol {

struct FrameHeader {
  std::uint16_t version{};
  MessageType message_type{};
  std::uint8_t flags{};
  std::uint16_t payload_length{};
  std::uint32_t session_id{};
  std::uint32_t sequence{};
  std::uint32_t acknowledgement{};
  std::uint64_t sender_time_us{};
};

struct Frame {
  FrameHeader header{};
  std::vector<std::uint8_t> payload{};
};

enum class DecodeResult {
  OK,
  EMPTY,
  TOO_LARGE,
  MALFORMED_COBS,
  SHORT_BODY,
  BAD_VERSION,
  BAD_LENGTH,
  BAD_CRC,
};

std::uint32_t crc32c(std::span<const std::uint8_t> input);
std::vector<std::uint8_t> cobs_encode(std::span<const std::uint8_t> input);
DecodeResult cobs_decode(std::span<const std::uint8_t> input,
                         std::vector<std::uint8_t>& output);
std::vector<std::uint8_t> encode_body(const Frame& frame);
DecodeResult decode_body(std::span<const std::uint8_t> body, Frame& frame);

}  // namespace parol6::protocol

