#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>

#define DEBUG_UI_DIAG 0

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>
#include <misc/freetype/imgui_freetype.h>

#include <stb_image.h>
#include "common/utils.h"

#include "../fonts/cn_sans.h"
#include "../fonts/eb_garamond.h"
#include "../fonts/icons.h"
#include "common/i18n.h"
#include "../ui/theme.h"
#include "imgui_internal.h"

namespace {
	// Glyph ranges for the Simplified Chinese UI font.
	// The array has to outlive the font atlas, hence static storage here instead of
	// a local. imconfig.h defines IMGUI_USE_WCHAR32, so ImWchar is a 32-bit type.
	const ImWchar CN_GLYPH_RANGES[] = {
		0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
		0x2000, 0x206F, // General Punctuation
		0x3000, 0x303F, // CJK Symbols and Punctuation
		0x4E00, 0x9FFF, // CJK Unified Ideographs
		0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
		0,
	};

	gfx::Color interpolate_color(const std::vector<gfx::Color>& colors, const std::vector<float>& positions, float t) {
		// Find the segment containing t
		size_t i = 0;
		for (; i < positions.size() - 1; i++) {
			if (t >= positions[i] && t <= positions[i + 1])
				break;
		}

		// If t is outside the range, clamp to the nearest color
		if (i >= positions.size() - 1) {
			if (t < positions[0])
				return colors[0];
			return colors[colors.size() - 1];
		}

		// Normalize t to the segment
		float segment_t = (t - positions[i]) / (positions[i + 1] - positions[i]);

		// Linear interpolation between colors
		const gfx::Color& c1 = colors[i];
		const gfx::Color& c2 = colors[i + 1];

		return {
			static_cast<uint8_t>(c1.r + (segment_t * (c2.r - c1.r))),
			static_cast<uint8_t>(c1.g + (segment_t * (c2.g - c1.g))),
			static_cast<uint8_t>(c1.b + (segment_t * (c2.b - c1.b))),
			static_cast<uint8_t>(c1.a + (segment_t * (c2.a - c1.a))),
		};
	}
}

