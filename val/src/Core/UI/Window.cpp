#include "Stdafx.hpp"
#include "Core/UI/Window.hpp"
#include "Core/BindSystem/BindSystem.hpp"
#include "Core/Config/Config.hpp"
#include "Core/Mouse/Mouse.hpp"
#include "Core/Features/DebugState.hpp"

#include <shellapi.h>
#include <chrono>
#include <string>

namespace Core::UI
{

	// ---- design tokens (one place to retheme) ----
	namespace tok
	{
		constexpr float label_w = 104.f; // label column of every setting row
		constexpr float row_h   = 26.f;
		constexpr float gap     = 8.f;

		inline ImVec4 rgb( int r, int g, int b, float a = 1.f ) { return ImVec4( r / 255.f, g / 255.f, b / 255.f, a ); }

		inline const ImVec4 bg = rgb( 11, 11, 16 ), panel = rgb( 18, 18, 26 ), card = rgb( 23, 23, 34 ), card2 = rgb( 28, 28, 42 );
		inline const ImVec4 line = rgb( 38, 38, 54 ), line2 = rgb( 49, 49, 74 );
		inline const ImVec4 tx = rgb( 232, 232, 242 ), tx2 = rgb( 154, 154, 180 ), tx3 = rgb( 95, 95, 122 );
		inline const ImVec4 ac = rgb( 124, 92, 252 ), ac2 = rgb( 164, 139, 255 );
		inline const ImVec4 ok = rgb( 61, 220, 151 ), bad = rgb( 229, 72, 77 );
	}

	// ---- components ----
	namespace ui
	{
		inline void section( const char* title )
		{
			ImGui::Dummy( { 0.f, 2.f } );
			ImGui::PushStyleColor( ImGuiCol_Text, tok::tx3 );
			ImGui::TextUnformatted( title );
			ImGui::PopStyleColor( );
			ImGui::Dummy( { 0.f, 0.f } );
		}

		// left label column, then the control takes the rest of the row
		inline void label( const char* text )
		{
			ImGui::AlignTextToFramePadding( );
			ImGui::TextColored( tok::tx2, "%s", text );
			ImGui::SameLine( tok::label_w );
		}

		inline void slider_i( const char* name, std::int32_t* v, int lo, int hi, const char* fmt = "%d" )
		{
			label( name );
			ImGui::PushID( name );
			ImGui::SetNextItemWidth( -1.f );
			ImGui::SliderInt( "##v", v, lo, hi, fmt );
			ImGui::PopID( );
		}

		// two sliders sharing one labelled row, value formats carry the axis ("X %dpx" / "Y %dpx")
		inline void pair_i( const char* name, std::int32_t* a, const char* fa, std::int32_t* b, const char* fb, int lo, int hi )
		{
			label( name );
			ImGui::PushID( name );
			const float w = ( ImGui::GetContentRegionAvail( ).x - tok::gap ) * 0.5f;
			ImGui::SetNextItemWidth( w );
			ImGui::SliderInt( "##a", a, lo, hi, fa );
			ImGui::SameLine( 0.f, tok::gap );
			ImGui::SetNextItemWidth( w );
			ImGui::SliderInt( "##b", b, lo, hi, fb );
			ImGui::PopID( );
		}

		inline void pair_f( const char* name, float* a, const char* fa, float* b, const char* fb, float lo, float hi )
		{
			label( name );
			ImGui::PushID( name );
			const float w = ( ImGui::GetContentRegionAvail( ).x - tok::gap ) * 0.5f;
			ImGui::SetNextItemWidth( w );
			ImGui::SliderFloat( "##a", a, lo, hi, fa );
			ImGui::SameLine( 0.f, tok::gap );
			ImGui::SetNextItemWidth( w );
			ImGui::SliderFloat( "##b", b, lo, hi, fb );
			ImGui::PopID( );
		}

		inline bool combo( const char* name, std::int32_t* v, const char* const* items, int n )
		{
			label( name );
			ImGui::PushID( name );
			ImGui::SetNextItemWidth( -1.f );
			const bool changed = ImGui::Combo( "##v", v, items, n );
			ImGui::PopID( );
			return changed;
		}

