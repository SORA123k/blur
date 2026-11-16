#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../render/primitives/color.h"

// ---------------------------------------------------------------------------
// theme — design tokens ported from clickgui-ui/index.html (:root variables)
//
//   --accent   #8b5cf6   violet
//   --accent-2 #22d3ee   cyan
//   --panel    rgba(16,16,24,.78)
//   --card     rgba(255,255,255,.035)
//   --stroke   rgba(255,255,255,.07)
//   --stroke-2 rgba(255,255,255,.13)
//   --text     #ececf5
//   --muted    #8a8a9e
//   --radius   16px
//
// All sizes below are in *UI units* (the design space). The whole UI is scaled
// once at draw time by theme::scale, so nothing here needs to know about DPI.
// ---------------------------------------------------------------------------
namespace theme {
	// --- brand --------------------------------------------------------------
	inline constexpr gfx::Color accent = { 139, 92, 246, 255 };  // #8b5cf6
	inline constexpr gfx::Color accent2 = { 34, 211, 238, 255 }; // #22d3ee
	inline constexpr gfx::Color accent_text = { 167, 139, 250, 255 }; // #a78bfa

	// --- ink ----------------------------------------------------------------
	inline constexpr gfx::Color text = { 236, 236, 245, 255 };     // #ececf5
	inline constexpr gfx::Color text_muted = { 138, 138, 158, 255 }; // #8a8a9e
	inline constexpr gfx::Color text_faint = { 95, 95, 116, 255 };   // #5f5f74

	// --- surfaces -----------------------------------------------------------
	inline constexpr gfx::Color backdrop = { 8, 8, 13, 255 };      // #08080d
	inline constexpr gfx::Color panel = { 16, 16, 24, 255 };       // #101018
	inline constexpr gfx::Color panel_raised = { 22, 22, 32, 255 }; // popups / drawers
	inline constexpr gfx::Color field = { 8, 8, 14, 255 };         // inset inputs (rgba(0,0,0,.28) over dark)

	// --- radii --------------------------------------------------------------
	inline constexpr float radius_panel = 16.f;
	inline constexpr float radius_card = 14.f;
	inline constexpr float radius_ctrl = 10.f;
	inline constexpr float radius_field = 9.f;
	inline constexpr float radius_pill = 999.f;

	// --- global UI scale (set once at startup by render::init) --------------
	inline float scale = 1.f;

	// --- colour helpers -----------------------------------------------------
	inline gfx::Color white(float alpha) {
		return { 255, 255, 255, static_cast<uint8_t>(std::lround(std::clamp(alpha, 0.f, 1.f) * 255.f)) };
	}

	inline gfx::Color black(float alpha) {
		return { 0, 0, 0, static_cast<uint8_t>(std::lround(std::clamp(alpha, 0.f, 1.f) * 255.f)) };
	}

	inline gfx::Color with_alpha(const gfx::Color& colour, float alpha) {
		return { colour.r, colour.g, colour.b, static_cast<uint8_t>(std::lround(std::clamp(alpha, 0.f, 1.f) * 255.f)) };
	}

	inline gfx::Color mix(const gfx::Color& from, const gfx::Color& to, float t) {
		t = std::clamp(t, 0.f, 1.f);
		return {
			static_cast<uint8_t>(std::lround(from.r + (to.r - from.r) * t)),
			static_cast<uint8_t>(std::lround(from.g + (to.g - from.g) * t)),
			static_cast<uint8_t>(std::lround(from.b + (to.b - from.b) * t)),
			static_cast<uint8_t>(std::lround(from.a + (to.a - from.a) * t)),
		};
	}

	inline gfx::Color accent_alpha(float alpha) {
		return with_alpha(accent, alpha);
	}

	// rgba(255,255,255,.035) card / .065 hover
	inline gfx::Color card(float hover = 0.f) {
		return white(0.035f + (0.030f * std::clamp(hover, 0.f, 1.f)));
	}

	inline gfx::Color stroke(float hover = 0.f) {
		return white(0.07f + (0.06f * std::clamp(hover, 0.f, 1.f)));
	}
}
