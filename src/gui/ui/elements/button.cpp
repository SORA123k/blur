#include "../ui.h"
#include "../../render/render.h"
#include "../theme.h"

#include "../keys.h"

const gfx::Size BUTTON_PADDING = { 14, 9 };
constexpr float BUTTON_ROUNDING = theme::radius_ctrl;

void ui::render_button(const Container& container, const AnimatedElement& element) {
	const auto& button_data = std::get<ButtonElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;

	const gfx::Rect& rect = element.element->rect;

	const bool pressed = rect.contains(keys::mouse_pos) && keys::is_mouse_dragging(SDL_BUTTON_LEFT);
	const float press = pressed ? 1.f : 0.f;

	// violet halo on hover: the kit lifts its cards instead of only recolouring them
	if (hover_anim > 0.01f) {
		render::rounded_rect_filled(
			gfx::Rect(rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4),
			theme::accent_alpha(0.10f * hover_anim * anim),
			BUTTON_ROUNDING + 2.f
		);
	}

	// translucent card fill, violet wash while pressed; a primary button is
	// filled with the accent colour instead
	gfx::Color fill = theme::white(0.045f + (0.040f * hover_anim));
	gfx::Color border = theme::stroke(hover_anim);

	if (button_data.primary) {
		fill = theme::mix(theme::accent, theme::accent2, 0.25f * hover_anim);
		border = theme::mix(theme::accent2, theme::white(0.35f), 0.5f * hover_anim);
	}
	else {
		fill = theme::mix(fill, theme::accent_alpha(0.32f), press);
		border = theme::mix(border, theme::accent_alpha(0.70f), press);
	}

	render::rounded_rect_filled(rect, fill.adjust_alpha(anim), BUTTON_ROUNDING);
	render::rounded_rect_stroke(rect, border.adjust_alpha(anim), BUTTON_ROUNDING);

	render::text(
		rect.center(),
		theme::text.adjust_alpha(anim),
		button_data.text,
		*button_data.font,
		FONT_CENTERED_X | FONT_CENTERED_Y
	);
}

bool ui::update_button(const Container& container, AnimatedElement& element) {
	const auto& button_data = std::get<ButtonElementData>(element.element->data);

	auto& anim = element.animations.at(hasher("hover"));

	bool hovered = element.element->rect.contains(keys::mouse_pos) && set_hovered_element(element);
	anim.set_goal(hovered ? 1.f : 0.f);

	if (hovered) {
		set_cursor(SDL_SYSTEM_CURSOR_POINTER);

		if (button_data.on_press) {
			if (keys::is_mouse_down()) {
				(*button_data.on_press)();
				keys::on_mouse_press_handled(SDL_BUTTON_LEFT);

				return true;
			}
		}
	}

	return false;
}

ui::AnimatedElement* ui::add_button(
	const std::string& id,
	Container& container,
	const std::string& text,
	const render::Font& font,
	std::optional<std::function<void()>> on_press,
	bool primary
) {
	gfx::Size text_size = font.calc_size(text);

	gfx::Rect rect(
		container.current_position,
		gfx::Size(text_size.w + (BUTTON_PADDING.w * 2), font.height() + (BUTTON_PADDING.h * 2))
	);

	Element element(
		id,
		ElementType::BUTTON,
		rect,
		ButtonElementData{
			.text = text,
			.font = &font,
			.on_press = std::move(on_press),
			.primary = primary,
		},
		render_button,
		update_button
	);

	return add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), AnimationState(25.f) },
			{ hasher("hover"), AnimationState(80.f) },
		}
	);
}
