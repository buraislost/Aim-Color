// ui/WindowProc.cpp — Win32 message handler: tray callbacks, close request, destroy
// Depends: ui/Window.hpp, ui/TrayMessage.hpp

// ===== Includes =====
#include "Stdafx.hpp"
#include "Core/UI/Window.hpp"
#include "Core/UI/TrayMessage.hpp"

namespace Core::UI
{
	// ===== Public API (Window members) =====
	LRESULT CALLBACK Window::wnd_proc( const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam )
	{
		if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wparam, lparam ) ) {
			return true;
		}

		if ( msg == WM_NCCREATE ) {
			const auto* cs = reinterpret_cast<CREATESTRUCTW*>( lparam );
			SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( cs->lpCreateParams ) );
		}

		auto* self = reinterpret_cast<Window*>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );

		if ( msg == kTrayMessage && self ) {
			if ( LOWORD( lparam ) == WM_LBUTTONUP || LOWORD( lparam ) == WM_LBUTTONDBLCLK ) {
				self->restore_from_tray( );
			}
			else if ( LOWORD( lparam ) == WM_RBUTTONUP ) {
				self->show_tray_menu( );
			}

			return 0;
		}

		if ( msg == WM_CLOSE ) { // Alt+F4 / taskbar close: ask instead of quitting
			self ? self->request_close( ) : PostQuitMessage( 0 );
			return 0;
		}

		if ( msg == WM_DESTROY ) {
			PostQuitMessage( 0 );
			return 0;
		}

		return DefWindowProc( hwnd, msg, wparam, lparam );
	}
}
