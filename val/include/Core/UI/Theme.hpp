#pragma once

// ui/Theme.hpp — design tokens: 8px spacing grid, one palette, one font (two sizes)
// Depends: imgui

namespace Core::UI::theme
{
	// ===== Spacing grid: only 4 / 8 / 16 / 24 =====
	inline constexpr float kSpace1 = 4.f;
	inline constexpr float kSpace2 = 8.f;
	inline constexpr float kSpace3 = 16.f;
	inline constexpr float kSpace4 = 24.f;

	// ===== Sizes (multiples of the grid) =====
	inline constexpr float kRadius      = 8.f;
	inline constexpr float kRowHeight   = 32.f;
	inline constexpr float kLabelWidth  = 96.f;
	inline constexpr float kLogoSize    = 32.f;
	inline constexpr float kTitleScale  = 1.25f; // the one "heading" size, regular text is 1.0
	inline constexpr float kDragZone    = 56.f;  // top strip that moves the window

	// ===== Palette: background, surface, border, text, muted, accent, status =====
	inline ImVec4 rgb( const int r, const int g, const int b, const float a = 1.f )
	{
		return ImVec4( r / 255.f, g / 255.f, b / 255.f, a );
	}

	inline const ImVec4 kBackground = rgb( 11, 11, 16 );
	inline const ImVec4 kSurface    = rgb( 23, 23, 34 );
	inline const ImVec4 kSurfaceHi  = rgb( 38, 38, 56 ); // hover / track
	inline const ImVec4 kBorder     = rgb( 38, 38, 54 );
	inline const ImVec4 kText       = rgb( 232, 232, 242 );
	inline const ImVec4 kMuted      = rgb( 130, 130, 160 );
	inline const ImVec4 kAccent     = rgb( 124, 92, 252 );
	inline const ImVec4 kAccentHi   = rgb( 164, 139, 255 );
	inline const ImVec4 kOk         = rgb( 61, 220, 151 );
	inline const ImVec4 kBad        = rgb( 229, 72, 77 );
	inline const ImVec4 kWarn       = rgb( 245, 190, 70 );

	void apply( );
}
