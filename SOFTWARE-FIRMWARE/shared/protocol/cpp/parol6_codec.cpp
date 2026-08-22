#include "parol6_codec.hpp"

#include <limits>

namespace parol6::protocol {
namespace {
constexpr std::size_t kHeaderBytes = 26;
constexpr std::size_t kCrcBytes = 4;
constexpr std::size_t kMaxBodyBytes = kHeaderBytes + kMaxPayloadBytes + kCrcBytes;

template <typename T>
void append_le(std::vector<std::uint8_t>& output, T value) {
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    output.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
  }
}

template <typename T>
T read_le(std::span<const std::uint8_t> input, std::size_t offset) {
  T value{};
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    value |= static_cast<T>(input[offset + index]) << (index * 8);
  }
  return value;
}
}  // namespace

std::uint32_t crc32c(std::span<const std::uint8_t> input) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const auto byte : input) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ ((crc & 1U) ? 0x82F63B78U : 0U);
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

std::vector<std::uint8_t> cobs_encode(std::span<const std::uint8_t> input) {
  std::vector<std::uint8_t> output(1, 0);
  std::size_t code_index = 0;
  std::uint8_t code = 1;
  for (const auto byte : input) {
    if (byte == 0) {
      output[code_index] = code;
      code_index = output.size();
      output.push_back(0);
      code = 1;
    } else {
      output.push_back(byte);
      ++code;
      if (code == 0xFF) {
        output[code_index] = code;
        code_index = output.size();
        output.push_back(0);
        code = 1;
      }
    }
  }
  output[code_index] = code;
  return output;
}

DecodeResult cobs_decode(std::span<const std::uint8_t> input,
                         std::vector<std::uint8_t>& output) {
  output.clear();
  if (input.empty()) return DecodeResult::EMPTY;
  std::size_t index = 0;
  while (index < input.size()) {
    const auto code = input[index++];
    if (code == 0) return DecodeResult::MALFORMED_COBS;
    const std::size_t end = index + code - 1;
    if (end > input.size()) return DecodeResult::MALFORMED_COBS;
    output.insert(output.end(), input.begin() + index, input.begin() + end);
    index = end;
    if (code != 0xFF && index < input.size()) output.push_back(0);
    if (output.size() > kMaxBodyBytes) return DecodeResult::TOO_LARGE;
  }
  return DecodeResult::OK;
}

std::vector<std::uint8_t> encode_body(const Frame& frame) {
  if (frame.payload.size() > kMaxPayloadBytes ||
      frame.payload.size() > std::numeric_limits<std::uint16_t>::max()) {
    return {};
  }
  std::vector<std::uint8_t> body;
  body.reserve(kHeaderBytes + frame.payload.size() + kCrcBytes);
  append_le(body, frame.header.version);
  body.push_back(static_cast<std::uint8_t>(frame.header.message_type));
  body.push_back(frame.header.flags);
  append_le(body, static_cast<std::uint16_t>(frame.payload.size()));
  append_le(body, frame.header.session_id);
  append_le(body, frame.header.sequence);
  append_le(body, frame.header.acknowledgement);
  append_le(body, frame.header.sender_time_us);
  body.insert(body.end(), frame.payload.begin(), frame.payload.end());
  append_le(body, crc32c(body));
  return body;
}

DecodeResult decode_body(std::span<const std::uint8_t> body, Frame& frame) {
  if (body.size() < kHeaderBytes + kCrcBytes) return DecodeResult::SHORT_BODY;
  if (body.size() > kMaxBodyBytes) return DecodeResult::TOO_LARGE;
  const auto version = read_le<std::uint16_t>(body, 0);
  if ((version >> 8) != kProtocolMajor) return DecodeResult::BAD_VERSION;
  const auto payload_length = read_le<std::uint16_t>(body, 4);
  if (body.size() != kHeaderBytes + payload_length + kCrcBytes) {
    return DecodeResult::BAD_LENGTH;
  }
  const auto received_crc = read_le<std::uint32_t>(body, body.size() - kCrcBytes);
  if (crc32c(body.first(body.size() - kCrcBytes)) != received_crc) {
    return DecodeResult::BAD_CRC;
  }
  frame.header.version = version;
  frame.header.message_type = static_cast<MessageType>(body[2]);
  frame.header.flags = body[3];
  frame.header.payload_length = payload_length;
  frame.header.session_id = read_le<std::uint32_t>(body, 6);
  frame.header.sequence = read_le<std::uint32_t>(body, 10);
  frame.header.acknowledgement = read_le<std::uint32_t>(body, 14);
  frame.header.sender_time_us = read_le<std::uint64_t>(body, 18);
  frame.payload.assign(body.begin() + kHeaderBytes, body.end() - kCrcBytes);
  return DecodeResult::OK;
}

}  // namespace parol6::protocol

