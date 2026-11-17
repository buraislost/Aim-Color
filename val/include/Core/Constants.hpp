#pragma once

// core/Constants.hpp — app-wide switches and sizes, so no magic numbers live in logic
// Depends: none

namespace Core
{
	// browser UI (127.0.0.1:8765). Off for now: the server is not started and the Web button is hidden.
	inline constexpr bool kWebUiEnabled = false;
	inline constexpr const char* kWebUiUrl = "http://127.0.0.1:8765";

	// hide the Bura window from screen share / capture tools (Windows display affinity).
	// it does not hide the process or the overlay from anything else.
	inline constexpr bool kHideFromCapture = true;

	inline constexpr float kWindowWidth  = 448.f;
	inline constexpr float kWindowHeight = 720.f;
}
