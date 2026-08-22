#pragma once

#include <cstddef>
#include <cstdint>

namespace parol6::identity {

enum class Request : std::uint8_t {
  none,
  identify,
  status,
  help,
  rejected,
  too_long,
};

class LineParser final {
 public:
  static constexpr std::size_t max_command_length = 31;

  constexpr Request feed(const std::uint8_t byte) noexcept {
    if (byte == '\r') {
      return Request::none;
    }
    if (byte == '\n') {
      return finish_line();
    }
    if (byte < 0x20U || byte > 0x7eU) {
      overflow_ = true;
      return Request::none;
    }
    if (length_ >= max_command_length) {
      overflow_ = true;
      return Request::none;
    }
    buffer_[length_++] = static_cast<char>(byte);
    buffer_[length_] = '\0';
    return Request::none;
  }

  constexpr void reset() noexcept {
    for (auto& byte : buffer_) {
      byte = '\0';
    }
    length_ = 0;
    overflow_ = false;
  }

 private:
  constexpr bool equals(const char* expected) const noexcept {
    std::size_t index = 0;
    while (expected[index] != '\0') {
      if (index >= length_ || buffer_[index] != expected[index]) {
        return false;
      }
      ++index;
    }
    return index == length_;
  }

  constexpr Request finish_line() noexcept {
    Request result = Request::none;
    if (overflow_) {
      result = Request::too_long;
    } else if (length_ == 0) {
      result = Request::none;
    } else if (equals("IDENTIFY")) {
      result = Request::identify;
    } else if (equals("STATUS")) {
      result = Request::status;
    } else if (equals("HELP")) {
      result = Request::help;
    } else {
      result = Request::rejected;
    }
    reset();
    return result;
  }

  char buffer_[max_command_length + 1]{};
  std::size_t length_{0};
  bool overflow_{false};
};

}  // namespace parol6::identity
