#include "../ui.h"
#include "../../render/render.h"
#include "../theme.h"

void ui::render_bar(const Container& container, const AnimatedElement& element) {
	const int text_gap = 8;

	const auto& bar_data = std::get<BarElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;

	gfx::Size text_size(0, 0);

	if (bar_data.bar_text && bar_data.text_color && bar_data.font) {
		gfx::Point text_pos = element.element->rect.origin();
		text_pos.y += element.element->rect.h / 2;

		text_size = (*bar_data.font)->calc_size(*bar_data.bar_text);

		text_pos.x = element.element->rect.x2();

		render::text(
			text_pos,
			theme::text_muted.adjust_alpha(anim),
			*bar_data.bar_text,
			**bar_data.font,
			FONT_RIGHT_ALIGN | FONT_CENTERED_Y
		);
	}

	gfx::Rect bar_rect = element.element->rect;
	bar_rect.w -= text_size.w + text_gap;
	bar_rect.y = bar_rect.center().y - 3;
	bar_rect.h = 6;

	const float rounding = bar_rect.h / 2.f;

	render::rounded_rect_filled(bar_rect, theme::white(0.07f).adjust_alpha(anim), rounding);

	if (bar_data.percent_fill > 0.f) {
		gfx::Rect fill_rect = bar_rect;
		fill_rect.w = static_cast<int>(static_cast<float>(bar_rect.w) * bar_data.percent_fill);
		if (fill_rect.w < bar_rect.h)
			fill_rect.w = bar_rect.h; // keep the pill shape at very low values

		// glow under the filled part
		render::rounded_rect_filled(
			gfx::Rect(fill_rect.x - 1, fill_rect.y - 2, fill_rect.w + 2, fill_rect.h + 4),
			theme::accent_alpha(0.18f * anim),
			rounding + 2.f
		);

		render::rounded_rect_filled(fill_rect, theme::accent.adjust_alpha(anim), rounding);

		// cyan leading edge, the kit's accent-2 gradient without needing a gradient
		gfx::Rect tip = fill_rect;
		tip.x = tip.x2() - static_cast<int>(std::min(fill_rect.h, fill_rect.w));
		tip.w = std::min(fill_rect.h, fill_rect.w);
		render::rounded_rect_filled(tip, theme::accent2.adjust_alpha(0.55f * anim), rounding);
	}
}

ui::AnimatedElement* ui::add_bar(
	const std::string& id,
	Container& container,
	float percent_fill,
	gfx::Color background_color,
	gfx::Color fill_color,
	int bar_width,
	std::optional<std::string> bar_text,
	std::optional<gfx::Color> text_color,
	std::optional<const render::Font*> font
) {
	Element element(
		id,
		ElementType::BAR,
		gfx::Rect(container.current_position, gfx::Size(bar_width, 6)),
		BarElementData{
			.percent_fill = percent_fill,
			.background_color = background_color,
			.fill_color = fill_color,
			.bar_text = std::move(bar_text),
			.text_color = text_color,
			.font = font,
		},
		render_bar
	);

	return add_element(container, std::move(element), container.element_gap);
}
