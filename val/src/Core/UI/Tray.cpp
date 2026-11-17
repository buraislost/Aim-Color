// ui/Tray.cpp — notification-area icon: hide the window there, restore or quit from its menu
// Depends: ui/Window.hpp

// ===== Includes =====
#include "Stdafx.hpp"
#include "Core/UI/Window.hpp"
#include "Core/UI/TrayMessage.hpp"

namespace Core::UI
{
	// ===== Constants =====
	namespace
	{
		constexpr UINT kMenuShow = 1;
		constexpr UINT kMenuQuit = 2;
	}

	// ===== Public API (Window members) =====
	void Window::hide_to_tray( )
	{
		if ( in_tray_ ) {
			return;
		}

		nid_                  = {};
		nid_.cbSize           = sizeof( nid_ );
		nid_.hWnd             = hwnd_;
		nid_.uID              = 1;
		nid_.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		nid_.uCallbackMessage = kTrayMessage;
		nid_.hIcon            = LoadIconW( nullptr, IDI_APPLICATION );
		wcscpy_s( nid_.szTip, L"Bura" );

		Shell_NotifyIconW( NIM_ADD, &nid_ );
		ShowWindow( hwnd_, SW_HIDE );
		in_tray_ = true;
	}

	void Window::restore_from_tray( )
	{
		if ( !in_tray_ ) {
			return;
		}

		Shell_NotifyIconW( NIM_DELETE, &nid_ );
		ShowWindow( hwnd_, SW_SHOW );
		SetForegroundWindow( hwnd_ );
		in_tray_ = false;
	}

	void Window::show_tray_menu( )
	{
		POINT cursor {};
		GetCursorPos( &cursor );

		const HMENU menu = CreatePopupMenu( );
		AppendMenuW( menu, MF_STRING, kMenuShow, L"Show Bura" );
		AppendMenuW( menu, MF_STRING, kMenuQuit, L"Quit" );

		SetForegroundWindow( hwnd_ ); // without this the menu does not close on an outside click
		const int cmd = TrackPopupMenu( menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd_, nullptr );
		DestroyMenu( menu );

		if ( cmd == static_cast<int>( kMenuShow ) ) {
			restore_from_tray( );
		}
		else if ( cmd == static_cast<int>( kMenuQuit ) ) {
			running_ = false;
		}
	}
}
