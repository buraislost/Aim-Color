#pragma once

// ui/TrayMessage.hpp — the private window message the tray icon posts to the Bura window
// Depends: Win32

namespace Core::UI
{
	inline constexpr UINT kTrayMessage = WM_APP + 1;
}