		// switch cell: label left, pill switch right. `w` = cell width (0 = fill the row)
		inline void toggle( const char* name, bool* v, float w = 0.f )
		{
			if ( w <= 0.f )
				w = ImGui::GetContentRegionAvail( ).x;

			ImGui::PushID( name );
			const ImVec2 p = ImGui::GetCursorScreenPos( );
			const ImVec2 sz { w, tok::row_h };

			if ( ImGui::InvisibleButton( "##t", sz ) )
				*v = !*v;

			auto* dl        = ImGui::GetWindowDrawList( );
			const bool hot  = ImGui::IsItemHovered( );
			dl->AddRectFilled( p, p + sz, ImGui::GetColorU32( hot ? tok::line2 : tok::card2 ), 6.f );
			dl->AddText( { p.x + 10.f, p.y + ( sz.y - ImGui::GetTextLineHeight( ) ) * 0.5f }, ImGui::GetColorU32( tok::tx ), name );

			const ImVec2 sw0 { p.x + sz.x - 40.f, p.y + ( sz.y - 14.f ) * 0.5f };
			dl->AddRectFilled( sw0, sw0 + ImVec2( 30.f, 14.f ), ImGui::GetColorU32( *v ? tok::ac : tok::line2 ), 7.f );
			dl->AddCircleFilled( { sw0.x + ( *v ? 23.f : 7.f ), sw0.y + 7.f }, 5.f, ImGui::GetColorU32( *v ? ImVec4( 1, 1, 1, 1 ) : tok::tx3 ) );
			ImGui::PopID( );
		}

		// segmented tab control, returns the selected index
		inline void tabs( const char* const* names, int n, int& sel )
		{
			const float w = ( ImGui::GetContentRegionAvail( ).x - tok::gap * 0.5f * ( n - 1 ) ) / static_cast<float>( n );

			for ( int i = 0; i < n; ++i )
			{
				const bool on = i == sel;

				if ( i )
					ImGui::SameLine( 0.f, tok::gap * 0.5f );

				ImGui::PushStyleColor( ImGuiCol_Button, on ? tok::ac : tok::panel );
				ImGui::PushStyleColor( ImGuiCol_ButtonHovered, on ? tok::ac : tok::line2 );
				ImGui::PushStyleColor( ImGuiCol_ButtonActive, tok::ac2 );
				ImGui::PushStyleColor( ImGuiCol_Text, on ? tok::tx : tok::tx2 );

				if ( ImGui::Button( names[ i ], { w, 28.f } ) )
					sel = i;

				ImGui::PopStyleColor( 4 );
			}
		}

		inline bool accent_button( const char* text, ImVec2 size )
		{
			ImGui::PushStyleColor( ImGuiCol_Button, tok::ac );
			ImGui::PushStyleColor( ImGuiCol_ButtonHovered, tok::ac2 );
			const bool pressed = ImGui::Button( text, size );
			ImGui::PopStyleColor( 2 );
			return pressed;
		}
	}

	// dark violet theme: flat frames, soft rounding, tight spacing
	static void apply_bura_style( )
	{
		ImGuiStyle& st = ImGui::GetStyle( );
		st.WindowRounding = 12.f;  st.ChildRounding = 10.f; st.FrameRounding = 6.f;
		st.PopupRounding  = 8.f;   st.GrabRounding  = 6.f;  st.TabRounding   = 8.f;
		st.ScrollbarRounding = 8.f; st.ScrollbarSize = 8.f;
		st.WindowPadding = { 14.f, 12.f }; st.FramePadding = { 8.f, 5.f };
		st.ItemSpacing   = { 8.f, 7.f };   st.WindowBorderSize = 1.f; st.FrameBorderSize = 0.f;
		st.ChildBorderSize = 1.f; st.GrabMinSize = 12.f;

		ImVec4* c = st.Colors;
		c[ ImGuiCol_Text ] = tok::tx;                 c[ ImGuiCol_TextDisabled ] = tok::tx3;
		c[ ImGuiCol_WindowBg ] = tok::bg;             c[ ImGuiCol_ChildBg ] = tok::card;
		c[ ImGuiCol_PopupBg ] = tok::card2;           c[ ImGuiCol_Border ] = tok::line;
		c[ ImGuiCol_FrameBg ] = tok::card2;           c[ ImGuiCol_FrameBgHovered ] = tok::line2;
		c[ ImGuiCol_FrameBgActive ] = tok::line2;
		c[ ImGuiCol_CheckMark ] = tok::ac2;
		c[ ImGuiCol_SliderGrab ] = tok::ac;           c[ ImGuiCol_SliderGrabActive ] = tok::ac2;
		c[ ImGuiCol_Button ] = tok::card2;            c[ ImGuiCol_ButtonHovered ] = tok::ac;
		c[ ImGuiCol_ButtonActive ] = tok::ac2;
		c[ ImGuiCol_Header ] = tok::rgb( 124, 92, 252, 0.28f );
		c[ ImGuiCol_HeaderHovered ] = tok::rgb( 124, 92, 252, 0.45f );
		c[ ImGuiCol_HeaderActive ] = tok::ac;
		c[ ImGuiCol_Separator ] = tok::line;          c[ ImGuiCol_ScrollbarBg ] = tok::card;
		c[ ImGuiCol_ScrollbarGrab ] = tok::line2;
	}

