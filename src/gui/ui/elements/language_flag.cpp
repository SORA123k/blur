#include "../ui.h"
#include "../../render/render.h"
#include "../theme.h"
#include "common/i18n.h"
#include "../keys.h"
#include "../flags.h"

// Real flag graphics (public domain PNGs, embedded in the exe) drawn as textures
// inside a rounded frame.
namespace {
	constexpr int FLAG_H = 18;
	constexpr int FLAG_GAP = 7;
	constexpr float FLAG_ROUNDING = 3.f;

	render::Texture* flag_texture(i18n::Lang lang) {
		// Leaked on purpose: the atlas/textures outlive the gl context at exit and
		// there are only ten of them.
		static std::array<render::Texture*, i18n::LANGUAGE_COUNT> cache{};

		const int index = static_cast<int>(lang);
		if (index < 0 || index >= i18n::LANGUAGE_COUNT)
			return nullptr;

		if (cache[index] == nullptr) {
			auto* texture = new render::Texture();

			if (!texture->load_from_memory(flags::BY_LANG[index].data, flags::BY_LANG[index].size)) {
				u::log_error("failed to load flag texture for {}", i18n::code(lang));
			}


			cache[index] = texture;
		}

		return cache[index];
	}
}

void ui::draw_flag(const gfx::Rect& rect, i18n::Lang lang) {
	const render::Texture* texture = flag_texture(lang);

	if (texture == nullptr || !texture->is_valid()) {
		// fallback: neutral plate, so a missing texture never looks like a bug
		render::rounded_rect_filled(rect, gfx::Color(70, 70, 84, 255), FLAG_ROUNDING);
		return;
	}

	// keep the real flag proportion: fit inside the box, centred
	const float aspect = (texture->height() > 0)
	                       ? static_cast<float>(texture->width()) / static_cast<float>(texture->height())
	                       : 1.5f;

	int draw_w = rect.w;
	int draw_h = static_cast<int>(std::lround(static_cast<float>(rect.w) / aspect));

	if (draw_h > rect.h) {
		draw_h = rect.h;
		draw_w = static_cast<int>(std::lround(static_cast<float>(rect.h) * aspect));
	}

	const gfx::Rect fitted(rect.center().x - (draw_w / 2), rect.center().y - (draw_h / 2), draw_w, draw_h);

	render::image_rounded(fitted, *texture, FLAG_ROUNDING);
}

void ui::render_language_flag(const Container& container, const AnimatedElement& element) {
	const auto& data = std::get<LanguageFlagElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;

	const gfx::Rect& rect = element.element->rect;

	gfx::Rect flag_rect = rect;
	flag_rect.h = FLAG_H;
	flag_rect.y = rect.y + ((rect.h - FLAG_H) / 2);

	// active: violet frame + glow
	if (data.active) {
		render::rounded_rect_filled(
			gfx::Rect(flag_rect.x - 3, flag_rect.y - 3, flag_rect.w + 6, flag_rect.h + 6),
			theme::accent_alpha(0.30f * anim),
			FLAG_ROUNDING + 3.f
		);
	}
	else if (hover_anim > 0.01f) {
		render::rounded_rect_filled(
			gfx::Rect(flag_rect.x - 2, flag_rect.y - 2, flag_rect.w + 4, flag_rect.h + 4),
			theme::white(0.10f * hover_anim * anim),
			FLAG_ROUNDING + 2.f
		);
	}

	draw_flag(flag_rect, data.lang);

	render::rounded_rect_stroke(
		flag_rect,
		data.active ? theme::accent.adjust_alpha(anim) : theme::white(0.18f).adjust_alpha(anim),
		FLAG_ROUNDING,
		ROUNDING_ALL,
		data.active ? 1.5f : 1.f
	);
}

bool ui::update_language_flag(const Container& container, AnimatedElement& element) {
	auto& data = std::get<LanguageFlagElementData>(element.element->data);
	auto& hover_anim = element.animations.at(hasher("hover"));

	bool hovered = element.element->rect.contains(keys::mouse_pos) && set_hovered_element(element);
	hover_anim.set_goal(hovered ? 1.f : 0.f);

	if (hovered) {
		set_cursor(SDL_SYSTEM_CURSOR_POINTER);

		if (keys::is_mouse_down() && data.on_pick) {
			data.on_pick(data.lang);
			keys::on_mouse_press_handled(SDL_BUTTON_LEFT);
			return true;
		}
	}

	return false;
}

void ui::add_language_picker(
	Container& container,
	const render::Font& font,
	i18n::Lang active,
	std::function<void(i18n::Lang)> on_pick
) {
	for (int i = 0; i < i18n::LANGUAGE_COUNT; ++i) {
		const auto lang = static_cast<i18n::Lang>(i);

		if (i > 0)
			ui::set_next_same_line(container);

		// each chip keeps the real flag proportion
		const int width = static_cast<int>(std::lround(static_cast<float>(FLAG_H) * flags::ASPECT[i]));

		Element element(
			std::string("language flag ") + i18n::code(lang),
			ElementType::LANGUAGE_FLAG,
			gfx::Rect(container.current_position, gfx::Size(width + FLAG_GAP, FLAG_H + 12)),
			LanguageFlagElementData{
				.lang = lang,
				.font = &font,
				.active = lang == active,
				.on_pick = on_pick,
			},
			render_language_flag,
			update_language_flag
		);

		add_element(
			container,
			std::move(element),
			container.element_gap,
			{
				{ hasher("main"), AnimationState(20.f) },
				{ hasher("hover"), AnimationState(80.f) },
			}
		);
	}
}