bool render::ImGuiWrap::init(SDL_Window* window, const SDL_GLContext& context) {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ctx = ImGui::CreateContext();
	io = &ImGui::GetIO();
	io->IniFilename = nullptr;
	io->LogFilename = nullptr;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100 (WebGL 1.0)
	const char* glsl_version = "#version 100";
#elif defined(IMGUI_IMPL_OPENGL_ES3)
	// GL ES 3.0 + GLSL 300 es (WebGL 2.0)
	const char* glsl_version = "#version 300 es";
#elif defined(__APPLE__)
	// GL 3.2 Core + GLSL 150
	const char* glsl_version = "#version 150";
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
#endif

	ImGui_ImplSDL3_InitForOpenGL(window, context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	return true;
}

float render::compute_ui_scale(SDL_Window* window) {
	// How many physical pixels one design unit covers.
	//
	// Windows hands us the user's display scale through SDL, so this is the
	// number we want (175% -> 1.75). The upstream build ignored it and pushed
	// 13px text straight into physical pixels, which is why the interface looked
	// tiny on a 2k/4k panel; BLUR_UI_SCALE overrides it for anyone who wants a
	// different size than the system suggests.
	SDL_DisplayID display = window ? SDL_GetDisplayForWindow(window) : SDL_GetPrimaryDisplay();

	float display_scale = SDL_GetDisplayContentScale(display);
	if (!(display_scale > 0.f))
		display_scale = 1.f;

	if (const char* override_value = std::getenv("BLUR_UI_SCALE")) {
		char* end = nullptr;
		const float parsed = std::strtof(override_value, &end);
		if (end != override_value && parsed > 0.f)
			return std::clamp(parsed, 0.75f, 4.f);
	}

	// A large panel running at 100% would still get tiny text, so nudge it up a
	// little in that case (mirrors what Windows itself recommends for such panels).
	int width_px = 0;
	int height_px = 0;
	if (window) {
		SDL_GetWindowSizeInPixels(window, &width_px, &height_px);
	}
	else if (const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(display)) {
		width_px = mode->w;
		height_px = mode->h;
	}

	if (display_scale < 1.2f && width_px >= 1900)
		display_scale = 1.25f;

	return std::clamp(display_scale, 0.75f, 3.f);
}

bool render::init(SDL_Window* window, const SDL_GLContext& context) {
	if (!imgui.init(window, context))
		return false;

	render::window = window;
	ui_scale = compute_ui_scale(window);

	return load_fonts();
}

bool render::load_fonts() {
	ImGuiIO& io = ImGui::GetIO();

	// Glyphs for every language the ui can switch to at runtime, so switching
	// language never needs a font rebuild.
	static ImVector<ImWchar> ranges;

	ImFontGlyphRangesBuilder builder;
	builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
	builder.AddText(i18n::all_characters().c_str());
	builder.BuildRanges(&ranges);

	io.Fonts->Clear();
	io.Fonts->TexDesiredWidth = 2048;

	// Rasterize at the real pixel scale so the text stays crisp once the viewport
	// stretches the whole interface up by render::ui_scale.
	ImFontConfig font_cfg;
	font_cfg.RasterizerDensity = render::ui_scale;

	if (!fonts::dejavu.init_raw(CN_SANS_DATA, 13.5f, &font_cfg, ranges.Data))
		return false;

	if (!fonts::header_font.init_raw(CN_SANS_DATA, 28.f, &font_cfg, ranges.Data))
		return false;

	if (!fonts::smaller_header_font.init_raw(CN_SANS_DATA, 17.f, &font_cfg, ranges.Data))
		return false;

	if (!fonts::icons.init(ICONS_COMPRESSED_DATA, 14.f, &font_cfg, io.Fonts->GetGlyphRangesDefault()))
		return false;

	theme::scale = render::ui_scale;

	return true;
}

void render::apply_ui_scale(float scale) {
	scale = std::clamp(scale, 0.75f, 4.f);
	if (std::abs(scale - ui_scale) < 0.001f)
		return;

	ImGuiIO& io = ImGui::GetIO();

	// The atlas is about to be rebuilt, so drop the GL font texture and the font
	// pointer first -- otherwise the next frame renders with a destroyed texture
	// and the app dies on the very next click.
	ImGui_ImplOpenGL3_DestroyFontsTexture();
	io.FontDefault = nullptr;

	ui_scale = scale;
	theme::scale = scale;

	if (!load_fonts())
		return;

	io.FontDefault = fonts::dejavu.im_font();
	io.Fonts->Build();
	ImGui_ImplOpenGL3_CreateFontsTexture();

	if (render::window != nullptr)
		update_window_size(render::window);
}

void render::destroy() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void render::update_window_size(SDL_Window* window) {
	int width_px = 0;
	int height_px = 0;
	SDL_GetWindowSizeInPixels(window, &width_px, &height_px);

	int width_points = 0;
	int height_points = 0;
	SDL_GetWindowSize(window, &width_points, &height_points);
	pixel_density =
		(width_points > 0) ? static_cast<float>(width_px) / static_cast<float>(width_points) : 1.f;

	// The window is measured in design units, not pixels: everything in the ui
	// is laid out in design units and stretched by ui_scale at draw time.
	const float scale = (ui_scale > 0.f) ? ui_scale : 1.f;
	window_size.w = std::max(1, static_cast<int>(std::lround(static_cast<float>(width_px) / scale)));
	window_size.h = std::max(1, static_cast<int>(std::lround(static_cast<float>(height_px) / scale)));
}

void render::ImGuiWrap::begin(SDL_Window* window) {
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();

	// The backend just set these from the sdl window (in *pixels*), but the whole
	// interface is laid out in design units, so override them *before* NewFrame():
	// the main viewport -- and therefore the projection used to render the draw
	// data -- is built from io.DisplaySize, and the framebuffer scale blows the
	// result up to the real pixel buffer (one design unit == ui_scale pixels).
	if (io) {
		io->DisplaySize = ImVec2(static_cast<float>(window_size.w), static_cast<float>(window_size.h));
		io->DisplayFramebufferScale = ImVec2(ui_scale, ui_scale);
	}

	ImGui::NewFrame();

#if DEBUG_UI_DIAG
	{
		static bool wrote = false;
		if (!wrote) {
			wrote = true;

			int win_w = 0, win_h = 0, px_w = 0, px_h = 0;
			SDL_GetWindowSize(window, &win_w, &win_h);
			SDL_GetWindowSizeInPixels(window, &px_w, &px_h);

			const ImGuiViewport* vp = ImGui::GetMainViewport();
			std::ofstream out("ui-diag.txt");
			out << "sdl_window " << win_w << "x" << win_h << "\n";
			out << "sdl_pixels " << px_w << "x" << px_h << "\n";
			out << "pixel_density " << pixel_density << "\n";
			out << "ui_scale " << ui_scale << "\n";
			out << "design_size " << window_size.w << "x" << window_size.h << "\n";
			out << "display_size " << io->DisplaySize.x << "x" << io->DisplaySize.y << "\n";
			out << "fb_scale " << io->DisplayFramebufferScale.x << "x" << io->DisplayFramebufferScale.y << "\n";
			out << "viewport_pos " << vp->Pos.x << "," << vp->Pos.y << "\n";
			out << "viewport_size " << vp->Size.x << "x" << vp->Size.y << "\n";
		}
	}
#endif

	drawlist = ImGui::GetForegroundDrawList();

	// calculate frametime
	auto now = std::chrono::high_resolution_clock::now();
	static auto last = now;

	frametime = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - last).count() / 1000.f;

	last = now;
}