	namespace
	{
		constexpr UINT kTrayMsg = WM_APP + 1;
		constexpr wchar_t kClassName[ ] = L"BuraWindow";
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
			.lpszClassName = kClassName,
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
			static_cast<int>( screen_center.x ),
			static_cast<int>( screen_center.y ),
			static_cast<int>( size_.x ),
			static_cast<int>( size_.y ),
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

	void Window::hide_to_tray( )
	{
		if ( in_tray_ )
			return;

		nid_                  = {};
		nid_.cbSize           = sizeof( nid_ );
		nid_.hWnd             = hwnd_;
		nid_.uID              = 1;
		nid_.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		nid_.uCallbackMessage = kTrayMsg;
		nid_.hIcon            = LoadIconW( nullptr, IDI_APPLICATION );
		wcscpy_s( nid_.szTip, L"Bura" );

		Shell_NotifyIconW( NIM_ADD, &nid_ );
		ShowWindow( hwnd_, SW_HIDE );
		in_tray_ = true;
	}

	void Window::restore_from_tray( )
	{
		if ( !in_tray_ )
			return;

		Shell_NotifyIconW( NIM_DELETE, &nid_ );
		ShowWindow( hwnd_, SW_SHOW );
		SetForegroundWindow( hwnd_ );
		in_tray_ = false;
	}

	void Window::show_tray_menu( )
	{
		POINT pt {};
		GetCursorPos( &pt );

		const HMENU menu = CreatePopupMenu( );
		AppendMenuW( menu, MF_STRING, 1, L"Show Bura" );
		AppendMenuW( menu, MF_STRING, 2, L"Quit" );

		SetForegroundWindow( hwnd_ ); // required, or the menu does not close when clicking elsewhere
		const int cmd = TrackPopupMenu( menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr );
		DestroyMenu( menu );

		if ( cmd == 1 )
			restore_from_tray( );
		else if ( cmd == 2 )
			running_ = false;
	}

