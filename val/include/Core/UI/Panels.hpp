#pragma once

// ui/Panels.hpp — one function per screen area (header, tabs, footer, close prompt)
// Depends: ui/Widgets.hpp, Config, BindSystem, Mouse

#include "Core/BindSystem/BindSystem.hpp"
#include "Core/Config/Config.hpp"
#include "Core/Mouse/Mouse.hpp"

namespace Core::UI::panel
{
	struct HeaderResult
	{
		bool close_clicked = false;
		bool web_clicked   = false;
		bool home_clicked  = false;
	};

	struct LauncherResult
	{
		bool close_clicked = false;
		bool open_menu     = false;
	};

	enum class CloseChoice
	{
		None,
		Tray,
		Quit,
	};

	// Command-like: draw functions, state lives in Config / Mouse
	HeaderResult header( bool connected, bool show_home );
	LauncherResult launcher( bool connected, const Mouse& mouse );
	void aimbot( Config& config, const BindSystem& binds );
	void trigger( Config& config, const BindSystem& binds );
	void settings( Config& config, Mouse& mouse, char* com_buf );
	void footer( const Mouse& mouse );
	CloseChoice close_prompt( bool open );
}