void render::ImGuiWrap::end(SDL_Window* window) { // NOLINT(readability-convert-member-functions-to-static)
	                                              // ^ yeah, but this is nicer to call
	static constexpr ImVec4 clear_colour = ImVec4(0.f, 0.f, 0.f, 1.f);

	for (auto& call : late_draw_calls) {
		call();
	}
	late_draw_calls.clear();

	ImGui::Render();

	int drawable_width = 0;
	int drawable_height = 0;
	SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);

	glViewport(0, 0, drawable_width, drawable_height);

	glClearColor(
		clear_colour.x * clear_colour.w,
		clear_colour.y * clear_colour.w,
		clear_colour.z * clear_colour.w,
		clear_colour.w
	);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(window);
}

void render::line(
	const gfx::Point& pos1, const gfx::Point& pos2, const gfx::Color& col, bool anti_aliased, float thickness
) {
	imgui.drawlist->AddLine(pos1, pos2, col.to_imgui(), thickness);
}

void render::rect_filled(const gfx::Rect& rect, const gfx::Color& col) {
	imgui.drawlist->AddRectFilled(rect.origin(), rect.max(), col.to_imgui());
}

void render::rect_stroke(const gfx::Rect& rect, const gfx::Color& col, float thickness) {
	imgui.drawlist->AddRect(rect.origin(), rect.max(), col.to_imgui(), 0.f, 0, thickness);
}

void render::rounded_rect_filled(
	const gfx::Rect& rect, const gfx::Color& col, float rounding, unsigned int rounding_flags
) {
	imgui.drawlist->AddRectFilled(rect.origin(), rect.max(), col.to_imgui(), rounding, rounding_flags);
}

void render::rounded_rect_stroke(
	const gfx::Rect& rect, const gfx::Color& col, float rounding, unsigned int rounding_flags, float thickness
) {
	imgui.drawlist->AddRect(rect.origin(), rect.max(), col.to_imgui(), rounding, rounding_flags, thickness);
}