	Window::~Window( )
	{
		if ( in_tray_ )
			Shell_NotifyIconW( NIM_DELETE, &nid_ );

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

		// every change made in ImGui or the web UI is written to bura_config.json within ~1s
		std::string saved_json = nlohmann::json( *config ).dump( );
		auto last_save_check   = std::chrono::steady_clock::now( );

		while ( running_ )
		{
			if ( PeekMessageA( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
			{
				TranslateMessage( &msg );
				DispatchMessageA( &msg );

				if ( msg.message == WM_QUIT )
					running_ = false;
			}

			if ( const auto t = std::chrono::steady_clock::now( ); t - last_save_check > std::chrono::seconds( 1 ) )
			{
				last_save_check = t;

				if ( std::string cur = nlohmann::json( *config ).dump( ); cur != saved_json )
				{
					config->save_file( );
					saved_json = std::move( cur );
				}
			}

			if ( in_tray_ )
			{
				Sleep( 40 ); // hidden: no rendering, the aim and web threads keep running
				continue;
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
			     offset_.second <= 52 )
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

			// the web UI may have changed the COM port: follow it unless the field is being edited
			if ( !ImGui::IsAnyItemActive( ) )
				strncpy_s( com_buf, config->settings_.com_port.c_str( ), _TRUNCATE );

			ImGui::SetNextWindowSize( size_ );
			ImGui::SetNextWindowPos( { 0.f, 0.f } );

			constexpr auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
			                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

			ImGui::Begin( "Bura", nullptr, flags );

			// the debug tab turns this back on every frame it is visible
			service_hub_->get<DebugState>( )->enabled = false;

			// connection state, polled every 3s (reconnects when it drops)
			static constexpr double check_interval = 3.0;
			static double last_check_time          = -check_interval;
			static bool is_connected_cached        = false;

			if ( const double now = ImGui::GetTime( ); now - last_check_time >= check_interval )
			{
				is_connected_cached = mouse->is_connected( );
				last_check_time     = now;

				if ( !is_connected_cached )
					mouse->connect( );
			}

			// ---- header: logo, title, status, web, close ----
			{
				const float W  = ImGui::GetWindowWidth( );
				const float y  = ImGui::GetCursorPosY( );
				const ImVec2 p = ImGui::GetCursorScreenPos( );
				auto* dl       = ImGui::GetWindowDrawList( );

				dl->AddRectFilled( p, p + ImVec2( 30.f, 30.f ), ImGui::GetColorU32( tok::ac ), 9.f );
				const ImVec2 bs = ImGui::CalcTextSize( "B" );
				dl->AddText( { p.x + ( 30.f - bs.x ) * 0.5f, p.y + ( 30.f - bs.y ) * 0.5f }, IM_COL32_WHITE, "B" );

				ImGui::SetCursorScreenPos( { p.x + 40.f, p.y - 1.f } );
				ImGui::BeginGroup( );
				ImGui::TextColored( tok::tx, "Bura" );
				ImGui::TextColored( tok::tx3, "color tracking" );
				ImGui::EndGroup( );

				ImGui::SetCursorPos( { W - 14.f - 26.f, y + 2.f } );

				ImGui::PushStyleColor( ImGuiCol_ButtonHovered, tok::bad );

				if ( ImGui::Button( "x", { 26.f, 26.f } ) )
					close_prompt_ = true;

				ImGui::PopStyleColor( );

				ImGui::SetCursorPos( { W - 14.f - 26.f - 8.f - 44.f, y + 2.f } );

				if ( ImGui::Button( "Web", { 44.f, 26.f } ) )
					ShellExecuteA( nullptr, "open", "http://127.0.0.1:8765", nullptr, nullptr, SW_SHOWNORMAL );

				if ( ImGui::IsItemHovered( ) )
					ImGui::SetTooltip( "Open the web UI (127.0.0.1:8765)" );

				const char* st  = is_connected_cached ? "Connected" : "No input";
				const float tw  = ImGui::CalcTextSize( st ).x;
				const float sx  = W - 14.f - 26.f - 8.f - 44.f - 14.f - tw - 14.f;
				const ImVec2 sp = ImGui::GetWindowPos( );
				ImGui::SetCursorPos( { sx + 14.f, y + 6.f } );
				ImGui::TextColored( is_connected_cached ? tok::ok : tok::bad, "%s", st );
				dl->AddCircleFilled( { sp.x + sx + 4.f, sp.y + y + 2.f + 13.f }, 4.f,
				                     ImGui::GetColorU32( is_connected_cached ? tok::ok : tok::bad ) );

				ImGui::SetCursorPos( { ImGui::GetStyle( ).WindowPadding.x, y + 36.f } );
			}

			// ---- tabs ----
			static int tab = 0;
			static const char* const tab_names[ ] = { "Aimbot", "Trigger", "Debug", "Settings" };
			ui::tabs( tab_names, 4, tab );

			// ---- body (scrolls if the window is shrunk) ----
			ImGui::BeginChild( "##body", { 0.f, -26.f }, 1 );
			auto& ab = config->aimbot_;

			if ( tab == 0 )
			{
				static const char* const colors[ ] = { "Magenta", "Yellow", "Red" };

				ui::section( "TARGET" );
				ui::combo( "Color", &config->settings_.color, colors, 3 );
				ui::slider_i( "Tolerance", &ab.tolerance, 0, 60 );

				ui::section( "AIM" );
				ui::label( "Bind" );
				bind_system->render_keybind_button( "Aimbot", ab.bind, true, { -1.f, 0.f } );
				ui::pair_i( "FOV", &ab.fov_x, "X %dpx", &ab.fov_y, "Y %dpx", 20, 250 );
				ui::pair_i( "Offset", &ab.offset_x, "X %dpx", &ab.offset_y, "Y %dpx", -15, 15 );

				ui::section( "MOTION" );
				ui::pair_f( "Gain / Smooth", &ab.lock_gain, "G %.2f", &ab.smooth, "S %.2f", 0.1f, 5.f );
				ab.smooth = std::min( ab.smooth, 1.f );
				ui::pair_f( "Speed", &ab.speed_x, "X %.2f", &ab.speed_y, "Y %.2f", 0.2f, 3.f );

				ui::section( "TIMING" );
				ui::slider_i( "Hold", &ab.hold_pct, 0, 150, "%d%% of head" );
				ui::pair_i( "React / Settle", &ab.reaction_ms, "%dms", &ab.lock_settle_ms, "%dms", 0, 150 );
				ab.lock_settle_ms = std::clamp( ab.lock_settle_ms, 1, 30 );

				ui::section( "BEHAVIOUR" );
				const float cw = ( ImGui::GetContentRegionAvail( ).x - tok::gap ) * 0.5f;
				ui::toggle( "Auto aim", &ab.auto_aim, cw );
				ImGui::SameLine( 0.f, tok::gap );
				ui::toggle( "Legit", &ab.legit, cw );
				ui::toggle( "Anti-Strafe", &ab.anti_strafe, cw );
				ImGui::SameLine( 0.f, tok::gap );
				ui::toggle( "Humanize", &ab.humanize, cw );
			}
			else if ( tab == 1 )
			{
				ui::section( "TRIGGER" );
				ui::label( "Bind" );
				bind_system->render_keybind_button( "Trigger", config->trigger_.bind, true, { -1.f, 0.f } );
				ui::slider_i( "Scan area", &config->trigger_.scan_area, 1, 30, "%dpx" );
				ui::slider_i( "Delay", &config->trigger_.delay, 1, 300, "%dms" );
				ui::toggle( "Move check", &config->trigger_.move_check );
			}
			else if ( tab == 2 )
			{
				render_debug_tab( );
			}
			else
			{
				static const char* const backends[ ] = { "SendInput (local)", "Makcu (serial)", "Medius (capi dll)" };

				ui::section( "INPUT" );
				ui::combo( "Device", &config->settings_.input_backend, backends, 3 );

				if ( config->settings_.input_backend == 1 )
				{
					ui::label( "COM port" );
					ImGui::SetNextItemWidth( -1.f );

					if ( ImGui::InputText( "##com_port", com_buf, sizeof( com_buf ) ) )
						config->settings_.com_port = com_buf;
				}

				if ( ui::accent_button( "Apply input", { -1.f, 30.f } ) )
					mouse->configure( config->settings_.input_backend, config->settings_.com_port );

				ui::section( "CONFIG" );
				const float bw = ( ImGui::GetContentRegionAvail( ).x - tok::gap ) * 0.5f;

				if ( ImGui::Button( "Copy to clipboard", { bw, 28.f } ) )
					ImGui::SetClipboardText( config->export_to_str( ).c_str( ) );

				ImGui::SameLine( 0.f, tok::gap );

				if ( ImGui::Button( "Import from clipboard", { bw, 28.f } ) )
				{
					const auto result = config->import_from_str( ImGui::GetClipboardText( ) );

					if ( result )
					{
						strncpy_s( com_buf, config->settings_.com_port.c_str( ), _TRUNCATE );
						mouse->configure( config->settings_.input_backend, config->settings_.com_port );
					}
					else
					{
						MessageBoxA( nullptr, "Failed to import config!", " ", MB_OK | MB_ICONERROR );
					}
				}
			}

			ImGui::EndChild( );

			// ---- footer ----
			ImGui::TextColored( tok::tx3, "Input: %s", mouse->status( ).c_str( ) );

			// ---- close prompt ----
			if ( close_prompt_.exchange( false ) )
				ImGui::OpenPopup( "Close Bura" );

			ImGui::SetNextWindowPos( { size_.x * 0.5f, size_.y * 0.5f }, ImGuiCond_Always, { 0.5f, 0.5f } );

			if ( ImGui::BeginPopupModal( "Close Bura", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove ) )
			{
				ImGui::TextColored( tok::tx2, "Keep Bura running in the system tray?" );
				ImGui::Dummy( { 0.f, 4.f } );

				if ( ui::accent_button( "Minimize to tray", { 200.f, 30.f } ) )
				{
					ImGui::CloseCurrentPopup( );
					hide_to_tray( );
				}

				if ( ImGui::Button( "Quit", { 98.f, 28.f } ) )
					close( );

				ImGui::SameLine( 0.f, tok::gap );

				if ( ImGui::Button( "Cancel", { 98.f, 28.f } ) )
					ImGui::CloseCurrentPopup( );

				ImGui::EndPopup( );
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

		config->save_file( );
	}

	LRESULT CALLBACK Window::wnd_proc( const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam )
	{
		if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wparam, lparam ) )
			return true;

		if ( msg == WM_NCCREATE )
			SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( reinterpret_cast<CREATESTRUCTW*>( lparam )->lpCreateParams ) );

		auto* self = reinterpret_cast<Window*>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );

		switch ( msg )
		{
			case kTrayMsg:
			{
				if ( self && ( LOWORD( lparam ) == WM_LBUTTONUP || LOWORD( lparam ) == WM_LBUTTONDBLCLK ) )
					self->restore_from_tray( );
				else if ( self && LOWORD( lparam ) == WM_RBUTTONUP )
					self->show_tray_menu( );

				return 0;
			}
			case WM_CLOSE: // Alt+F4 / taskbar close: ask instead of quitting
			{
				if ( self )
					self->request_close( );
				else
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
