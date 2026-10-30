// SPDX-License-Identifier: MIT
#pragma once

#include "core/LanguageRuleEngine.hpp"

namespace st {

/// Load the same saved preferences as the desktop settings panel. Native hosts
/// call this on activation so an engine never performs disk I/O while typing.
EnginePtr loadNativeEngine();

} // namespace st