void render::rect_filled_gradient(
	const gfx::Rect& rect,
	const std::vector<gfx::Point>& gradient_direction,
	const std::vector<gfx::Color>& colors,
	const std::vector<float>& positions
) {
	// Validate inputs
	if (colors.size() < 2 || colors.size() != positions.size() || gradient_direction.size() != 2) {
		return; // Invalid input parameters
	}

	// For custom gradient direction, we need to split the rectangle into multiple triangles
	ImVec2 origin = rect.origin();
	ImVec2 max = rect.max();

	// Calculate direction vector
	ImVec2 dir_vec(
		gradient_direction[1].x - gradient_direction[0].x, gradient_direction[1].y - gradient_direction[0].y
	);
	float dir_length = sqrtf((dir_vec.x * dir_vec.x) + (dir_vec.y * dir_vec.y));

	if (dir_length < 0.0001f) {
		return; // Invalid direction
	}

	// Normalize direction vector
	dir_vec.x /= dir_length;
	dir_vec.y /= dir_length;

	// Calculate perpendicular vector
	ImVec2 perp_vec(-dir_vec.y, dir_vec.x);

	// Create gradient mesh using triangles
	const int segments = 32; // Number of segments for smooth gradient

	for (int i = 0; i < segments; i++) {
		float t1 = static_cast<float>(i) / segments;
		float t2 = static_cast<float>(i + 1) / segments;

		// Find color at position t1 and t2
		ImU32 col1 = interpolate_color(colors, positions, t1).to_imgui();
		ImU32 col2 = interpolate_color(colors, positions, t2).to_imgui();

		// Calculate points for this segment
		ImVec2 p1 = ImVec2(
			origin.x + (dir_vec.x * t1 * (max.x - origin.x)) + (perp_vec.x * origin.y),
			origin.y + (dir_vec.y * t1 * (max.y - origin.y)) + (perp_vec.y * origin.x)
		);
		ImVec2 p2 = ImVec2(
			origin.x + (dir_vec.x * t2 * (max.x - origin.x)) + (perp_vec.x * origin.y),
			origin.y + (dir_vec.y * t2 * (max.y - origin.y)) + (perp_vec.y * origin.x)
		);
		ImVec2 p3 = ImVec2(
			max.x + (dir_vec.x * t1 * (max.x - origin.x)) + (perp_vec.x * max.y),
			max.y + (dir_vec.y * t1 * (max.y - origin.y)) + (perp_vec.y * max.x)
		);
		ImVec2 p4 = ImVec2(
			max.x + (dir_vec.x * t2 * (max.x - origin.x)) + (perp_vec.x * max.y),
			max.y + (dir_vec.y * t2 * (max.y - origin.y)) + (perp_vec.y * max.x)
		);

		// Draw triangles
		imgui.drawlist->AddTriangleFilled(p1, p2, p3, col1);
		imgui.drawlist->AddTriangleFilled(p2, p3, p4, col2);
	}
}

void render::rect_filled_gradient(
	const gfx::Rect& rect,
	GradientDirection direction,
	const std::vector<gfx::Color>& colors,
	const std::vector<float>& positions
) {
	// Validate inputs
	if (colors.size() < 2 || colors.size() != positions.size()) {
		return; // Invalid input parameters
	}

	ImVec2 origin = rect.origin();
	ImVec2 max = rect.max();

	// For predefined directions, we can use the ImGui built-in functions
	if (colors.size() == 2) {
		ImU32 col1 = colors[0].to_imgui();
		ImU32 col2 = colors[1].to_imgui();

		switch (direction) {
			case GradientDirection::GRADIENT_RIGHT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col1, col2, col2, col1);
				break;
			case GradientDirection::GRADIENT_LEFT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col2, col1, col1, col2);
				break;
			case GradientDirection::GRADIENT_DOWN:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col1, col1, col2, col2);
				break;
			case GradientDirection::GRADIENT_UP:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col2, col2, col1, col1);
				break;
			case GradientDirection::GRADIENT_DOWN_RIGHT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col1, col2, col2, col1);
				break;
			case GradientDirection::GRADIENT_UP_LEFT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col2, col1, col1, col2);
				break;
			case GradientDirection::GRADIENT_DOWN_LEFT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col2, col1, col2, col1);
				break;
			case GradientDirection::GRADIENT_UP_RIGHT:
				imgui.drawlist->AddRectFilledMultiColor(origin, max, col1, col2, col1, col2);
				break;
		}
	}
	else {
		// For more than 2 colors, we need a more complex approach
		// Convert direction to points for the general implementation
		std::vector<gfx::Point> gradient_direction;

		switch (direction) {
			case GradientDirection::GRADIENT_RIGHT:
				gradient_direction = { gfx::Point(origin.x, origin.y), gfx::Point(max.x, origin.y) };
				break;
			case GradientDirection::GRADIENT_LEFT:
				gradient_direction = { gfx::Point(max.x, origin.y), gfx::Point(origin.x, origin.y) };
				break;
			case GradientDirection::GRADIENT_DOWN:
				gradient_direction = { gfx::Point(origin.x, origin.y), gfx::Point(origin.x, max.y) };
				break;
			case GradientDirection::GRADIENT_UP:
				gradient_direction = { gfx::Point(origin.x, max.y), gfx::Point(origin.x, origin.y) };
				break;
			case GradientDirection::GRADIENT_DOWN_RIGHT:
				gradient_direction = { gfx::Point(origin.x, origin.y), gfx::Point(max.x, max.y) };
				break;
			case GradientDirection::GRADIENT_UP_LEFT:
				gradient_direction = { gfx::Point(max.x, max.y), gfx::Point(origin.x, origin.y) };
				break;
			case GradientDirection::GRADIENT_DOWN_LEFT:
				gradient_direction = { gfx::Point(max.x, origin.y), gfx::Point(origin.x, max.y) };
				break;
			case GradientDirection::GRADIENT_UP_RIGHT:
				gradient_direction = { gfx::Point(origin.x, max.y), gfx::Point(max.x, origin.y) };
				break;
		}

		// Use the general implementation
		rect_filled_gradient(rect, gradient_direction, colors, positions);
	}
}

