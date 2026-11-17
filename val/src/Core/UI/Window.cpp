// ui/Window.cpp — window lifetime, the main loop steps, and the Win32 message handler
// Depends: ui/Window.hpp, ui/Panels.hpp, ui/Theme.hpp, Config, Mouse, BindSystem

// ===== Includes =====
#include "Stdafx.hpp"
#include "Core/UI/Window.hpp"
#include "Core/UI/Panels.hpp"
#include "Core/UI/Widgets.hpp"
#include "Core/UI/Theme.hpp"
#include "Core/UI/TrayMessage.hpp"
#include "Core/BindSystem/BindSystem.hpp"
#include "Core/Config/Config.hpp"
#include "Core/Constants.hpp"
#include "Core/Mouse/Mouse.hpp"
#include "Core/Features/DebugState.hpp"

#include <shellapi.h>

namespace Core::UI
{
	namespace th = theme;

	// ===== Constants =====
	namespace
	{
		constexpr wchar_t kClassName[ ]       = L"BuraWindow";
		constexpr double kConnectionCheckS    = 3.0;
		constexpr auto kAutosaveInterval      = std::chrono::seconds( 1 );
		constexpr DWORD kTrayIdleSleepMs      = 40;
		constexpr float kFooterHeight         = th::kRowHeight - th::kSpace2;
		constexpr ImGuiWindowFlags kRootFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		                                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
		                                        ImGuiWindowFlags_NoScrollWithMouse;
	}

	// ===== Public API: lifetime =====
	Window::Window( const std::wstring& title, const ImVec2 size, ServiceHub& service_hub )
		: size_( size ),
		  offset_( 0, 0 ),
		  service_hub_( &service_hub )
	{
		wc_ =
		{
			.cbSize = sizeof( wc_ ),
			.style = CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = wnd_proc,
			.hInstance = GetModuleHandleA( nullptr ),
			.hCursor = LoadCursor( nullptr, IDC_ARROW ),
			.lpszClassName = kClassName,
		};

		if ( !RegisterClassExW( &wc_ ) ) {
			throw std::runtime_error( "Failed to register window class." );
		}

		const int x = GetSystemMetrics( SM_CXSCREEN ) / 2 - static_cast<int>( size_.x ) / 2;
		const int y = GetSystemMetrics( SM_CYSCREEN ) / 2 - static_cast<int>( size_.y ) / 2;

		hwnd_ = CreateWindow( wc_.lpszClassName, title.c_str( ), WS_POPUP | WS_VISIBLE, x, y, static_cast<int>( size_.x ),
		                      static_cast<int>( size_.y ), nullptr, nullptr, wc_.hInstance, this );

		if ( !hwnd_ ) {
			throw std::runtime_error( "Failed to create window." );
		}

		d3d_device_ = std::make_shared<D3D11Device>( hwnd_ );
		initialize_imgui( );

		ShowWindow( hwnd_, SW_SHOWDEFAULT );
		UpdateWindow( hwnd_ );
	}

	Window::~Window( )
	{
		if ( in_tray_ ) {
			Shell_NotifyIconW( NIM_DELETE, &nid_ );
		}

		shutdown_imgui( );

		if ( hwnd_ ) {
			DestroyWindow( hwnd_ );
		}

		UnregisterClassW( wc_.lpszClassName, wc_.hInstance );
	}

	ServiceHub* Window::get_service_hub( ) const
	{
		return service_hub_;
	}

	bool Window::is_running( ) const
	{
		return running_;
	}

	void Window::close( )
	{
		running_ = false;
	}

	void Window::initialize_imgui( ) const
	{
		ImGui::CreateContext( );
		theme::apply( );

		ImGuiIO& io    = ImGui::GetIO( );
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;

		// one UI font: Segoe UI when present (the built-in bitmap font is the fallback)
		constexpr const char* kFontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
		io.Fonts->AddFontFromFileTTF( kFontPath, theme::kFontSize );

		ImGui_ImplWin32_Init( hwnd_ );
		ImGui_ImplDX11_Init( d3d_device_->get_device( ), d3d_device_->get_device_context( ) );
	}

	void Window::shutdown_imgui( ) const
	{
		ImGui_ImplDX11_Shutdown( );
		ImGui_ImplWin32_Shutdown( );
		ImGui::DestroyContext( );
	}

	// ===== Main loop =====
	void Window::run( )
	{
		const auto config = service_hub_->get<Config>( );

		strncpy_s( com_buf_, config->settings_.com_port.c_str( ), _TRUNCATE );
		saved_json_      = nlohmann::json( *config ).dump( );
		last_save_check_ = std::chrono::steady_clock::now( );

		while ( running_ ) {
			pump_messages( );
			autosave_config( );

			if ( in_tray_ ) {
				Sleep( kTrayIdleSleepMs ); // hidden: no rendering, aim keeps running on its own thread
				continue;
			}

			drag_window( );

			if ( running_ ) {
				render_frame( );
			}
		}

		config->save_file( );
	}

