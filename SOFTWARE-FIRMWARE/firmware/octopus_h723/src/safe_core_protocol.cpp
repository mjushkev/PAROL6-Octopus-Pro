#include "safe_core_protocol.hpp"

namespace parol6::safe_core {
namespace {

constexpr Request parse(const char* command) {
  LineParser parser;
  std::size_t index = 0;
  while (command[index] != '\0') {
    if (parser.feed(static_cast<std::uint8_t>(command[index])) != Request::none) {
      return Request::rejected;
    }
    ++index;
  }
  return parser.feed('\n');
}

static_assert(parse("IDENTIFY") == Request::identify);
static_assert(parse("STATUS") == Request::status);
static_assert(parse("HEARTBEAT") == Request::heartbeat);
static_assert(parse("DIAGNOSTICS") == Request::diagnostics);
static_assert(parse("HELP") == Request::help);
static_assert(parse("MOVE") == Request::rejected);
static_assert(parse("MOTOR_ENABLE") == Request::rejected);
static_assert(parse("") == Request::none);

}  // namespace
}  // namespace parol6::safe_core