void render::rect_gradient_multi_filled(
	const gfx::Rect& rect,
	const gfx::Color& col_top_left,
	const gfx::Color& col_top_right,
	const gfx::Color& col_bottom_left,
	const gfx::Color& col_bottom_right
) {
	render::rect_stroke(rect, col_top_left);

	imgui.drawlist->AddRectFilledMultiColor(
		rect.top_left(),
		rect.bottom_right(),
		col_top_left.to_imgui(),
		col_top_right.to_imgui(),
		col_bottom_right.to_imgui(),
		col_bottom_left.to_imgui()
	);
}

void render::quadrilateral_filled(
	const gfx::Point& bottom_left,
	const gfx::Point& bottom_right,
	const gfx::Point& top_left,
	const gfx::Point& top_right,
	const gfx::Color& col
) {
	std::array<ImVec2, 5> positions = { top_left, bottom_left, bottom_right, top_right, top_left };

	imgui.drawlist->AddConvexPolyFilled(positions.data(), static_cast<int>(positions.size()), col.to_imgui());
}

void render::quadrilateral_stroke(
	const gfx::Point& bottom_left,
	const gfx::Point& bottom_right,
	const gfx::Point& top_left,
	const gfx::Point& top_right,
	const gfx::Color& col,
	float thickness
) {
	std::array<ImVec2, 5> positions = { top_left, bottom_left, bottom_right, top_right, top_left };

	imgui.drawlist->AddPolyline(
		positions.data(), static_cast<int>(positions.size()), col.to_imgui(), ImDrawFlags_Closed, thickness
	);
}

void render::triangle_filled(
	const gfx::Point& p1,
	const gfx::Point& p2,
	const gfx::Point& p3,
	const gfx::Color& col,
	float thickness,
	bool anti_aliased
) {
	imgui.drawlist->AddTriangleFilled(p1, p2, p3, col.to_imgui());
}

void render::triangle_stroke(
	const gfx::Point& p1,
	const gfx::Point& p2,
	const gfx::Point& p3,
	const gfx::Color& col,
	float thickness,
	bool anti_aliased
) {
	imgui.drawlist->AddTriangle(p1, p2, p3, col.to_imgui(), thickness);
}

void render::circle_filled(
	const gfx::Point& pos,
	float radius,
	const gfx::Color& colour,
	float thickness,
	int parts,
	float degrees,
	float start_degree,
	bool anti_aliased
) {
	imgui.drawlist->AddCircleFilled(pos, radius, colour.to_imgui(), parts);
}

void render::circle_stroke(
	const gfx::Point& pos,
	float radius,
	const gfx::Color& colour,
	float thickness,
	int parts,
	float degrees,
	float start_degree,
	bool anti_aliased
) {
	if (degrees != 360.f || start_degree != 0.f) {
		auto min_rad = u::deg_to_rad(start_degree);
		auto max_rad = u::deg_to_rad(degrees);
		imgui.drawlist->PathArcTo(pos, radius, min_rad, max_rad, parts);
		imgui.drawlist->PathStroke(colour.to_imgui(), 0, thickness);
		return;
	}

	imgui.drawlist->AddCircle(pos, radius, colour.to_imgui(), parts, thickness);
}

