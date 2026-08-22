#include "binary_protocol.hpp"

#include <algorithm>

namespace parol6::binary {
namespace {

template <typename T>
void append_le(std::array<std::uint8_t, kMaxBodyBytes>& output,
               std::size_t& offset, T value) noexcept {
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    output[offset++] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

template <typename T>
T read_le(const std::uint8_t* input, std::size_t offset) noexcept {
  T value{};
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    value |= static_cast<T>(input[offset + index]) << (index * 8U);
  }
  return value;
}

constexpr std::array<std::uint8_t, 32> kGoldenHeartbeatUart{{
    0x01, 0x03, 0x01, 0x04, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x05, 0xD1, 0xF5, 0x45, 0x1C, 0x00,
}};

}  // namespace

std::uint32_t crc32c(const std::uint8_t* data, std::size_t length) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (std::uint8_t bit = 0; bit < 8U; ++bit) {
      crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0x82F63B78U : 0U);
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

DecodeResult decode_body(const std::uint8_t* body, std::size_t length,
                         Frame& frame) noexcept {
  if (length < kHeaderBytes + kCrcBytes) {
    return DecodeResult::body_too_short;
  }
  if (length > kMaxBodyBytes) {
    return DecodeResult::decoded_too_large;
  }
  const auto version = read_le<std::uint16_t>(body, 0);
  if ((version >> 8U) != protocol::kProtocolMajor) {
    return DecodeResult::bad_version;
  }
  const auto payload_length = read_le<std::uint16_t>(body, 4);
  if (payload_length > protocol::kMaxPayloadBytes ||
      length != kHeaderBytes + payload_length + kCrcBytes) {
    return DecodeResult::bad_length;
  }
  const auto received_crc = read_le<std::uint32_t>(body, length - kCrcBytes);
  if (crc32c(body, length - kCrcBytes) != received_crc) {
    return DecodeResult::bad_crc;
  }

  frame.header.version = version;
  frame.header.message_type = static_cast<protocol::MessageType>(body[2]);
  frame.header.flags = body[3];
  frame.header.payload_length = payload_length;
  frame.header.session_id = read_le<std::uint32_t>(body, 6);
  frame.header.sequence = read_le<std::uint32_t>(body, 10);
  frame.header.acknowledgement = read_le<std::uint32_t>(body, 14);
  frame.header.sender_time_us = read_le<std::uint64_t>(body, 18);
  std::copy_n(body + kHeaderBytes, payload_length, frame.payload.begin());
  return DecodeResult::ok;
}

std::size_t encode_body(
    const Frame& frame,
    std::array<std::uint8_t, kMaxBodyBytes>& output) noexcept {
  if (frame.header.payload_length > protocol::kMaxPayloadBytes) {
    return 0;
  }
  std::size_t offset = 0;
  append_le(output, offset, frame.header.version);
  output[offset++] = static_cast<std::uint8_t>(frame.header.message_type);
  output[offset++] = frame.header.flags;
  append_le(output, offset, frame.header.payload_length);
  append_le(output, offset, frame.header.session_id);
  append_le(output, offset, frame.header.sequence);
  append_le(output, offset, frame.header.acknowledgement);
  append_le(output, offset, frame.header.sender_time_us);
  std::copy_n(frame.payload.begin(), frame.header.payload_length,
              output.begin() + offset);
  offset += frame.header.payload_length;
  append_le(output, offset, crc32c(output.data(), offset));
  return offset;
}

std::size_t cobs_encode(
    const std::uint8_t* input, std::size_t length,
    std::array<std::uint8_t, kMaxUartPacketBytes>& output) noexcept {
  if (length > kMaxBodyBytes) {
    return 0;
  }
  std::size_t output_length = 1;
  std::size_t code_index = 0;
  std::uint8_t code = 1;
  for (std::size_t index = 0; index < length; ++index) {
    if (input[index] == 0U) {
      output[code_index] = code;
      code_index = output_length++;
      code = 1;
    } else {
      output[output_length++] = input[index];
      ++code;
      if (code == 0xFFU) {
        output[code_index] = code;
        code_index = output_length++;
        code = 1;
      }
    }
  }
  output[code_index] = code;
  return output_length;
}

DecodeResult cobs_decode(
    const std::uint8_t* input, std::size_t length,
    std::array<std::uint8_t, kMaxBodyBytes>& output,
    std::size_t& output_length) noexcept {
  output_length = 0;
  if (length == 0U) {
    return DecodeResult::empty;
  }
  std::size_t input_index = 0;
  while (input_index < length) {
    const auto code = input[input_index++];
    if (code == 0U) {
      return DecodeResult::malformed_cobs;
    }
    const std::size_t block_end = input_index + code - 1U;
    if (block_end > length) {
      return DecodeResult::malformed_cobs;
    }
    while (input_index < block_end) {
      if (output_length >= output.size()) {
        return DecodeResult::decoded_too_large;
      }
      output[output_length++] = input[input_index++];
    }
    if (code != 0xFFU && input_index < length) {
      if (output_length >= output.size()) {
        return DecodeResult::decoded_too_large;
      }
      output[output_length++] = 0;
    }
  }
  return DecodeResult::ok;
}

std::size_t encode_uart(
    const Frame& frame, std::array<std::uint8_t, kMaxBodyBytes>& body,
    std::array<std::uint8_t, kMaxUartPacketBytes>& output) noexcept {
  const auto body_length = encode_body(frame, body);
  if (body_length == 0U) {
    return 0;
  }
  auto output_length = cobs_encode(body.data(), body_length, output);
  if (output_length == 0U || output_length >= output.size()) {
    return 0;
  }
  output[output_length++] = 0;
  return output_length;
}

FeedResult StreamDecoder::feed(const std::uint8_t byte, Frame& frame) noexcept {
  if (byte != 0U) {
    if (overflow_ || encoded_length_ >= encoded_.size()) {
      overflow_ = true;
      return FeedResult::none;
    }
    encoded_[encoded_length_++] = byte;
    return FeedResult::none;
  }

  if (overflow_) {
    last_error_ = DecodeResult::encoded_too_large;
    reset();
    return FeedResult::rejected;
  }
  std::size_t body_length = 0;
  last_error_ =
      cobs_decode(encoded_.data(), encoded_length_, body_, body_length);
  encoded_length_ = 0;
  if (last_error_ != DecodeResult::ok) {
    return FeedResult::rejected;
  }
  last_error_ = decode_body(body_.data(), body_length, frame);
  return last_error_ == DecodeResult::ok ? FeedResult::frame_ready
                                         : FeedResult::rejected;
}

void StreamDecoder::reset() noexcept {
  encoded_length_ = 0;
  overflow_ = false;
}

ReplayDecision ReplayWindow::check_and_mark(
    const std::uint32_t sequence) noexcept {
  if (!initialized_) {
    initialized_ = true;
    highest_ = sequence;
    bitmap_ = 1U;
    return ReplayDecision::accept;
  }
  if (sequence > highest_) {
    const auto shift = sequence - highest_;
    bitmap_ = shift >= protocol::kReplayWindow ? 0U : bitmap_ << shift;
    bitmap_ |= 1U;
    highest_ = sequence;
    return ReplayDecision::accept;
  }
  const auto offset = highest_ - sequence;
  if (offset >= protocol::kReplayWindow) {
    return ReplayDecision::too_old;
  }
  const std::uint64_t mask = 1ULL << offset;
  if ((bitmap_ & mask) != 0U) {
    return ReplayDecision::duplicate;
  }
  bitmap_ |= mask;
  return ReplayDecision::accept;
}

void write_u16(std::uint8_t* output, const std::uint16_t value) noexcept {
  output[0] = static_cast<std::uint8_t>(value);
  output[1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(std::uint8_t* output, const std::uint32_t value) noexcept {
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    output[index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

bool self_test() noexcept {
  StreamDecoder decoder;
  Frame frame;
  FeedResult result = FeedResult::none;
  for (const auto byte : kGoldenHeartbeatUart) {
    result = decoder.feed(byte, frame);
  }
  if (result != FeedResult::frame_ready ||
      frame.header.version != kProtocolVersion ||
      frame.header.message_type != protocol::MessageType::HEARTBEAT ||
      frame.header.payload_length != 0U) {
    return false;
  }
  static constexpr std::array<std::uint8_t, 9> check{{
      '1', '2', '3', '4', '5', '6', '7', '8', '9',
  }};
  if (crc32c(check.data(), check.size()) != 0xE3069283U) {
    return false;
  }
  ReplayWindow replay;
  return replay.check_and_mark(10U) == ReplayDecision::accept &&
         replay.check_and_mark(10U) == ReplayDecision::duplicate &&
         replay.check_and_mark(12U) == ReplayDecision::accept &&
         replay.check_and_mark(11U) == ReplayDecision::accept &&
         replay.check_and_mark(11U) == ReplayDecision::duplicate;
}

static_assert(kMaxBodyBytes == 2078U);
static_assert(kMaxUartPacketBytes == 2088U);
static_assert(protocol::kReplayWindow == 64U);
static_assert(protocol::kHeartbeatReplyPayloadBytes == 28U);
static_assert(protocol::kDeviceInfoPayloadBytes == 64U);
static_assert(protocol::kNackPayloadBytes == 8U);

}  // namespace parol6::binary
