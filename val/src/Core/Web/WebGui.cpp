// no precompiled header: httplib must see <winsock2.h> before <windows.h> (Stdafx.hpp pulls windows.h in)
#include <httplib.h>

#include "Stdafx.hpp"
#include "Core/Web/WebGui.hpp"
#include "Core/Web/WebPage.hpp"
#include "Core/Config/Config.hpp"
#include "Core/Features/DebugState.hpp"
#include "Core/Mouse/Mouse.hpp"

#include <algorithm>
#include <atomic>

namespace Core::Web
{
	using json = nlohmann::json;

	namespace
	{
		std::string key_name( int vk )
		{
			switch ( vk )
			{
				case 0: return "";
				case VK_LBUTTON: return "Mouse1";
				case VK_RBUTTON: return "Mouse2";
				case VK_MBUTTON: return "Mouse3";
				case VK_XBUTTON1: return "Mouse4";
				case VK_XBUTTON2: return "Mouse5";
				default: break;
			}

			char buf[ 64 ] {};
			const LONG scan = static_cast<LONG>( MapVirtualKeyA( static_cast<UINT>( vk ), MAPVK_VK_TO_VSC ) ) << 16;

			if ( GetKeyNameTextA( scan, buf, sizeof( buf ) ) > 0 )
				return buf;

			return "VK " + std::to_string( vk );
		}

		template<typename T>
		void take( const json& j, const char* key, T& out, T lo, T hi )
		{
			const auto it = j.find( key );

			if ( it == j.end( ) || !it->is_number( ) )
				return;

			out = std::clamp( it->get<T>( ), lo, hi );
		}

		void take_bool( const json& j, const char* key, bool& out )
		{
			const auto it = j.find( key );

			if ( it != j.end( ) && it->is_boolean( ) )
				out = it->get<bool>( );
		}

		void take_mode( const json& j, KeyBind& b )
		{
			const auto it = j.find( "bind_mode" );

			if ( it != j.end( ) && it->is_number_integer( ) )
				b.mode = it->get<int>( ) == 1 ? KeyMode::Toggle : KeyMode::HoldOn;
		}

		// waits for the next key / mouse press (up to 6s). Esc clears the bind. returns -1 on timeout
		int wait_for_key( )
		{
			for ( int i = 0; i < 6000 / 15; ++i )
			{
				for ( int vk = 1; vk < 255; ++vk )
				{
					if ( GetAsyncKeyState( vk ) & 1 )
						return vk == VK_ESCAPE ? 0 : vk;
				}

				Sleep( 15 );
			}

			return -1;
		}
	}

	struct WebGui::Impl
	{
		httplib::Server server;
		unsigned short port = 8765;
	};