void render::text(
	gfx::Point pos,
	const gfx::Color& colour,
	const std::string& text,
	const Font& font,
	unsigned int flags,
	float rotation_deg,
	int rotation_pivot_y
) {
	if (!font)
		return;

	ImGui::PushFont(font.im_font());

	int vtx_idx_begin = imgui.drawlist->_VtxCurrentIdx;

	if (flags) {
		const auto size = font.calc_size(text);

		if (flags & FONT_CENTERED_X)
			pos.x -= int(size.w * 0.5f);

		if (flags & FONT_CENTERED_Y)
			pos.y -= int(size.h * 0.5f);

		if (flags & FONT_RIGHT_ALIGN)
			pos.x -= size.w;

		if (flags & FONT_BOTTOM_ALIGN)
			pos.y -= size.h;

		if (flags & FONT_OUTLINE) {
			const gfx::Color outline_colour(0, 0, 0, colour.a * 0.8f);
			static const std::array<gfx::Point, 4> offsets = {
				gfx::Point{ -1, 0 },
				gfx::Point{ 1, 0 },
				gfx::Point{ 0, -1 },
				gfx::Point{ 0, 1 },
			};

			for (const auto& offset : offsets) {
				imgui.drawlist->AddText(
					font.im_font(), font.size(), pos + offset, outline_colour.to_imgui(), text.data()
				);
			}
		}

		if (flags & FONT_DROPSHADOW) {
			const gfx::Color dropshadow_colour(0, 0, 0, colour.a * 0.6f);
			const int shift_amount = 1;

			imgui.drawlist->AddText(
				font.im_font(), font.size(), pos.offset(shift_amount), dropshadow_colour.to_imgui(), text.data()
			);
		}
	}

	imgui.drawlist->AddText(font.im_font(), font.size(), pos, colour.to_imgui(), text.data());

	if (rotation_deg != 0.f) {
		int vtx_idx_end = imgui.drawlist->_VtxCurrentIdx;

		auto text_size = font.calc_size(text);
		gfx::Rect text_rect(pos, gfx::Size(text_size.w, rotation_pivot_y));

		ImVec2 pivot_in = text_rect.center();
		ImVec2 pivot_out = text_rect.center();

		float ang = u::deg_to_rad(rotation_deg);
		ImGui::ShadeVertsTransformPos(
			imgui.drawlist, vtx_idx_begin, vtx_idx_end, pivot_in, std::cos(ang), std::sin(ang), pivot_out
		);
	}

	ImGui::PopFont();
}

render::Texture::~Texture() {
	destroy();
}

bool render::Texture::load_from_file(const std::string& path) {
	// Use SDL to load the image
	SDL_Surface* surface = IMG_Load(path.c_str());

	if (!surface) {
		u::log_error("Failed to load image: {}", SDL_GetError());
		return false;
	}

	// Convert to RGBA format if needed
	SDL_Surface* rgb_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(surface);

	if (!rgb_surface) {
		u::log_error("Failed to convert surface: {}", SDL_GetError());
		return false;
	}

	bool result = load_from_surface(rgb_surface);
	SDL_DestroySurface(rgb_surface);

	return result;
}

bool render::Texture::load_from_memory(const unsigned char* data, std::size_t size) {
	// png only (the flag graphics); decoded with stb_image because the sdl3_image
	// build we link only carries jpeg.
	int width = 0;
	int height = 0;
	int channels = 0;

	unsigned char* pixels = stbi_load_from_memory(
		data, static_cast<int>(size), &width, &height, &channels, 4
	);

	if (pixels == nullptr) {
		u::log_error("Failed to decode png: {}", stbi_failure_reason());
		return false;
	}

	SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
	if (surface == nullptr) {
		stbi_image_free(pixels);
		u::log_error("Failed to wrap decoded png: {}", SDL_GetError());
		return false;
	}

	const bool result = load_from_surface(surface);

	SDL_DestroySurface(surface);
	stbi_image_free(pixels);

	return result;
}

bool render::Texture::load_from_surface(SDL_Surface* surface) {
	if (!surface)
		return false;

	// Delete old texture if exists
	destroy();

	// Create a new texture
	glGenTextures(1, &m_id);
	glBindTexture(GL_TEXTURE_2D, m_id);

	// Setup texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Upload pixels into texture
	GLenum format = GL_RGBA;
	glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0, format, GL_UNSIGNED_BYTE, surface->pixels);

	m_width = surface->w;
	m_height = surface->h;

	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

void render::Texture::destroy() {
	if (m_id) {
		glDeleteTextures(1, &m_id);
		m_id = 0;
		m_width = 0;
		m_height = 0;
	}
}

void render::Texture::bind() const {
	glBindTexture(GL_TEXTURE_2D, m_id);
}