	void Window::pump_messages( )
	{
		MSG msg = {};

		if ( !PeekMessageA( &msg, nullptr, 0U, 0U, PM_REMOVE ) ) {
			return;
		}

		TranslateMessage( &msg );
		DispatchMessageA( &msg );

		if ( msg.message == WM_QUIT ) {
			running_ = false;
		}
	}

	// ImGui and the web UI edit the same Config; whichever changed it, it is written within a second
	void Window::autosave_config( )
	{
		const auto now = std::chrono::steady_clock::now( );

		if ( now - last_save_check_ < kAutosaveInterval ) {
			return;
		}

		last_save_check_ = now;
		const auto config = service_hub_->get<Config>( );
		std::string current = nlohmann::json( *config ).dump( );

		if ( current == saved_json_ ) {
			return;
		}

		config->save_file( );
		saved_json_ = std::move( current );
	}

	// the window has no title bar: dragging the top strip moves it
	void Window::drag_window( )
	{
		if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ) {
			POINT point;
			RECT rect;

			GetCursorPos( &point );
			GetWindowRect( hwnd_, &rect );

			offset_.first  = point.x - rect.left;
			offset_.second = point.y - rect.top;
		}

		if ( !ImGui::IsMouseDragging( ImGuiMouseButton_Left ) || offset_.second > th::kDragZone ) {
			return;
		}

		POINT point;
		GetCursorPos( &point );
		SetWindowPos( hwnd_, nullptr, point.x - offset_.first, point.y - offset_.second, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
	}

	void Window::render_frame( )
	{
		ImGui_ImplDX11_NewFrame( );
		ImGui_ImplWin32_NewFrame( );
		ImGui::NewFrame( );

		// the web UI may have changed the COM port: follow it unless the field is being edited
		if ( !ImGui::IsAnyItemActive( ) ) {
			strncpy_s( com_buf_, service_hub_->get<Config>( )->settings_.com_port.c_str( ), _TRUNCATE );
		}

		ImGui::SetNextWindowSize( size_ );
		ImGui::SetNextWindowPos( { 0.f, 0.f } );
		ImGui::Begin( "Bura", nullptr, kRootFlags );

		draw_ui( );

		ImGui::End( );
		present( );
	}

	// polled every few seconds; reconnects when the device dropped
	void Window::refresh_connection( )
	{
		const double now = ImGui::GetTime( );

		if ( now < next_connection_check_ ) {
			return;
		}

		const auto mouse         = service_hub_->get<Mouse>( );
		next_connection_check_   = now + kConnectionCheckS;
		connected_               = mouse->is_connected( );

		if ( !connected_ ) {
			mouse->connect( );
		}
	}

	void Window::draw_ui( )
	{
		const auto mouse = service_hub_->get<Mouse>( );
		static int tab   = 0;
		static const char* const tab_names[ ] = { "Aimbot", "Trigger", "Debug", "Settings" };

		service_hub_->get<DebugState>( )->enabled = false; // the Debug tab turns it back on while visible
		refresh_connection( );

		const auto header = panel::header( connected_ );

		if ( header.close_clicked ) {
			close_prompt_ = true;
		}

		if ( header.web_clicked ) {
			ShellExecuteA( nullptr, "open", kWebUiUrl, nullptr, nullptr, SW_SHOWNORMAL );
		}

		widget::tabs( tab_names, 4, tab );

		ImGui::BeginChild( "##body", { 0.f, -kFooterHeight }, 1 );
		draw_tab_body( tab );
		ImGui::EndChild( );

		panel::footer( *mouse );

		switch ( panel::close_prompt( close_prompt_.exchange( false ) ) ) {
			case panel::CloseChoice::Tray: hide_to_tray( ); break;
			case panel::CloseChoice::Quit: close( ); break;
			case panel::CloseChoice::None: break;
		}
	}

	void Window::draw_tab_body( const int tab )
	{
		const auto config = service_hub_->get<Config>( );
		const auto binds  = service_hub_->get<BindSystem>( );
		const auto mouse  = service_hub_->get<Mouse>( );

		switch ( tab ) {
			case 0: panel::aimbot( *config, *binds ); break;
			case 1: panel::trigger( *config, *binds ); break;
			case 2: render_debug_tab( ); break;
			default: panel::settings( *config, *mouse, com_buf_ ); break;
		}
	}

	void Window::present( )
	{
		ImGui::Render( );

		auto* rtv = d3d_device_->get_render_target_view( );
		d3d_device_->get_device_context( )->OMSetRenderTargets( 1, &rtv, nullptr );

		constexpr float transparent[ 4 ] = { 0.f, 0.f, 0.f, 0.f };
		d3d_device_->get_device_context( )->ClearRenderTargetView( rtv, transparent );

		ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );
		d3d_device_->get_swap_chain( )->Present( 1, 0 );
	}
}
