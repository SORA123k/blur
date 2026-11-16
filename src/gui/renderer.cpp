#include "common/i18n.h"
#include "renderer.h"

#include "common/config_app.h"
#include "common/rendering.h"

#include "gui/ui/keys.h"
#include "sdl.h"
#include "gui.h"

#include "ui/ui.h"
#include "render/render.h"
#include "os/desktop_notification.h"

#include "components/main.h"
#include "components/notifications.h"
#include "components/configs/configs.h"

#define DEBUG_RENDER 0
#define DEBUG_UI_DIAG 0

namespace {
	// Soft radial bloom. imgui has no radial gradient, so a handful of stacked
	// translucent circles accumulate into a smooth falloff.
	void draw_radial_glow(const gfx::Point& center, float radius, const gfx::Color& colour, float strength) {
		constexpr int steps = 18;
		const float alpha = (strength / static_cast<float>(steps)) * 2.4f;

		for (int i = steps; i >= 1; --i) {
			const float t = static_cast<float>(i) / static_cast<float>(steps);
			render::circle_filled(center, radius * t, theme::with_alpha(colour, alpha), 1.f, 64);
		}
	}
}

bool gui::renderer::redraw_window(bool rendered_last, bool want_to_render) {
	keys::on_frame_start();
	ui::on_frame_start();
	sdl::on_frame_start();

	render::update_window_size(sdl::window);

	auto now = std::chrono::steady_clock::now();
	static auto last_frame_time = now;

#if DEBUG_RENDER
	float fps = -1.f;
#endif
	float delta_time =
		sdl::vsync_frame_time_ms / 1000.f; // TODO: TEMP: using vsync fps as deltatime to avoid jumping issues. let's
	                                       // see if it causes any issues. it should be fine...

	if (!rendered_last) {
		// delta_time = sdl::DEFAULT_DELTA_TIME;
	}
	else {
		float time_since_last_frame = std::chrono::duration<float>(now - last_frame_time).count();

#if DEBUG_RENDER
		fps = 1.f / time_since_last_frame;

// float current_fps = 1.f / time_since_last_frame;
// if (fps == -1.f)
// 	fps = current_fps;
// fps = (fps * FPS_SMOOTHING) + (current_fps * (1.0f - FPS_SMOOTHING));
#endif

		// delta_time = std::min(time_since_last_frame, 1.f / sdl::MIN_FPS);
	}

	// #if DEBUG_RENDER
	// 	u::log("delta time: {}, rendered last: {}", delta_time, rendered_last);
	// #endif

	last_frame_time = now;

	const gfx::Rect rect(gfx::Point(0, 0), render::window_size);

	static float bg_drop_overlay_percent = 0.f;
	static float bg_last_percent = bg_drop_overlay_percent;
	bg_drop_overlay_percent = u::lerp(bg_drop_overlay_percent, gui::dragging ? 1.f : 0.f, 25.f * delta_time);
	want_to_render |= bg_drop_overlay_percent != bg_last_percent;
	bg_last_percent = bg_drop_overlay_percent;

	gfx::Rect nav_container_rect = rect;
	nav_container_rect.h = 70;
	nav_container_rect.y = rect.y2() - nav_container_rect.h;

	ui::reset_container(
		nav_container, sdl::window, nav_container_rect, fonts::dejavu.height(), {}, 1.2f, gfx::Color(16, 16, 24, 240)
	);

	int nav_cutoff = rect.y2() - nav_container_rect.y;
	int bottom_pad = std::max(PAD_Y, nav_cutoff);

	const static int main_pad_x = std::min(100, render::window_size.w / 10); // bit of magic never hurt anyone
	ui::reset_container(
		main_container, sdl::window, rect, 13, ui::Padding{ PAD_Y, main_pad_x, bottom_pad, main_pad_x }
	);

	const int config_page_container_gap = PAD_X / 2;

	// Glass panels belong to the config screen; the main screen is a free
	// floating layout and must not be covered by an always-rendered container.
	const std::optional<gfx::Color> panel_fill =
		screen == Screens::CONFIG ? std::optional<gfx::Color>(theme::white(0.035f)) : std::nullopt;

	gfx::Rect config_container_rect = rect;

	if (components::configs::loaded_config) {
		config_container_rect.w = 200 + PAD_X * 2;
	}

	ui::reset_container(
		config_container,
		sdl::window,
		config_container_rect,
		9,
		ui::Padding{ PAD_Y, PAD_X, bottom_pad, PAD_X },
		1.2f,
		panel_fill
	);

	gfx::Rect config_preview_container_rect = rect;
	config_preview_container_rect.x = config_container_rect.x2() + config_page_container_gap;
	config_preview_container_rect.w -= config_container_rect.w + config_page_container_gap;

	gfx::Rect config_preview_header_container_rect = config_preview_container_rect;
	config_preview_header_container_rect.h = 80;

	ui::reset_container(
		config_preview_header_container,
		sdl::window,
		config_preview_header_container_rect,
		fonts::dejavu.height(),
		ui::Padding{ PAD_Y, PAD_X }
	);

	gfx::Rect config_preview_content_container_rect = config_preview_container_rect;
	config_preview_content_container_rect.y = config_preview_header_container_rect.y2();
	config_preview_content_container_rect.h -= config_preview_header_container_rect.h;

	ui::reset_container(
		config_preview_content_container,
		sdl::window,
		config_preview_content_container_rect,
		fonts::dejavu.height(),
		ui::Padding{ 0, PAD_X, bottom_pad, PAD_X }
	);

	ui::reset_container(
		option_information_container,
		sdl::window,
		config_preview_container_rect,
		9,
		ui::Padding{ PAD_Y, PAD_X, bottom_pad, PAD_X },
		1.2f,
		panel_fill
	);

	gfx::Rect notification_container_rect = rect;
	notification_container_rect.w = 230;
	notification_container_rect.x =
		rect.x2() - notification_container_rect.w - components::notifications::NOTIFICATIONS_PAD_X;
	notification_container_rect.h = 300;
	notification_container_rect.y = components::notifications::NOTIFICATIONS_PAD_Y;

	ui::reset_container(notification_container, sdl::window, notification_container_rect, 6, {});

	switch (screen) {
		case Screens::MAIN: {
			if (components::configs::should_load_config) {
				components::configs::loaded_config = false;
			}

			components::main::home_screen(main_container, delta_time);

			if (initialisation_res) {
				auto current_render = rendering.get_current_render();
				if (current_render) {
					ui::add_button(
						(*current_render)->is_paused() ? "resume render button" : "pause render button",
						nav_container,
						(*current_render)->is_paused() ? i18n::t("继续") : i18n::t("暂停"),
						fonts::dejavu,
						[] {
							auto current_render = rendering.get_current_render();
							if ((*current_render)->is_paused())
								(*current_render)->resume();
							else
								(*current_render)->pause();
						}
					);

					ui::set_next_same_line(nav_container);
					ui::add_button("stop render button", nav_container, i18n::t("取消"), fonts::dejavu, [] {
						auto current_render = rendering.get_current_render();
						if (current_render)
							(*current_render)->stop();
					});

					ui::set_next_same_line(nav_container);
					components::main::open_files_button(nav_container, i18n::t("添加文件"));
				}

				ui::set_next_same_line(nav_container);
				ui::add_button("config button", nav_container, i18n::t("配置"), fonts::dejavu, [] {
					screen = Screens::CONFIG;
				});
			}

			ui::center_elements_in_container(main_container);

			break;
		}
		case Screens::CONFIG: {
			components::configs::should_load_config = true;

			ui::set_next_same_line(nav_container);
			ui::add_button("back button", nav_container, i18n::t("返回"), fonts::dejavu, [] {
				screen = Screens::MAIN;
			});

			components::configs::screen(
				config_container,
				nav_container,
				config_preview_header_container,
				config_preview_content_container,
				option_information_container,
				delta_time
			);

			ui::center_elements_in_container(config_preview_header_container);
			ui::center_elements_in_container(config_preview_content_container);
			ui::center_elements_in_container(option_information_container, true, false);

			break;
		}
	}

	components::notifications::render(notification_container);

	ui::center_elements_in_container(nav_container);

	want_to_render |= ui::update_container_frame(notification_container, delta_time);
	want_to_render |= ui::update_container_frame(nav_container, delta_time);

	want_to_render |= ui::update_container_frame(main_container, delta_time);
	want_to_render |= ui::update_container_frame(config_container, delta_time);
	want_to_render |= ui::update_container_frame(config_preview_header_container, delta_time);
	want_to_render |= ui::update_container_frame(config_preview_content_container, delta_time);
	want_to_render |= ui::update_container_frame(option_information_container, delta_time);
	ui::on_update_frame_end();

	if (!want_to_render)
		// note: DONT RENDER ANYTHING ABOVE HERE!!! todo: render queue?
		return false;

	render::imgui.begin(sdl::window);
	{
		// backdrop: near-black base with a violet glow bleeding in from the top
		// left and a cyan one from the bottom right (the clickgui-ui "scene").
		render::rect_filled(rect, theme::backdrop);
		draw_radial_glow(
			gfx::Point(rect.x - (rect.w / 8), rect.y - (rect.h / 6)), rect.w * 0.62f, theme::accent, 0.22f
		);
		draw_radial_glow(
			gfx::Point(rect.x2() + (rect.w / 10), rect.y2() + (rect.h / 5)), rect.w * 0.55f, theme::accent2, 0.13f
		);

#if DEBUG_RENDER
		{
			// debug
			static const int debug_box_size = 30;
			static float x = rect.x2() - debug_box_size;
			static float y = 100.f;
			static bool right = false;
			static bool down = true;
			x += 1.f * (right ? 1 : -1);
			y += 1.f * (down ? 1 : -1);

			render::rect_filled(gfx::Rect(x, y, debug_box_size, debug_box_size), gfx::Color(255, 0, 0, 50));

			if (right) {
				if (x + debug_box_size > rect.x2())
					right = false;
			}
			else {
				if (x < 0)
					right = true;
			}

			if (down) {
				if (y + debug_box_size > rect.y2())
					down = false;
			}
			else {
				if (y < 0)
					down = true;
			}
		}
#endif

		// front -> back
		ui::render_container(main_container);
		ui::render_container(config_container);
		ui::render_container(config_preview_content_container);
		ui::render_container(config_preview_header_container);
		ui::render_container(option_information_container);
		ui::render_container(nav_container);
		ui::render_container(notification_container);

		// file drop overlay
		if (bg_drop_overlay_percent > 0.f)
			render::rect_filled(rect, gfx::Color::white(bg_drop_overlay_percent * 30.f));

#if DEBUG_UI_DIAG
		render::text(
			gfx::Point(10, 10),
			gfx::Color(0, 255, 128, 255),
			std::format(
				"win {}x{} scale {:.2f} density {:.2f}", render::window_size.w, render::window_size.h, render::ui_scale,
				render::pixel_density
			),
			fonts::dejavu
		);
#endif

#if DEBUG_RENDER
		if (fps != -1.f) {
			gfx::Point fps_pos(rect.x2() - PAD_X, rect.y + PAD_Y);
			render::text(
				fps_pos, gfx::Color(0, 255, 0, 255), std::format("{:.0f} fps", fps), fonts::dejavu, FONT_RIGHT_ALIGN
			);
		}
#endif
	}
	render::imgui.end(sdl::window);

	ui::on_frame_end();

	return true;
}