	WebGui::WebGui( ServiceHub& hub, unsigned short port )
		: impl_( std::make_unique<Impl>( ) )
	{
		impl_->port = port;

		const auto config = hub.get<Config>( );
		const auto mouse  = hub.get<Mouse>( );
		const auto debug  = hub.get<DebugState>( );
		auto& srv         = impl_->server;

		srv.Get( "/", [ ]( const httplib::Request&, httplib::Response& res )
		{
			std::string page = kBody;
			page += kCss;
			page += kBody2;
			page += kJs;
			page += kEnd;
			res.set_content( page, "text/html; charset=utf-8" );
		} );

		srv.Get( "/api/ping", [ ]( const httplib::Request&, httplib::Response& res ) { res.set_content( "{\"ok\":true}", "application/json" ); } );

		srv.Get( "/api/config", [ = ]( const httplib::Request&, httplib::Response& res )
		{
			json j     = *config;
			j[ "_names" ] = { { "aimbot", key_name( config->aimbot_.bind.key_code ) },
			                  { "trigger", key_name( config->trigger_.bind.key_code ) } };
			j[ "_mouse" ] = mouse->status( );
			res.set_content( j.dump( ), "application/json" );
		} );

		srv.Post( "/api/config", [ = ]( const httplib::Request& req, httplib::Response& res )
		{
			const json j = json::parse( req.body, nullptr, false );

			if ( j.is_discarded( ) || !j.is_object( ) )
			{
				res.status = 400;
				res.set_content( "{\"ok\":false}", "application/json" );
				return;
			}

			if ( const auto a = j.find( "aimbot_" ); a != j.end( ) && a->is_object( ) )
			{
				auto& c = config->aimbot_;
				take<std::int32_t>( *a, "fov_x", c.fov_x, 20, 250 );
				take<std::int32_t>( *a, "fov_y", c.fov_y, 20, 250 );
				take<std::int32_t>( *a, "offset_x", c.offset_x, -15, 15 );
				take<std::int32_t>( *a, "offset_y", c.offset_y, -15, 15 );
				take<std::int32_t>( *a, "hold_pct", c.hold_pct, 0, 150 );
				take<float>( *a, "lock_gain", c.lock_gain, 0.1f, 5.f );
				take<float>( *a, "smooth", c.smooth, 0.1f, 1.f );
				take<std::int32_t>( *a, "lock_settle_ms", c.lock_settle_ms, 1, 30 );
				take<std::int32_t>( *a, "reaction_ms", c.reaction_ms, 0, 150 );
				take<float>( *a, "speed_x", c.speed_x, 0.2f, 3.f );
				take<float>( *a, "speed_y", c.speed_y, 0.2f, 3.f );
				take<std::int32_t>( *a, "tolerance", c.tolerance, 0, 60 );
				take_bool( *a, "anti_strafe", c.anti_strafe );
				take_bool( *a, "humanize", c.humanize );
				take_bool( *a, "legit", c.legit );
				take_bool( *a, "y_axis", c.y_axis );
				take<std::int32_t>( *a, "y_limit", c.y_limit, 0, 30 );
				take_bool( *a, "auto_aim", c.auto_aim );
				take_mode( *a, c.bind );
			}

			if ( const auto t = j.find( "trigger_" ); t != j.end( ) && t->is_object( ) )
			{
				auto& c = config->trigger_;
				take<std::int32_t>( *t, "scan_area", c.scan_area, 1, 30 );
				take<std::int32_t>( *t, "delay", c.delay, 1, 300 );
				take_bool( *t, "move_check", c.move_check );
				take_mode( *t, c.bind );
			}

			if ( const auto s = j.find( "settings_" ); s != j.end( ) && s->is_object( ) )
			{
				auto& c = config->settings_;
				take<std::int32_t>( *s, "color", c.color, 0, 2 );
				take<std::int32_t>( *s, "input_backend", c.input_backend, 0, 2 );

				if ( const auto p = s->find( "com_port" ); p != s->end( ) && p->is_string( ) )
					c.com_port = p->get<std::string>( ).substr( 0, 16 );
			}

			res.set_content( "{\"ok\":true}", "application/json" );
		} );

		srv.Post( "/api/bind", [ = ]( const httplib::Request& req, httplib::Response& res )
		{
			const json j     = json::parse( req.body, nullptr, false );
			const auto name  = j.is_object( ) ? j.value( "name", std::string { } ) : std::string { };
			KeyBind* bind    = name == "aimbot" ? &config->aimbot_.bind : name == "trigger" ? &config->trigger_.bind : nullptr;

			if ( !bind )
			{
				res.status = 400;
				res.set_content( "{\"ok\":false}", "application/json" );
				return;
			}

			const int vk = wait_for_key( );

			if ( vk >= 0 )
			{
				bind->key_code   = vk;
				bind->is_toggled = false;
			}

			res.set_content( json { { "ok", vk >= 0 }, { "name", key_name( bind->key_code ) } }.dump( ), "application/json" );
		} );

		srv.Post( "/api/input/apply", [ = ]( const httplib::Request&, httplib::Response& res )
		{
			mouse->configure( config->settings_.input_backend, config->settings_.com_port );
			res.set_content( json { { "ok", true }, { "status", mouse->status( ) } }.dump( ), "application/json" );
		} );

		srv.Get( "/api/debug", [ = ]( const httplib::Request&, httplib::Response& res )
		{
			debug->web_touch( );
			const auto i = debug->info( );
			res.set_content( json {
				{ "captured", i.captured }, { "cap_w", i.cap_w }, { "cap_h", i.cap_h }, { "lit", i.lit },
				{ "blobs", i.blobs }, { "big_blobs", i.big_blobs }, { "found", i.found }, { "on_target", i.on_target },
				{ "aim_active", i.aim_active }, { "trigger_active", i.trigger_active }, { "dist", i.dist },
				{ "mean_bgr", { i.mean_bgr[ 0 ], i.mean_bgr[ 1 ], i.mean_bgr[ 2 ] } },
				{ "center_hsv", { i.center_hsv[ 0 ], i.center_hsv[ 1 ], i.center_hsv[ 2 ] } },
				{ "frames", i.frames } }.dump( ), "application/json" );
		} );

		srv.Get( "/api/preview.png", [ = ]( const httplib::Request&, httplib::Response& res )
		{
			debug->web_touch( );
			cv::Mat rgba;

			if ( !debug->preview( rgba ) )
			{
				res.status = 204;
				return;
			}

			cv::Mat bgra;
			cv::cvtColor( rgba, bgra, cv::COLOR_RGBA2BGRA );
			std::vector<uchar> png;
			cv::imencode( ".png", bgra, png );
			res.set_header( "Cache-Control", "no-store" );
			res.set_content( std::string( png.begin( ), png.end( ) ), "image/png" );
		} );

		thread_ = std::thread( [ this ] { impl_->server.listen( "127.0.0.1", impl_->port ); } );
	}

	WebGui::~WebGui( )
	{
		impl_->server.stop( );

		if ( thread_.joinable( ) )
			thread_.join( );
	}
}
