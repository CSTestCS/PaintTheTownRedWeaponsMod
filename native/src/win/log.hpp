// Log file: CustomWeapons/_CustomWeapons.log
#pragma once
#include <string>

namespace cw {
void logInit(const std::string& folder);
void logInfo(const std::string& msg);
void logWarn(const std::string& msg);
void logError(const std::string& msg);
/// Logs only the first time a key is seen (avoids per-frame spam).
void logOnce(const std::string& key, const std::string& msg, bool warn = true);
}
