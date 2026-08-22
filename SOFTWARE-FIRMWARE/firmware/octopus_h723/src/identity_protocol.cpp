#include "identity_protocol.hpp"

namespace parol6::identity {

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

constexpr Request parse_overlong_command() {
  LineParser parser;
  for (std::size_t index = 0; index <= LineParser::max_command_length; ++index) {
    parser.feed('A');
  }
  return parser.feed('\n');
}

static_assert(parse("IDENTIFY") == Request::identify);
static_assert(parse("STATUS") == Request::status);
static_assert(parse("HELP") == Request::help);
static_assert(parse("identify") == Request::rejected);
static_assert(parse("MOVE") == Request::rejected);
static_assert(parse("") == Request::none);
static_assert(parse_overlong_command() == Request::too_long);

}  // namespace parol6::identity
