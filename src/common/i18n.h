#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// Tiny localisation layer (no gettext, no external dependency).
//
// Every user facing string is written in the source as simplified Chinese and
// passed through i18n::t(); the key *is* the Chinese text, which keeps the code
// readable and makes a missing translation fall back to something meaningful
// instead of a raw key id.
//
// i18n::tf() does the same for std::format templates: std::format needs a
// compile time format string, so a translated one has to go through
// std::vformat at runtime.
// ---------------------------------------------------------------------------
namespace i18n {
	enum class Lang : uint8_t {
		ZhCN = 0,
		ZhTW,
		En,
		Ja,
		Ko,
		Ru,
		Es,
		Fr,
		De,
		Pt,
		COUNT
	};

	constexpr int LANGUAGE_COUNT = static_cast<int>(Lang::COUNT);

	/// Currently selected language (defaults to simplified Chinese).
	inline Lang current = Lang::ZhCN;

	/// BCP-47-ish code used in the config file: "zh-CN", "en", ...
	[[nodiscard]] const char* code(Lang lang);

	/// Language name written in that language ("简体中文", "English", ...).
	[[nodiscard]] const char* endonym(Lang lang);

	[[nodiscard]] Lang from_code(std::string_view code);

	/// Switch language; also re-rasterises nothing, the font atlas already
	/// carries every language.
	void set(Lang lang);

	/// Translate a simplified Chinese source string.
	[[nodiscard]] const char* t(const char* zh_key);

	/// Translate a std::format template and format it.
	template <typename... Args>
	std::string tf(const char* zh_format, Args&&... args) {
		return std::vformat(t(zh_format), std::make_format_args(args...));
	}

	/// Every character used by any translation, for the font glyph ranges.
	[[nodiscard]] const std::string& all_characters();
}
