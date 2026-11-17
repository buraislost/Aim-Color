#include "Stdafx.hpp"
#include "Core/UI/Window.hpp"
#include "Core/BindSystem/BindSystem.hpp"
#include "Core/Config/Config.hpp"
#include "Core/Mouse/Mouse.hpp"
#include "Core/Features/DebugState.hpp"

namespace Core::UI
{

	// Bura web UI look: dark violet palette, 12px rounded cards, pill tabs, violet sliders
	static void apply_bura_style( )
	{
		ImGuiStyle& st = ImGui::GetStyle( );
		st.WindowRounding = 12.f;  st.ChildRounding = 12.f; st.FrameRounding = 8.f;
		st.PopupRounding  = 8.f;   st.GrabRounding  = 8.f;  st.TabRounding   = 8.f;
		st.ScrollbarRounding = 8.f;
		st.WindowPadding = { 16.f, 14.f }; st.FramePadding = { 10.f, 7.f };
		st.ItemSpacing   = { 10.f, 9.f };  st.WindowBorderSize = 1.f; st.FrameBorderSize = 1.f;
		st.GrabMinSize   = 14.f;

		auto rgb = [ ]( int r, int g, int b, float a = 1.f ) { return ImVec4( r / 255.f, g / 255.f, b / 255.f, a ); };
		const ImVec4 bg = rgb( 11, 11, 16 ), panel = rgb( 18, 18, 26 ), card = rgb( 23, 23, 34 ), card2 = rgb( 28, 28, 42 );
		const ImVec4 line = rgb( 38, 38, 54 ), line2 = rgb( 49, 49, 74 );
		const ImVec4 tx = rgb( 232, 232, 242 ), tx2 = rgb( 154, 154, 180 ), tx3 = rgb( 95, 95, 122 );
		const ImVec4 ac = rgb( 124, 92, 252 ), ac2 = rgb( 164, 139, 255 );

		ImVec4* c = st.Colors;
		c[ ImGuiCol_Text ] = tx;                 c[ ImGuiCol_TextDisabled ] = tx3;
		c[ ImGuiCol_WindowBg ] = bg;             c[ ImGuiCol_ChildBg ] = card;
		c[ ImGuiCol_PopupBg ] = card2;           c[ ImGuiCol_Border ] = line;
		c[ ImGuiCol_FrameBg ] = card2;           c[ ImGuiCol_FrameBgHovered ] = line2;
		c[ ImGuiCol_FrameBgActive ] = line2;
		c[ ImGuiCol_TitleBg ] = panel;           c[ ImGuiCol_TitleBgActive ] = panel;
		c[ ImGuiCol_TitleBgCollapsed ] = panel;
		c[ ImGuiCol_CheckMark ] = ac2;
		c[ ImGuiCol_SliderGrab ] = ac;           c[ ImGuiCol_SliderGrabActive ] = ac2;
		c[ ImGuiCol_Button ] = card2;            c[ ImGuiCol_ButtonHovered ] = ac;
		c[ ImGuiCol_ButtonActive ] = ac2;
		c[ ImGuiCol_Header ] = rgb( 124, 92, 252, 0.28f );
		c[ ImGuiCol_HeaderHovered ] = rgb( 124, 92, 252, 0.45f );
		c[ ImGuiCol_HeaderActive ] = ac;
		c[ ImGuiCol_Tab ] = panel;               c[ ImGuiCol_TabHovered ] = rgb( 124, 92, 252, 0.55f );
		c[ ImGuiCol_TabActive ] = ac;            c[ ImGuiCol_TabUnfocused ] = panel;
		c[ ImGuiCol_TabUnfocusedActive ] = rgb( 124, 92, 252, 0.6f );
		c[ ImGuiCol_Separator ] = line;          c[ ImGuiCol_ScrollbarBg ] = bg;
		c[ ImGuiCol_ScrollbarGrab ] = line2;
		( void )tx2;
	}
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
			.lpszClassName = title.c_str( ),
		};

		if ( !RegisterClassExW( &wc_ ) )
			throw std::runtime_error( "Failed to register window class." );

		const ImVec2 screen_center
		{
			GetSystemMetrics( SM_CXSCREEN ) / 2 - size_.x / 2,
			GetSystemMetrics( SM_CYSCREEN ) / 2 - size_.y / 2
		};

		hwnd_ = CreateWindow
		(
			wc_.lpszClassName,
			title.c_str( ),
			WS_POPUP | WS_VISIBLE,
			screen_center.x,
			screen_center.y,
			size_.x,
			size_.y,
			nullptr,
			nullptr,
			wc_.hInstance,
			this
		);

		if ( !hwnd_ )
			throw std::runtime_error( "Failed to create window." );

		d3d_device_ = std::make_shared<D3D11Device>( hwnd_ );

		initialize_imgui( );

		ShowWindow( hwnd_, SW_SHOWDEFAULT );
		UpdateWindow( hwnd_ );
	}

	Window::~Window( )
	{
		shutdown_imgui( );

		if ( hwnd_ )
			DestroyWindow( hwnd_ );

		UnregisterClassW( wc_.lpszClassName, wc_.hInstance );
	}

	ServiceHub* Window::get_service_hub( ) const
	{
		return service_hub_;
	}

	void Window::initialize_imgui( ) const
	{
		ImGui::CreateContext( );
		apply_bura_style( );

		ImGuiIO& io    = ImGui::GetIO( );
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;

		ImGui_ImplWin32_Init( hwnd_ );
		ImGui_ImplDX11_Init( d3d_device_->get_device( ), d3d_device_->get_device_context( ) );
	}

	void Window::shutdown_imgui( ) const
	{
		ImGui_ImplDX11_Shutdown( );
		ImGui_ImplWin32_Shutdown( );
		ImGui::DestroyContext( );
	}

	void Window::upload_preview( const cv::Mat& rgba )
	{
		auto* device  = d3d_device_->get_device( );
		auto* context = d3d_device_->get_device_context( );

		if ( !preview_tex_ || preview_w_ != rgba.cols || preview_h_ != rgba.rows )
		{
			preview_srv_.Reset( );
			preview_tex_.Reset( );

			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width                = rgba.cols;
			desc.Height               = rgba.rows;
			desc.MipLevels            = 1;
			desc.ArraySize            = 1;
			desc.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count     = 1;
			desc.Usage                = D3D11_USAGE_DYNAMIC;
			desc.BindFlags            = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags       = D3D11_CPU_ACCESS_WRITE;

			if ( FAILED( device->CreateTexture2D( &desc, nullptr, &preview_tex_ ) ) ||
			     FAILED( device->CreateShaderResourceView( preview_tex_.Get( ), nullptr, &preview_srv_ ) ) )
			{
				preview_srv_.Reset( );
				preview_tex_.Reset( );
				return;
			}

			preview_w_ = rgba.cols;
			preview_h_ = rgba.rows;
		}

		D3D11_MAPPED_SUBRESOURCE mapped = {};

		if ( FAILED( context->Map( preview_tex_.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) )
			return;

		for ( int y = 0; y < rgba.rows; ++y )
			std::memcpy( static_cast<std::uint8_t*>( mapped.pData ) + static_cast<std::size_t>( y ) * mapped.RowPitch,
			             rgba.ptr( y ), static_cast<std::size_t>( rgba.cols ) * 4 );

		context->Unmap( preview_tex_.Get( ), 0 );
	}

	// live view of what the color scan sees. while this tab is open ColorBot scans even with no bind held
	// (it still never moves / clicks unless the real bind is active).
	void Window::render_debug_tab( )
	{
		const auto debug  = service_hub_->get<DebugState>( );
		const auto mouse  = service_hub_->get<Mouse>( );
		const auto config = service_hub_->get<Config>( );

		debug->enabled = true;

		const auto info = debug->info( );

		const auto ok  = ImVec4( 0.f, 1.f, 0.f, 1.f );
		const auto bad = ImVec4( 1.f, 0.3f, 0.3f, 1.f );

		auto row = [ & ]( const char* label, const bool good, const char* fmt, auto... args )
		{
			ImGui::TextColored( good ? ok : bad, good ? "[OK]" : "[!!]" );
			ImGui::SameLine( );
			ImGui::Text( "%s: ", label );
			ImGui::SameLine( );
			ImGui::Text( fmt, args... );
		};

		static const char* color_names[ ] = { "Magenta", "Yellow", "Red" };

		row( "Capture", info.captured && ( info.mean_bgr[ 0 ] || info.mean_bgr[ 1 ] || info.mean_bgr[ 2 ] ),
		     "%dx%d  mean BGR(%d,%d,%d)%s", info.cap_w, info.cap_h, info.mean_bgr[ 0 ], info.mean_bgr[ 1 ], info.mean_bgr[ 2 ],
		     !info.captured ? "  NO FRAME" : ( !( info.mean_bgr[ 0 ] || info.mean_bgr[ 1 ] || info.mean_bgr[ 2 ] ) ? "  BLACK FRAME" : "" ) );

		row( "Mask pixels", info.lit > 0, "%d lit  (color: %s)", info.lit,
		     color_names[ std::clamp( info.color, 0, 2 ) ] );

		row( "Blobs", info.big_blobs > 0, "%d total, %d >= min area", info.blobs, info.big_blobs );

		row( "Target", info.found, "head(%d,%d) center(%d,%d) dist %.1f px", info.head.x, info.head.y, info.center.x,
		     info.center.y, info.dist );

		row( "Lock / on target", info.has_lock, "lock=%d on=%d", info.has_lock ? 1 : 0, info.on_target ? 1 : 0 );

		row( "Aim armed", info.aim_active, "%s (bind held or Auto Aim)", info.aim_active ? "yes" : "NO - nothing will move" );
		row( "Trigger armed", info.trigger_active, "%s", info.trigger_active ? "yes" : "no" );

		row( "Input", mouse->is_connected( ), "backend=%d  %s", config->settings_.input_backend, mouse->status( ).c_str( ) );

		ImGui::Text( "Pixel under crosshair HSV: H=%d S=%d V=%d   frames=%llu", info.center_hsv[ 0 ], info.center_hsv[ 1 ],
		             info.center_hsv[ 2 ], static_cast<unsigned long long>( info.frames ) );

		// one-line verdict: first thing in the chain that is broken
		const char* verdict = "Detection OK";

		if ( !info.captured )
			verdict = "Screen capture returns nothing";
		else if ( !( info.mean_bgr[ 0 ] || info.mean_bgr[ 1 ] || info.mean_bgr[ 2 ] ) )
			verdict = "Frame is black: game is exclusive fullscreen / protected. Use borderless windowed";
		else if ( info.lit == 0 )
			verdict = "No pixel matches the color: wrong color option, or outline not in the capture box (raise FOV)";
		else if ( !info.found )
			verdict = "Color found but blobs are below min area";
		else if ( !info.aim_active && !info.trigger_active )
			verdict = "Target seen, but no bind is held: hold the Aimbot/Trigger key (or enable Auto Aim)";
		else if ( !mouse->is_connected( ) )
			verdict = "Target seen and armed, but input device is Disconnected";

		ImGui::Separator( );
		ImGui::TextColored( ImVec4( 1.f, 0.9f, 0.3f, 1.f ), "Verdict: %s", verdict );
		ImGui::Separator( );

		if ( ImGui::Button( "Test move right 40px" ) )
			mouse->move_raw( 40, 0 );

		ImGui::SameLine( );

		if ( ImGui::Button( "Test move left 40px" ) )
			mouse->move_raw( -40, 0 );

		cv::Mat preview;

		if ( debug->preview( preview ) )
			upload_preview( preview );

		if ( preview_srv_ )
		{
			ImGui::Text( "Left: capture    Right: mask    green=crosshair  blue=trigger box  yellow=head" );

			const float avail  = ImGui::GetContentRegionAvail( ).x;
			const float scale  = std::max( 1.f, std::min( 3.f, avail / static_cast<float>( preview_w_ ) ) );
			const ImVec2 shown = { preview_w_ * scale, preview_h_ * scale };

			ImGui::Image( reinterpret_cast<ImTextureID>( preview_srv_.Get( ) ), shown );
		}
	}

	bool Window::is_running( ) const
	{
		return running_;
	}

	void Window::close( )
	{
		running_ = false;
	}

	void Window::run( )
	{
		const auto mouse       = service_hub_->get<Mouse>( );
		const auto config      = service_hub_->get<Config>( );
		const auto bind_system = service_hub_->get<BindSystem>( );

		MSG msg = {};

		char com_buf[ 16 ] = {};
		strncpy_s( com_buf, config->settings_.com_port.c_str( ), _TRUNCATE );

		while ( running_ )
		{
			if ( PeekMessageA( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
			{
				TranslateMessage( &msg );
				DispatchMessageA( &msg );

				if ( msg.message == WM_QUIT )
					running_ = false;
			}

			if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
			{
				POINT point;
				RECT rect;

				GetCursorPos( &point );
				GetWindowRect( hwnd_, &rect );

				offset_.first  = point.x - rect.left;
				offset_.second = point.y - rect.top;
			}

			if ( ImGui::IsMouseDragging( ImGuiMouseButton_Left ) && offset_.second >= 0 &&
			     offset_.second <= ImGui::GetTextLineHeight( ) + ImGui::GetStyle( ).FramePadding.y * 3.5f )
			{
				POINT point;
				GetCursorPos( &point );

				SetWindowPos
				(
					hwnd_,
					nullptr,
					point.x - offset_.first,
					point.y - offset_.second,
					0,
					0,
					SWP_NOSIZE | SWP_NOZORDER
				);
			}

			if ( !running_ )
				break;

			ImGui_ImplDX11_NewFrame( );
			ImGui_ImplWin32_NewFrame( );
			ImGui::NewFrame( );

			ImGui::SetNextWindowSize( size_ );
			ImGui::SetNextWindowPos( { 0.f, 0.f } );

			constexpr auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

			ImGui::Begin( "gamerdoc lixo", nullptr, flags );

			// the debug tab turns this back on every frame it is visible
			service_hub_->get<DebugState>( )->enabled = false;

			// brand header (Bura web UI): violet title + status badge
			ImGui::TextColored( ImVec4( 0.643f, 0.545f, 1.f, 1.f ), "Bura" );
			ImGui::SameLine( ImGui::GetWindowWidth( ) - 100.f );
			ImGui::TextColored( ImVec4( 0.24f, 0.86f, 0.59f, 1.f ), "Running" );
			ImGui::TextDisabled( "Web UI: http://127.0.0.1:8765" );
			ImGui::Separator( );

			if ( ImGui::BeginTabBar( "##tabs" ) )
			{
				if ( ImGui::BeginTabItem( "Aimbot" ) )
				{
					ImGui::Text( "Bind" );
					bind_system->render_keybind_button( "Aimbot", config->aimbot_.bind );

					ImGui::Checkbox( "Auto Aim (no bind)", &config->aimbot_.auto_aim );

					ImGui::Text( "Fov (X)" );
					ImGui::SliderInt( "##fov_x", &config->aimbot_.fov_x, 20, 250, "%dpx" );

					ImGui::Text( "Fov (Y)" );
					ImGui::SliderInt( "##fov_y", &config->aimbot_.fov_y, 20, 250, "%dpx" );

					ImGui::Text( "Offset (X)" );
					ImGui::SliderInt( "##offset_x", &config->aimbot_.offset_x, -15, 15, "%dpx" );

					ImGui::Text( "Offset (Y)" );
					ImGui::SliderInt( "##offset_y", &config->aimbot_.offset_y, -15, 15, "%dpx" );

					ImGui::Text( "Hold (% of head, 0 = off)" );
					ImGui::SliderInt( "##hold_pct", &config->aimbot_.hold_pct, 0, 150, "%d%%" );

					ImGui::Text( "Gain (counts/px)" );
					ImGui::SliderFloat( "##lock_gain", &config->aimbot_.lock_gain, 0.1f, 5.f, "%.2f" );

					ImGui::Text( "Smooth (1 = snap)" );
					ImGui::SliderFloat( "##smooth", &config->aimbot_.smooth, 0.1f, 1.f, "%.2f" );

					ImGui::Text( "Settle" );
					ImGui::SliderInt( "##lock_settle", &config->aimbot_.lock_settle_ms, 1, 30, "%dms" );

					ImGui::EndTabItem( );
				}

				if ( ImGui::BeginTabItem( "Trigger" ) )
				{
					ImGui::Text( "Bind" );
					bind_system->render_keybind_button( "Trigger", config->trigger_.bind );

					ImGui::Checkbox( "Move Check", &config->trigger_.move_check );

					ImGui::Text( "Scan Area" );
					ImGui::SliderInt( "##scan_area", &config->trigger_.scan_area, 1, 30, "%dpx" );

					ImGui::Text( "Delay" );
					ImGui::SliderInt( "##delay", &config->trigger_.delay, 1, 300, "%dms" );

					ImGui::EndTabItem( );
				}

				if ( ImGui::BeginTabItem( "Debug" ) )
					{
						render_debug_tab( );
						ImGui::EndTabItem( );
					}

					if ( ImGui::BeginTabItem( "Settings" ) )
				{
					ImGui::Text( "Target Color" );
					const char* items[ ] = { "Magenta (Tritanopia)", "Yellow (Deuteranopia/Protanopia)", "Red (Default)" };
					ImGui::Combo( "##colors", &config->settings_.color, items, IM_ARRAYSIZE( items ) );

					ImGui::Text( "Input" );
					const char* backends[ ] = { "SendInput (local)", "Makcu (serial)", "Medius (capi dll)" };
					ImGui::Combo( "##backend", &config->settings_.input_backend, backends, IM_ARRAYSIZE( backends ) );

					if ( config->settings_.input_backend == 1 )
					{
						ImGui::Text( "COM port" );

						if ( ImGui::InputText( "##com_port", com_buf, sizeof( com_buf ) ) )
							config->settings_.com_port = com_buf;
					}

					if ( ImGui::Button( "Apply input" ) )
					{
						mouse->configure( config->settings_.input_backend, config->settings_.com_port );
					}

					ImGui::NewLine( );

					if ( ImGui::Button( "Copy config to clipboard" ) )
					{
						ImGui::SetClipboardText( config->export_to_str( ).c_str( ) );
					}

					if ( ImGui::Button( "Import config from clipboard" ) )
					{
						const auto text   = ImGui::GetClipboardText( );
						const auto result = config->import_from_str( text );

						if ( result )
						{
							strncpy_s( com_buf, config->settings_.com_port.c_str( ), _TRUNCATE );
							mouse->configure( config->settings_.input_backend, config->settings_.com_port );
						}

						if ( !result )
						{
							MessageBoxA( nullptr, "Failed to import config!", " ", MB_OK | MB_ICONERROR );
						}
					}

					if ( ImGui::Button( "Close" ) )
					{
						close( );
					}

					ImGui::EndTabItem( );
				}

				ImGui::EndTabBar( );
			}

			const float text_height      = ImGui::GetTextLineHeightWithSpacing( );
			const float available_height = ImGui::GetContentRegionAvail( ).y;

			ImGui::SetCursorPosY( ImGui::GetCursorPosY( ) + available_height - text_height );
			ImGui::Separator( );

			static constexpr double check_interval = 3.0;
			static double last_check_time          = -check_interval;
			static bool is_connected_cached        = false;

			const double current_time = ImGui::GetTime( );

			if ( current_time - last_check_time >= check_interval )
			{
				is_connected_cached = mouse->is_connected( );
				last_check_time     = current_time;

				if ( !is_connected_cached )
				{
					mouse->connect( );
				}
			}

			ImGui::Text( "Input Status:" );
			ImGui::SameLine( );

			if ( is_connected_cached )
			{
				ImGui::TextColored( ImVec4( 0.0f, 1.0f, 0.0f, 1.0f ), "Connected" );
			}
			else
			{
				ImGui::TextColored( ImVec4( 1.0f, 0.0f, 0.0f, 1.0f ), "Disconnected" );
			}

			if ( config->settings_.input_backend == 1 )
			{
				ImGui::SameLine( );
				ImGui::TextDisabled( "(%s)", mouse->status( ).c_str( ) );
			}

			ImGui::End( );

			ImGui::Render( );

			auto* rtv = d3d_device_->get_render_target_view( );
			d3d_device_->get_device_context( )->OMSetRenderTargets( 1, &rtv, nullptr );

			constexpr float transparent_color[ 4 ] = { 0.f, 0.f, 0.f, 0.f };
			d3d_device_->get_device_context( )->ClearRenderTargetView( rtv, transparent_color );

			ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );
			d3d_device_->get_swap_chain( )->Present( 1, 0 );
		}
	}

	LRESULT CALLBACK Window::wnd_proc( const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam )
	{
		if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wparam, lparam ) )
			return true;

		switch ( msg )
		{
			case WM_CLOSE:
			{
				PostQuitMessage( 0 );
				return 0;
			}
			case WM_DESTROY:
			{
				PostQuitMessage( 0 );
				return 0;
			}
		}

		return DefWindowProc( hwnd, msg, wparam, lparam );
	}
}
