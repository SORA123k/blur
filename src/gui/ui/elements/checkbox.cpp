#include "../ui.h"
#include "../../render/render.h"
#include "../theme.h"
#include "../keys.h"

// Rendered as a pill switch (the clickgui-ui kit has no checkboxes): a rounded
// track that fills with the accent gradient plus a glow when on, and a white
// knob that slides over.
constexpr float TRACK_W = 34.f;
constexpr float TRACK_H = 18.f;
constexpr float KNOB_RADIUS = 7.f;
constexpr float LABEL_GAP = 10.f;

void ui::render_checkbox(const Container& container, const AnimatedElement& element) {
	const auto& checkbox_data = std::get<CheckboxElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float check_anim = element.animations.at(hasher("check")).current;

	const gfx::Rect& rect = element.element->rect;

	gfx::Rect track_rect = rect;
	track_rect.w = static_cast<int>(TRACK_W);
	track_rect.h = static_cast<int>(TRACK_H);
	track_rect.y = rect.y + ((rect.h - track_rect.h) / 2);

	const float rounding = TRACK_H / 2.f;

	// glow while enabled
	if (check_anim > 0.01f) {
		render::rounded_rect_filled(
			gfx::Rect(track_rect.x - 2, track_rect.y - 2, track_rect.w + 4, track_rect.h + 4),
			theme::accent_alpha(0.22f * check_anim * anim),
			rounding + 2.f
		);
	}

	// track: translucent when off, accent when on
	gfx::Color track = theme::white(0.10f + (0.03f * hover_anim));
	track = theme::mix(track, theme::accent, check_anim);
	render::rounded_rect_filled(track_rect, track.adjust_alpha(anim), rounding);

	// a hint of the second accent on the far side of the track
	if (check_anim > 0.01f) {
		render::rounded_rect_filled(
			gfx::Rect(track_rect.x + (track_rect.w / 2), track_rect.y, track_rect.w / 2, track_rect.h),
			theme::accent2.adjust_alpha(0.22f * check_anim * anim),
			rounding,
			ROUNDING_RIGHT
		);
	}

	render::rounded_rect_stroke(
		track_rect,
		theme::mix(theme::stroke(hover_anim), theme::accent_alpha(0.85f), check_anim).adjust_alpha(anim),
		rounding
	);

	// knob
	const float knob_travel = TRACK_W - (KNOB_RADIUS * 2.f) - 4.f;
	const float knob_x = static_cast<float>(track_rect.x) + KNOB_RADIUS + 2.f + (knob_travel * check_anim);
	const float knob_y = static_cast<float>(track_rect.y) + (TRACK_H / 2.f);

	render::circle_filled({ static_cast<int>(knob_x), static_cast<int>(knob_y + 1.f) }, KNOB_RADIUS, theme::black(0.35f * anim));
	render::circle_filled(
		{ static_cast<int>(knob_x), static_cast<int>(knob_y) },
		KNOB_RADIUS,
		theme::white(1.f).adjust_alpha(anim)
	);

	// label
	gfx::Point text_pos = rect.origin();
	text_pos.x += static_cast<int>(TRACK_W + LABEL_GAP);
	text_pos.y = rect.center().y;

	render::text(
		text_pos,
		theme::mix(theme::text_muted, theme::text, check_anim).adjust_alpha(anim),
		checkbox_data.label,
		*checkbox_data.font,
		FONT_CENTERED_Y
	);
}

bool ui::update_checkbox(const Container& container, AnimatedElement& element) {
	const auto& checkbox_data = std::get<CheckboxElementData>(element.element->data);

	auto& hover_anim = element.animations.at(hasher("hover"));
	auto& check_anim = element.animations.at(hasher("check"));

	bool hovered = element.element->rect.contains(keys::mouse_pos) && set_hovered_element(element);
	hover_anim.set_goal(hovered ? 1.f : 0.f);
	check_anim.set_goal(*checkbox_data.checked ? 1.f : 0.f);

	if (hovered) {
		set_cursor(SDL_SYSTEM_CURSOR_POINTER);

		if (keys::is_mouse_down()) {
			*checkbox_data.checked = !(*checkbox_data.checked);
			check_anim.set_goal(*checkbox_data.checked ? 1.f : 0.f);

			keys::on_mouse_press_handled(SDL_BUTTON_LEFT);

			if (checkbox_data.on_change)
				(*checkbox_data.on_change)(*checkbox_data.checked);

			return true;
		}
	}

	return false;
}

ui::AnimatedElement* ui::add_checkbox(
	const std::string& id,
	Container& container,
	const std::string& label,
	bool& checked,
	const render::Font& font,
	std::optional<std::function<void(bool)>> on_change
) {
	gfx::Size total_size(200, std::max(static_cast<int>(TRACK_H), font.height()));

	Element element(
		id,
		ElementType::CHECKBOX,
		gfx::Rect(container.current_position, total_size),
		CheckboxElementData{
			.label = label,
			.checked = &checked,
			.font = &font,
			.on_change = std::move(on_change),
		},
		render_checkbox,
		update_checkbox
	);

	return add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), AnimationState(25.f) },
			{ hasher("hover"), AnimationState(80.f) },
			{ hasher("check"), AnimationState(12.f, checked ? 1.f : 0.f) },
		}
	);
}
