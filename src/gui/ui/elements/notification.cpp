#include "../ui.h"
#include "../../render/render.h"
#include "../theme.h"
#include "../keys.h"

const gfx::Size NOTIFICATION_TEXT_PADDING = { 16, 10 };
static const float NOTIFICATION_ROUNDING = theme::radius_card - 2.f;
static const int CLOSE_BUTTON_SIZE = 16;
static const int CLOSE_BUTTON_PADDING = 8;
static const int ACCENT_BAR_WIDTH = 3;

namespace {
	gfx::Rect get_close_button_rect(const gfx::Rect& notification_rect) {
		return {
			notification_rect.x2() - CLOSE_BUTTON_SIZE - CLOSE_BUTTON_PADDING,
			notification_rect.y + CLOSE_BUTTON_PADDING,
			CLOSE_BUTTON_SIZE,
			CLOSE_BUTTON_SIZE,
		};
	}
}

void ui::render_notification(const Container& container, const AnimatedElement& element) {
	const auto& notification_data = std::get<NotificationElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float close_hover_anim = element.animations.at(hasher("close_hover")).current;

	gfx::Color accent_color = theme::accent;
	gfx::Color text_color = theme::text;

	switch (notification_data.type) {
		case NotificationType::SUCCESS: {
			accent_color = gfx::Color(52, 211, 153, 255); // emerald
			break;
		}
		case NotificationType::NOTIF_ERROR: {
			accent_color = gfx::Color(244, 63, 94, 255); // rose
			break;
		}
		case NotificationType::INFO:
		default: {
			accent_color = theme::accent;
			break;
		}
	};

	anim *= 1.f - (hover_anim * 0.2);

	accent_color = accent_color.adjust_alpha(anim);
	text_color = text_color.adjust_alpha(anim);

	const gfx::Rect& rect = element.element->rect;

	// glass card
	render::rounded_rect_filled(rect, theme::with_alpha(theme::panel_raised, anim), NOTIFICATION_ROUNDING);
	render::rounded_rect_filled(
		gfx::Rect(rect.x, rect.y, rect.w, static_cast<int>(rect.h * 0.5f)),
		theme::white(0.03f * anim),
		NOTIFICATION_ROUNDING,
		ROUNDING_TOP
	);
	render::rounded_rect_stroke(rect, theme::white(0.13f).adjust_alpha(anim), NOTIFICATION_ROUNDING);

	// type accent bar on the left
	render::rounded_rect_filled(
		gfx::Rect(rect.x, rect.y + 6, ACCENT_BAR_WIDTH, rect.h - 12),
		accent_color,
		ACCENT_BAR_WIDTH / 2.f
	);

	if (notification_data.on_close) {
		gfx::Rect close_button_rect = get_close_button_rect(element.element->rect);

		gfx::Color close_button_color = text_color;
		close_button_color = close_button_color.adjust_alpha(0.7f + (close_hover_anim * 0.3f));

		// close x
		float x_padding = 4;
		render::line(
			gfx::Point(close_button_rect.x + x_padding, close_button_rect.y + x_padding),
			gfx::Point(close_button_rect.x2() - x_padding, close_button_rect.y2() - x_padding),
			close_button_color
		);
		render::line(
			gfx::Point(close_button_rect.x + x_padding, close_button_rect.y2() - x_padding),
			gfx::Point(close_button_rect.x2() - x_padding, close_button_rect.y + x_padding),
			close_button_color
		);
	}

	gfx::Point text_pos = element.element->rect.origin();
	text_pos.x += NOTIFICATION_TEXT_PADDING.w + ACCENT_BAR_WIDTH;
	text_pos.y += NOTIFICATION_TEXT_PADDING.h;

	for (const auto& line : notification_data.lines) {
		render::text(text_pos, text_color, line, *notification_data.font); // TODO: align properly
		text_pos.y += notification_data.line_height;
	}
}

bool ui::update_notification(const Container& container, AnimatedElement& element) {
	const auto& notification_data = std::get<NotificationElementData>(element.element->data);

	if (notification_data.on_close) {
		gfx::Rect close_button_rect = get_close_button_rect(element.element->rect);

		bool close_hovered = close_button_rect.contains(keys::mouse_pos) && set_hovered_element(element);
		auto& close_anim = element.animations.at(hasher("close_hover"));
		close_anim.set_goal(close_hovered ? 1.f : 0.f);

		if (close_hovered) {
			set_cursor(SDL_SYSTEM_CURSOR_POINTER);
			if (keys::is_mouse_down()) {
				(*notification_data.on_close)(element.element->id);
				keys::on_mouse_press_handled(SDL_BUTTON_LEFT);
				return true;
			}
		}
	}

	if (notification_data.on_click) {
		bool hovered =
			element.element->rect.contains(keys::mouse_pos) &&
			(!notification_data.on_close || !get_close_button_rect(element.element->rect).contains(keys::mouse_pos)) &&
			set_hovered_element(element);

		auto& anim = element.animations.at(hasher("hover"));
		anim.set_goal(hovered ? 1.f : 0.f);

		if (hovered) {
			set_cursor(SDL_SYSTEM_CURSOR_POINTER);

			if (keys::is_mouse_down()) {
				(*notification_data.on_click)(element.element->id);
				keys::on_mouse_press_handled(SDL_BUTTON_LEFT);

				return true;
			}
		}
	}

	return false;
}

ui::AnimatedElement* ui::add_notification(
	const std::string& id,
	Container& container,
	const std::string& text,
	ui::NotificationType type,
	const render::Font& font,
	std::optional<std::function<void(const std::string& id)>> on_click,
	std::optional<std::function<void(const std::string& id)>> on_close
) {
	gfx::Size notification_size = { 230, 100 }; // height is a maximum to start with

	const int line_height = font.height() + 5;

	gfx::Size text_size = notification_size - (NOTIFICATION_TEXT_PADDING * 2);
	if (on_close) {
		text_size.w -= (CLOSE_BUTTON_SIZE + CLOSE_BUTTON_PADDING * 2);
	}

	std::vector<std::string> lines = render::wrap_text(text, text_size, font, line_height);

	notification_size.h = lines.size() * line_height;
	notification_size.h += NOTIFICATION_TEXT_PADDING.h * 2;

	// now that we've calculated the notification size, add text padding

	Element element(
		id,
		ElementType::NOTIFICATION,
		gfx::Rect(container.current_position, notification_size),
		NotificationElementData{
			.lines = lines,
			.type = type,
			.font = &font,
			.line_height = line_height,
			.on_click = std::move(on_click),
			.on_close = std::move(on_close),
		},
		render_notification,
		update_notification
	);

	return add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), AnimationState(15.f) },
			{ hasher("hover"), AnimationState(80.f) },
			{ hasher("close_hover"), AnimationState(80.f) },
		}
	);
}