void render::Texture::unbind() {
	glBindTexture(GL_TEXTURE_2D, 0);
}

// Image rendering functions
void render::image(const gfx::Rect& rect, const Texture& texture, const gfx::Color& tint_color) {
	if (!texture.is_valid())
		return;

	imgui.drawlist->AddImage(
		texture.get_id(), rect.origin(), rect.max(), ImVec2(0, 0), ImVec2(1, 1), tint_color.to_imgui()
	);
}

void render::image_with_borders(
	const gfx::Rect& rect,
	const Texture& texture,
	const gfx::Color& border_color,
	const gfx::Color& inner_border_color,
	float border_thickness,
	const gfx::Color& tint_color
) {
	if (!texture.is_valid())
		return;

	image(rect.shrink(3), texture, tint_color);

	rect_stroke(rect.shrink(2), border_color, border_thickness);
	rect_stroke(rect.shrink(1), inner_border_color, border_thickness);
	rect_stroke(rect, border_color, border_thickness);
}

void render::image_rounded(
	const gfx::Rect& rect,
	const Texture& texture,
	float rounding,
	unsigned int rounding_flags,
	const gfx::Color& tint_color
) {
	if (!texture.is_valid())
		return;

	imgui.drawlist->AddImageRounded(
		texture.get_id(),
		rect.origin(),
		rect.max(),
		ImVec2(0, 0),
		ImVec2(1, 1),
		tint_color.to_imgui(),
		rounding,
		rounding_flags
	);
}

void render::rounded_image_with_borders(
	const gfx::Rect& rect,
	const Texture& texture,
	float rounding,
	const gfx::Color& border_color,
	const gfx::Color& inner_border_color,
	float border_thickness,
	unsigned int rounding_flags,
	const gfx::Color& tint_color
) {
	if (!texture.is_valid())
		return;

	image_rounded(rect.shrink(3), texture, rounding, rounding_flags, tint_color);

	rounded_rect_stroke(rect.shrink(2), border_color, rounding, rounding_flags, border_thickness);
	rounded_rect_stroke(rect.shrink(1), inner_border_color, rounding, rounding_flags, border_thickness);
	rounded_rect_stroke(rect, border_color, rounding, rounding_flags, border_thickness);
}

void render::push_clip_rect(const gfx::Rect& rect, bool intersect_clip_rect) {
	imgui.drawlist->PushClipRect(rect.origin(), rect.max(), intersect_clip_rect);
}

void render::push_clip_rect(int x1, int y1, int x2, int y2, bool intersect_clip_rect) {
	push_clip_rect(gfx::Rect(gfx::Point(x1, y1), gfx::Point(x2, y2)), intersect_clip_rect);
}

void render::push_fullscreen_clip_rect() {
	push_clip_rect(0, 0, window_size.w, window_size.h, false);
}

gfx::Rect render::pop_clip_rect() {
	ImVec2 min = imgui.drawlist->GetClipRectMin();
	ImVec2 max = imgui.drawlist->GetClipRectMax();

	imgui.drawlist->PopClipRect();

	return {
		gfx::Point(min.x, min.y),
		gfx::Point(max.x, max.y),
	};
}

gfx::Rect render::get_clip_rect() {
	ImVec2 min = imgui.drawlist->GetClipRectMin();
	ImVec2 max = imgui.drawlist->GetClipRectMax();

	return {
		gfx::Point(min.x, min.y),
		gfx::Point(max.x, max.y),
	};
}

namespace {
	// Number of bytes of the UTF-8 sequence that starts at `i`.
	size_t utf8_seq_len(const std::string& text, size_t i) {
		const auto byte = static_cast<unsigned char>(text[i]);
		if (byte < 0x80)
			return 1;
		if ((byte & 0xE0) == 0xC0)
			return 2;
		if ((byte & 0xF0) == 0xE0)
			return 3;
		if ((byte & 0xF8) == 0xF0)
			return 4;
		return 1;
	}

