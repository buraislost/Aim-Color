// ui/DebugTab.cpp — Debug tab: pipeline status rows, verdict, live mask preview
// Depends: ui/Window.hpp, DebugState, Mouse, Config

// ===== Includes =====
#include "Stdafx.hpp"
#include "Core/UI/Window.hpp"
#include "Core/UI/Theme.hpp"
#include "Core/Config/Config.hpp"
#include "Core/Mouse/Mouse.hpp"
#include "Core/Features/DebugState.hpp"

namespace Core::UI
{
	namespace th = theme;
	using Info   = DebugState::Info;

	// ===== Constants =====
	namespace
	{
		constexpr float kMaxPreviewScale = 3.f;
		constexpr int kTestMoveCounts    = 40;

		// ===== Internal helpers =====
		bool frame_is_black( const Info& i )
		{
			return !( i.mean_bgr[ 0 ] || i.mean_bgr[ 1 ] || i.mean_bgr[ 2 ] );
		}

		void status_row( const char* label, const bool good, const char* text )
		{
			ImGui::TextColored( good ? th::kOk : th::kBad, good ? "OK" : "!!" );
			ImGui::SameLine( th::kSpace4 + th::kSpace1 );
			ImGui::TextColored( th::kMuted, "%s", label );
			ImGui::SameLine( 128.f );
			ImGui::TextUnformatted( text );
		}

		void capture_rows( const Info& i )
		{
			static const char* const colors[ ] = { "Magenta", "Yellow", "Red" };
			char buf[ 128 ];

			snprintf( buf, sizeof( buf ), "%dx%d  BGR(%d,%d,%d)%s", i.cap_w, i.cap_h, i.mean_bgr[ 0 ], i.mean_bgr[ 1 ],
			          i.mean_bgr[ 2 ], !i.captured ? "  no frame" : ( frame_is_black( i ) ? "  black frame" : "" ) );
			status_row( "Capture", i.captured && !frame_is_black( i ), buf );

			snprintf( buf, sizeof( buf ), "%d lit  (%s)", i.lit, colors[ std::clamp( i.color, 0, 2 ) ] );
			status_row( "Mask", i.lit > 0, buf );

			snprintf( buf, sizeof( buf ), "%d total, %d big enough", i.blobs, i.big_blobs );
			status_row( "Blobs", i.big_blobs > 0, buf );
		}

		void target_rows( const Info& i, const Mouse& mouse, const Config& config )
		{
			char buf[ 128 ];

			snprintf( buf, sizeof( buf ), "head(%d,%d)  dist %.1fpx", i.head.x, i.head.y, i.dist );
			status_row( "Target", i.found, buf );

			snprintf( buf, sizeof( buf ), "lock %d  on target %d", i.has_lock ? 1 : 0, i.on_target ? 1 : 0 );
			status_row( "Lock", i.has_lock, buf );

			status_row( "Aim", i.aim_active, i.aim_active ? "armed" : "idle: hold the aim key or enable Auto aim" );
			status_row( "Trigger", i.trigger_active, i.trigger_active ? "armed" : "idle" );

			snprintf( buf, sizeof( buf ), "backend %d  %s", config.settings_.input_backend, mouse.status( ).c_str( ) );
			status_row( "Input", mouse.is_connected( ), buf );
		}

		// first broken link of the chain, or "Detection OK"
		const char* verdict_for( const Info& i, const Mouse& mouse )
		{
			if ( !i.captured ) { return "Screen capture returns nothing"; }
			if ( frame_is_black( i ) ) { return "Black frame: use borderless windowed, not exclusive fullscreen"; }
			if ( i.lit == 0 ) { return "No pixel matches the color: check Color, or raise FOV"; }
			if ( !i.found ) { return "Color found but every blob is below the min area"; }
			if ( !i.aim_active && !i.trigger_active ) { return "Target seen, but no bind is held"; }
			if ( !mouse.is_connected( ) ) { return "Target seen and armed, but the input device is disconnected"; }
			return "Detection OK";
		}
	}

	// ===== Public API (Window members) =====
	void Window::upload_preview( const cv::Mat& rgba )
	{
		auto* device  = d3d_device_->get_device( );
		auto* context = d3d_device_->get_device_context( );

		if ( !preview_tex_ || preview_w_ != rgba.cols || preview_h_ != rgba.rows ) {
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
			     FAILED( device->CreateShaderResourceView( preview_tex_.Get( ), nullptr, &preview_srv_ ) ) ) {
				preview_srv_.Reset( );
				preview_tex_.Reset( );
				return;
			}

			preview_w_ = rgba.cols;
			preview_h_ = rgba.rows;
		}

		D3D11_MAPPED_SUBRESOURCE mapped = {};

		if ( FAILED( context->Map( preview_tex_.Get( ), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped ) ) ) {
			return;
		}

		for ( int y = 0; y < rgba.rows; ++y ) {
			std::memcpy( static_cast<std::uint8_t*>( mapped.pData ) + static_cast<std::size_t>( y ) * mapped.RowPitch,
			             rgba.ptr( y ), static_cast<std::size_t>( rgba.cols ) * 4 );
		}

		context->Unmap( preview_tex_.Get( ), 0 );
	}

	// while this tab is open ColorBot scans even with no bind held (it never moves unless the real bind is active)
	void Window::render_debug_tab( )
	{
		const auto debug  = service_hub_->get<DebugState>( );
		const auto mouse  = service_hub_->get<Mouse>( );
		const auto config = service_hub_->get<Config>( );

		debug->enabled  = true;
		const Info info = debug->info( );

		capture_rows( info );
		target_rows( info, *mouse, *config );

		ImGui::Dummy( { 0.f, 0.f } );
		ImGui::TextColored( th::kWarn, "%s", verdict_for( info, *mouse ) );
		ImGui::Text( "HSV under crosshair  H%d S%d V%d   frames %llu", info.center_hsv[ 0 ], info.center_hsv[ 1 ],
		             info.center_hsv[ 2 ], static_cast<unsigned long long>( info.frames ) );

		const float cell = ( ImGui::GetContentRegionAvail( ).x - th::kSpace2 ) * 0.5f;

		if ( ImGui::Button( "Test move right", { cell, th::kRowHeight } ) ) {
			mouse->move_raw( kTestMoveCounts, 0 );
		}

		ImGui::SameLine( 0.f, th::kSpace2 );

		if ( ImGui::Button( "Test move left", { cell, th::kRowHeight } ) ) {
			mouse->move_raw( -kTestMoveCounts, 0 );
		}

		cv::Mat preview;

		if ( debug->preview( preview ) ) {
			upload_preview( preview );
		}

		if ( !preview_srv_ ) {
			return;
		}

		ImGui::TextColored( th::kMuted, "left: capture   right: mask" );
		const float scale = std::clamp( ImGui::GetContentRegionAvail( ).x / static_cast<float>( preview_w_ ), 1.f, kMaxPreviewScale );
		ImGui::Image( reinterpret_cast<ImTextureID>( preview_srv_.Get( ) ), { preview_w_ * scale, preview_h_ * scale } );
	}
}
