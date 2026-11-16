#include "common/i18n.h"
#include "sdl.h"

#include <cmath>

#include "common/config_app.h"
#include "render/render.h"
#include "os/desktop_notification.h"

namespace {
	std::unordered_map<SDL_SystemCursor, SDL_Cursor*> cursor_cache;
	SDL_SystemCursor current_cursor = SDL_SYSTEM_CURSOR_DEFAULT;
	bool set_cursor_this_frame = false;
}

tl::expected<void, std::string> sdl::initialise() {
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"); // idk mpv example says to
	// SDL_SetHint(SDL_HINT_MAC_SCROLL_MOMENTUM, "1");
	SDL_SetHint(
		SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1"
	); // allows the screen to auto sleep. WHY IS THIS DISABLED BY DEFAULT?

	if (!SDL_Init(SDL_INIT_VIDEO))
		return tl::unexpected(i18n::t("SDL 初始化失败"));

	// Initialise notification system
	auto config = config_app::get_app_config();

	// Language and zoom come from the app config, before anything is drawn.
	i18n::set(i18n::from_code(config.language));

	if (config.render_success_notifications || config.render_failure_notifications) {
		desktop_notification::initialise(APPLICATION_NAME);
	}

	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100 (WebGL 1.0)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
	// GL ES 3.0 + GLSL 300 es (WebGL 2.0)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
	// GL 3.2 Core + GLSL 150
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
	// GL 3.0 + GLSL 130
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

	// Create window with graphics context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	// create sdl window
	//
	// 591x381 / 450x250 are *design* units; they are multiplied by the display
	// scale so the interface keeps the size the author intended on high dpi
	// panels instead of shrinking into the corner.
	const float ui_scale = config.ui_scale > 0.f ? config.ui_scale : render::compute_ui_scale(nullptr);
	render::ui_scale = ui_scale;

	window =
		SDL_CreateWindow("Blur", 591, 381, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

	if (!window)
		return tl::unexpected(i18n::t("创建 SDL 窗口失败"));

	{
		int window_w = 0;
		int window_h = 0;
		int pixel_w = 0;
		int pixel_h = 0;
		SDL_GetWindowSize(window, &window_w, &window_h);
		SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);

		const float density = (window_w > 0) ? static_cast<float>(pixel_w) / static_cast<float>(window_w) : 1.f;
		const float to_window_units = ui_scale / (density > 0.f ? density : 1.f);

		SDL_SetWindowSize(
			window,
			static_cast<int>(std::lround(591.f * to_window_units)),
			static_cast<int>(std::lround(381.f * to_window_units))
		);
		SDL_SetWindowMinimumSize(
			window,
			static_cast<int>(std::lround(450.f * to_window_units)),
			static_cast<int>(std::lround(250.f * to_window_units))
		);
	}

	SDL_AddEventWatch(event_watcher, window);

	// enable drag and drop
	SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);

	// create opengl context
	gl_context = SDL_GL_CreateContext(window);
	if (!gl_context)
		return tl::unexpected(i18n::t("创建 SDL GL 上下文失败"));

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) { // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
		                                                          // ^ c lib
		return tl::unexpected(i18n::t("初始化 GLAD 失败"));
	}

	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1); // enable vsync
	SDL_ShowWindow(window);

	if (!render::init(window, gl_context))
		return tl::unexpected(i18n::t("初始化渲染器失败"));

	return {};
}

void sdl::cleanup() {
	for (auto& [_, cursor] : cursor_cache) {
		SDL_DestroyCursor(cursor);
	}
	cursor_cache.clear();

	render::destroy();

	if (sdl::gl_context) {
		SDL_GL_DestroyContext(sdl::gl_context);
		sdl::gl_context = nullptr;
	}

	if (sdl::window) {
		SDL_RemoveEventWatch(event_watcher, window);

		SDL_DestroyWindow(sdl::window);
		sdl::window = nullptr;
	}

	desktop_notification::cleanup();

	SDL_Quit();
}

bool sdl::event_watcher(void* data, SDL_Event* event) {
	switch (event->type) {
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
			SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
			if (win == static_cast<SDL_Window*>(data)) {
				// gui::renderer::redraw_window(true); // TODO: squishy jelly

				render::imgui.begin(sdl::window);
				render::imgui.end(sdl::window);
			}
			break;
		}

		default:
			break;
	}

	return true;
}

void sdl::on_frame_start() {
	set_cursor_this_frame = false;
}

void sdl::set_cursor(SDL_SystemCursor cursor) {
	if (current_cursor != cursor) {
		current_cursor = cursor;

		// Check if cursor is already cached
		if (!cursor_cache.contains(cursor)) {
			cursor_cache[cursor] = SDL_CreateSystemCursor(cursor);
		}

		SDL_SetCursor(cursor_cache[cursor]);
	}

	set_cursor_this_frame = true;
}

void sdl::update_vsync() {
	static int last_display_index = -1;

	int display_index = SDL_GetDisplayForWindow(window);
	if (display_index != last_display_index) {
		last_display_index = display_index;

		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display_index);
		if (mode) {
			double rate = mode->refresh_rate;
			vsync_frame_time_ms = float(1.f / (rate + VSYNC_EXTRA_FPS)) * 1000.f;
			u::log("switched screen, updated vsync_frame_time. refresh rate: {:.2f} hz", rate);
		}
	}
}