void gui::renderer::on_render_finished(Render* render, const tl::expected<RenderResult, std::string>& result) {
	if (!result) {
		gui::components::notifications::add(
			i18n::tf("渲染 '{}' 失败，点击可复制错误信息", render->get_video_name()),
			ui::NotificationType::NOTIF_ERROR,
			[result](const std::string& id) {
				SDL_SetClipboardText(result.error().c_str());

				gui::components::notifications::close(id);

				gui::components::notifications::add(
					i18n::t("已复制错误信息到剪贴板"),
					ui::NotificationType::INFO,
					{},
					std::chrono::duration<float>(2.f)
				);
			},
			std::nullopt
		);

		auto app_config = config_app::get_app_config();
		if (app_config.render_failure_notifications) {
			desktop_notification::show(i18n::t("模糊渲染失败"), u::truncate_with_ellipsis(result.error(), 100));
		}
		return;
	}

	if (result->stopped) {
		gui::components::notifications::add(
			i18n::tf("渲染 '{}' 已停止", render->get_video_name()), ui::NotificationType::INFO
		);
		return;
	}

	auto output_path = render->get_output_video_path();

	gui::components::notifications::add(
		i18n::tf("渲染 '{}' 已完成", render->get_video_name()),
		ui::NotificationType::SUCCESS,
		[output_path](const std::string& id) {
			std::string file_url = std::format("file://{}", output_path);
			if (!SDL_OpenURL(file_url.c_str())) {
				u::log_error(i18n::t("打开输出文件夹失败: {}"), SDL_GetError());
			}
		}
	);

	auto app_config = config_app::get_app_config();
	if (app_config.render_success_notifications) {
		desktop_notification::show(i18n::t("模糊渲染完成"), i18n::t("渲染已成功完成"));
	}
}