	// Offset of the first byte of the codepoint that ends at `end`.
	size_t utf8_prev_start(const std::string& text, size_t end) {
		size_t i = end;
		while (i > 0) {
			i--;
			if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80)
				return i;
		}
		return 0;
	}

	unsigned int utf8_decode(const std::string& text, size_t i, size_t len) {
		const auto b0 = static_cast<unsigned char>(text[i]);
		if (len == 1)
			return b0;
		if (len == 2)
			return ((b0 & 0x1Fu) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3Fu);
		if (len == 3)
			return ((b0 & 0x0Fu) << 12) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6)
			     | (static_cast<unsigned char>(text[i + 2]) & 0x3Fu);
		return ((b0 & 0x07u) << 18) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 12)
		     | ((static_cast<unsigned char>(text[i + 2]) & 0x3Fu) << 6)
		     | (static_cast<unsigned char>(text[i + 3]) & 0x3Fu);
	}

	// Codepoints that may be line-broken without inserting a space (CJK/Kana/Hangul,
	// fullwidth forms and the CJK punctuation blocks).
	bool is_cjk(unsigned int cp) {
		return (cp >= 0x1100 && cp <= 0x11FF) || (cp >= 0x2E80 && cp <= 0x33FF)
		    || (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF)
		    || (cp >= 0xA000 && cp <= 0xA4CF) || (cp >= 0xAC00 && cp <= 0xD7AF)
		    || (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE30 && cp <= 0xFE4F)
		    || (cp >= 0xFF00 && cp <= 0xFFEF) || (cp >= 0x20000 && cp <= 0x3FFFF);
	}
}

bool render::clip_string(std::string& text, const Font& font, int max_width, int min_chars) {
	int size = font.calc_size(text).w;
	if (size <= max_width)
		return true;

	int dots_size = font.calc_size("...").w;

	// Strip whole codepoints, not bytes: popping a byte off a multi-byte UTF-8
	// sequence would produce garbage for any non-ASCII text.
	while (static_cast<int>(text.size()) > min_chars) {
		text.erase(utf8_prev_start(text, text.size()));

		if (font.calc_size(text).w + dots_size <= max_width) {
			text += "...";
			return true;
		}
	}

	text += "...";
	return false;
}

std::vector<std::string> render::wrap_text(
	const std::string& text, const gfx::Size& dimensions, const Font& font, int line_height
) {
	std::vector<std::string> lines;
	if (!font)
		return lines;

	// Tokenise the string into units that may be separated by a line break:
	//   * a run of whitespace becomes one space token (and a break opportunity),
	//   * every CJK codepoint is its own token (CJK has no inter-word spaces, so
	//     any two ideographs are a valid break point),
	//   * everything else accumulates into a word token (Latin words must not be
	//     split unless they are wider than the whole box).
	// Working on UTF-8 codepoints instead of raw bytes also means a line break can
	// never fall in the middle of a multi-byte character.
	std::vector<std::string> tokens;
	std::string word;
	size_t i = 0;
	while (i < text.size()) {
		const size_t len = utf8_seq_len(text, i);
		const unsigned int cp = utf8_decode(text, i, len);
		std::string ch = text.substr(i, len);
		i += len;

		if (cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n') {
			if (!word.empty()) {
				tokens.push_back(word);
				word.clear();
			}
			tokens.emplace_back(" ");
		}
		else if (is_cjk(cp)) {
			if (!word.empty()) {
				tokens.push_back(word);
				word.clear();
			}
			tokens.push_back(ch);
		}
		else {
			word += ch;
		}
	}
	if (!word.empty())
		tokens.push_back(word);

	const auto width_of = [&font](const std::string& value) {
		return font.calc_size(value).w;
	};

	std::string current_line;

	for (const auto& token : tokens) {
		if (token == " ") {
			if (!current_line.empty())
				current_line += ' ';
			continue;
		}

		if (width_of(current_line + token) <= dimensions.w) {
			current_line += token;
			continue;
		}

		if (!current_line.empty()) {
			lines.push_back(current_line);
			current_line = token;
			continue;
		}

		// The token alone does not fit: hard break it on codepoint boundaries.
		std::string piece;
		size_t j = 0;
		while (j < token.size()) {
			const size_t len = utf8_seq_len(token, j);
			const std::string unit = token.substr(j, len);
			j += len;

			if (!piece.empty() && width_of(piece + unit) > dimensions.w) {
				lines.push_back(piece);
				piece = unit;
			}
			else {
				piece += unit;
			}
		}
		current_line = piece;
	}

	if (!current_line.empty())
		lines.push_back(current_line);

	if (lines.empty())
		lines.emplace_back();

	(void)line_height;
	return lines;
}
