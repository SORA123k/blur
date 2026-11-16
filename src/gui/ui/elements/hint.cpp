#include "../ui.h"
#include "../../render/render.h"
#include "../theme.h"

constexpr float HINT_ROUNDING = theme::radius_card - 2.f;
constexpr gfx::Size HINT_PADDING = { 14, 10 };
constexpr int PARAGRAPH_SPACING = 8;
constexpr int BAR_WIDTH = 3;

namespace {
	int get_line_height(const ui::Container& container, const render::Font& font) {
		return font.height() * container.line_height;
	}

	std::vector<ui::Paragraph> wrap_paragraphs(
		const ui::Container& container, const std::vector<std::string>& input_lines, const render::Font& font
	) {
		auto size = container.get_usable_rect().size();
		std::vector<ui::Paragraph> paragraphs;

		for (size_t i = 0; i < input_lines.size(); ++i) {
			auto wrapped = render::wrap_text(input_lines[i], size, font);

			ui::Paragraph paragraph;
			paragraph.lines = wrapped;
			paragraph.is_last = (i == input_lines.size() - 1);

			paragraphs.push_back(paragraph);
		}

		return paragraphs;
	}

	int calculate_total_height(
		const ui::Container& container, const std::vector<ui::Paragraph>& paragraphs, const render::Font& font
	) {
		int height = 0;
		int line_height = get_line_height(container, font);

		for (const auto& paragraph : paragraphs) {
			// Add height for all lines in this paragraph
			height += paragraph.lines.size() * line_height;

			// Add paragraph spacing after each paragraph except the last
			if (!paragraph.is_last) {
				height += PARAGRAPH_SPACING;
			}
		}

		return height;
	}
}

void ui::render_hint(const Container& container, const AnimatedElement& element) {
	const auto& text_data = std::get<HintElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;

	gfx::Color adjusted_colour = text_data.color.adjust_alpha(anim);

	const gfx::Rect& rect = element.element->rect;

	// card background + hairline, like the kit's callouts
	render::rounded_rect_filled(rect, theme::white(0.035f).adjust_alpha(anim), HINT_ROUNDING);
	render::rounded_rect_stroke(rect, theme::white(0.07f).adjust_alpha(anim), HINT_ROUNDING);

	// accent bar down the left edge
	render::rounded_rect_filled(
		gfx::Rect(rect.x, rect.y + 8, BAR_WIDTH, rect.h - 16),
		theme::accent.adjust_alpha(anim),
		BAR_WIDTH / 2.f
	);

	gfx::Point text_pos = { rect.x + HINT_PADDING.w + BAR_WIDTH, rect.origin().y + HINT_PADDING.h };
	int line_height = get_line_height(container, *text_data.font);

	// Render each paragraph
	for (const auto& paragraph : text_data.paragraphs) {
		// Render all lines in this paragraph
		for (const auto& line : paragraph.lines) {
			render::text(text_pos, adjusted_colour, line, *text_data.font);
			text_pos.y += line_height;
		}

		// Add paragraph spacing after each paragraph except the last
		if (!paragraph.is_last) {
			text_pos.y += PARAGRAPH_SPACING;
		}
	}
}

ui::AnimatedElement* ui::add_hint(
	const std::string& id,
	Container& container,
	const std::vector<std::string>& paragraphs,
	gfx::Color color,
	const render::Font& font
) {
	auto wrapped_paragraphs = wrap_paragraphs(container, paragraphs, font);
	int text_height = calculate_total_height(container, wrapped_paragraphs, font);

	Element element(
		id,
		ElementType::HINT,
		gfx::Rect(container.current_position, gfx::Size(container.get_usable_rect().w, text_height) + HINT_PADDING * 2),
		HintElementData{
			.paragraphs = wrapped_paragraphs,
			.color = color,
			.font = &font,
		},
		render_hint
	);

	return add_element(container, std::move(element), container.element_gap);
}
