#include "i18n.h"

#include "i18n_table.h"

#include <unordered_map>

namespace {
	constexpr std::string_view CODES[i18n::LANGUAGE_COUNT] = {
		"zh-CN", "zh-TW", "en", "ja", "ko", "ru", "es", "fr", "de", "pt"
	};

	constexpr std::string_view ENDONYMS[i18n::LANGUAGE_COUNT] = {
		"简体中文", "繁體中文", "English", "日本語", "한국어", "Русский", "Español", "Français", "Deutsch", "Português"
	};

	/// Key -> entry lookup, built once.
	const std::unordered_map<std::string_view, const i18n_table::Entry*>& index() {
		static const std::unordered_map<std::string_view, const i18n_table::Entry*> map = [] {
			std::unordered_map<std::string_view, const i18n_table::Entry*> m;
			m.reserve(i18n_table::ENTRY_COUNT * 2);

			for (const auto& entry : i18n_table::ENTRIES) {
				m.emplace(entry.zh, &entry);
			}

			return m;
		}();

		return map;
	}
}

const char* i18n::code(Lang lang) {
	const int i = static_cast<int>(lang);
	return (i >= 0 && i < LANGUAGE_COUNT) ? CODES[i].data() : CODES[0].data();
}

const char* i18n::endonym(Lang lang) {
	const int i = static_cast<int>(lang);
	return (i >= 0 && i < LANGUAGE_COUNT) ? ENDONYMS[i].data() : ENDONYMS[0].data();
}

i18n::Lang i18n::from_code(std::string_view code_value) {
	for (int i = 0; i < LANGUAGE_COUNT; ++i) {
		if (CODES[i] == code_value)
			return static_cast<Lang>(i);
	}

	return Lang::ZhCN;
}

void i18n::set(Lang lang) {
	current = lang;
}

const char* i18n::t(const char* zh_key) {
	if (zh_key == nullptr || current == Lang::ZhCN)
		return zh_key;

	const auto it = index().find(zh_key);
	if (it == index().end())
		return zh_key;

	const char* translated = it->second->values[static_cast<int>(current) - 1];
	return (translated != nullptr && *translated != '\0') ? translated : zh_key;
}

const std::string& i18n::all_characters() {
	static const std::string characters = [] {
		std::string all;

		for (const auto& entry : i18n_table::ENTRIES) {
			all += entry.zh;

			for (const char* value : entry.values) {
				if (value != nullptr)
					all += value;
			}
		}

		// language names are shown in the picker but do not live in the tables
		for (const auto& name : ENDONYMS) {
			all += name;
		}

		return all;
	}();

	return characters;
}
