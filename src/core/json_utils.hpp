// src/core/json_utils.hpp — JSON string unescape utility.
#pragma once
#include <string>
#include <string_view>

namespace pcp::desktop {

std::string jsonUnescape(std::string_view s);

} // namespace pcp::desktop
