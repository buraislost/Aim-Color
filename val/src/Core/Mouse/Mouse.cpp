#include "Stdafx.hpp"
#include "Core/Mouse/Mouse.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string_view>

// medius backend is compiled in only when medius.h is on the include path
// (drop it into thirdparty/include/); the dll is loaded at runtime, so no .lib is linked.
#if __has_include( <medius.h> )
	// objbase.h defines `interface` as `struct`, and medius.h has a field named `interface`
	#pragma push_macro( "interface" )
	#undef interface
	#include <medius.h>
	#pragma pop_macro( "interface" )
	#define VAL_HAS_MEDIUS 1
#else
	#define VAL_HAS_MEDIUS 0
#endif

namespace Core
{
	namespace
	{
		// stock MAKCU firmware speaks ASCII (km.move(x,y) / km.click(1)) over its CH343 serial port.
		// 115200 works without the vendor baud-upgrade frame; 4 Mbaud needs that frame, not implemented.
		constexpr DWORD MAKCU_BAUD = 115200;

		// how long the medius left button stays down for a click
		constexpr DWORD MEDIUS_CLICK_HOLD_MS = 10;

		void send_mouse( const DWORD flags, const LONG dx = 0, const LONG dy = 0 )
		{
			INPUT input   = {};
			input.type    = INPUT_MOUSE;
			input.mi.dx    = dx;
			input.mi.dy    = dy;
			input.mi.dwFlags = flags;

			SendInput( 1, &input, sizeof( INPUT ) );
		}

#if VAL_HAS_MEDIUS
		// pointer types come from the header's own declarations
		struct MediusApi
		{
			decltype( &medius_abi_version )         abi_version  = nullptr;
			decltype( &medius_device_find_mouse_box ) find       = nullptr;
			decltype( &medius_device_free )         free_device  = nullptr;
			decltype( &medius_device_move_rel )     move_rel     = nullptr;
			decltype( &medius_usage_button )        usage_button = nullptr;
			decltype( &medius_device_press )        press        = nullptr;
			decltype( &medius_device_soft_release ) soft_release = nullptr;
		} g_medius;
#endif
	}

	Mouse::Mouse( ) = default;

	Mouse::~Mouse( )
	{
		std::lock_guard lock( mutex_ );

		close_port_locked( );
		close_medius_locked( );

		if ( medius_dll_ )
		{
			FreeLibrary( medius_dll_ );
			medius_dll_ = nullptr;
		}
	}

	void Mouse::configure( const std::int32_t backend, const std::string& port )
	{
		std::lock_guard lock( mutex_ );

		close_port_locked( );
		close_medius_locked( );

		switch ( backend )
		{
			case static_cast<std::int32_t>( Backend::Makcu ):
				backend_ = Backend::Makcu;
				break;
			case static_cast<std::int32_t>( Backend::Medius ):
				backend_ = Backend::Medius;
				break;
			default:
				backend_ = Backend::SendInput;
				break;
		}

		port_name_ = port;

		if ( backend_ == Backend::Makcu )
			open_port_locked( );
		else if ( backend_ == Backend::Medius )
			open_medius_locked( );
	}

	// SendInput: nothing to do. Makcu / Medius: (re)open if closed.
	void Mouse::connect( ) const
	{
		std::lock_guard lock( mutex_ );

		if ( backend_ == Backend::Makcu && port_ == INVALID_HANDLE_VALUE )
			open_port_locked( );
		else if ( backend_ == Backend::Medius && !medius_dev_ )
			open_medius_locked( );
	}

	bool Mouse::is_connected( ) const
	{
		std::lock_guard lock( mutex_ );

		switch ( backend_ )
		{
			case Backend::Makcu:
				return port_ != INVALID_HANDLE_VALUE;
			case Backend::Medius:
				return medius_dev_ != nullptr;
			default:
				return true;
		}
	}

	void Mouse::left_click( ) const
	{
		std::unique_lock lock( mutex_ );

		if ( backend_ == Backend::Makcu )
		{
			constexpr std::string_view cmd = "km.click(1)\r\n";
			write_locked( cmd.data( ), cmd.size( ) );
			return;
		}

		if ( backend_ == Backend::Medius )
		{
#if VAL_HAS_MEDIUS
			if ( !medius_dev_ )
				return;

			auto* dev         = static_cast<MediusDevice*>( medius_dev_ );
			const auto usage  = g_medius.usage_button( MEDIUS_BUTTON_LEFT );

			if ( g_medius.press( dev, usage ) != MEDIUS_STATUS_OK )
			{
				close_medius_locked( );
				return;
			}

			// release outside the lock so moves are not held up for the whole click
			lock.unlock( );
			Sleep( MEDIUS_CLICK_HOLD_MS );
			lock.lock( );

			if ( medius_dev_ && g_medius.soft_release( static_cast<MediusDevice*>( medius_dev_ ), usage ) != MEDIUS_STATUS_OK )
				close_medius_locked( );
#endif
			return;
		}

		send_mouse( MOUSEEVENTF_LEFTDOWN );
		send_mouse( MOUSEEVENTF_LEFTUP );
	}

	// Makcu / Medius selected but not connected: the move is dropped on purpose,
	// falling back to SendInput would silently switch the input path.
	void Mouse::send_move( const std::int32_t dx, const std::int32_t dy ) const
	{
		std::lock_guard lock( mutex_ );

		constexpr std::int32_t lim = 32767;

		if ( backend_ == Backend::Makcu )
		{
			char buf[ 40 ];
			const int len = std::snprintf( buf, sizeof( buf ), "km.move(%d,%d)\r\n",
			                               std::clamp( dx, -lim, lim ), std::clamp( dy, -lim, lim ) );

			if ( len > 0 )
				write_locked( buf, static_cast<std::size_t>( len ) );

			return;
		}

		if ( backend_ == Backend::Medius )
		{
#if VAL_HAS_MEDIUS
			if ( !medius_dev_ )
				return;

			const auto status = g_medius.move_rel( static_cast<MediusDevice*>( medius_dev_ ),
			                                       static_cast<std::int16_t>( std::clamp( dx, -lim, lim ) ),
			                                       static_cast<std::int16_t>( std::clamp( dy, -lim, lim ) ) );

			// a failed call closes the device, so is_connected() flips and the UI loop retries connect()
			if ( status != MEDIUS_STATUS_OK )
				close_medius_locked( );
#endif
			return;
		}

		send_mouse( MOUSEEVENTF_MOVE, dx, dy );
	}

	void Mouse::move_raw( const std::int32_t dx, const std::int32_t dy ) const
	{
		if ( dx != 0 || dy != 0 )
			send_move( dx, dy );
	}

	// relative move along quadratic bezier from (0,0) through (ctrl_x, ctrl_y) to (x, y)
	// one report per segment, sleep 1ms between segments; rounding error is carried so total delta is exact
	void Mouse::move( const std::int32_t x,
	                  const std::int32_t y,
	                  const std::uint32_t segments,
	                  const std::int32_t ctrl_x,
	                  const std::int32_t ctrl_y ) const
	{
		const std::uint32_t steps = std::max<std::uint32_t>( segments, 1u );

		std::int32_t sent_x = 0;
		std::int32_t sent_y = 0;

		for ( std::uint32_t i = 1; i <= steps; ++i )
		{
			const double t = static_cast<double>( i ) / steps;
			const double u = 1.0 - t;

			const double px = 2.0 * u * t * ctrl_x + t * t * x;
			const double py = 2.0 * u * t * ctrl_y + t * t * y;

			const auto cur_x = static_cast<std::int32_t>( std::lround( px ) );
			const auto cur_y = static_cast<std::int32_t>( std::lround( py ) );

			const std::int32_t dx = cur_x - sent_x;
			const std::int32_t dy = cur_y - sent_y;

			if ( dx != 0 || dy != 0 )
				send_move( dx, dy );

			sent_x = cur_x;
			sent_y = cur_y;

			if ( i < steps )
				Sleep( 1 );
		}
	}

	std::string Mouse::status( ) const
	{
		std::lock_guard lock( mutex_ );
		return status_;
	}

	namespace
	{
		// every COM port currently present, from HKLM\HARDWARE\DEVICEMAP\SERIALCOMM
		std::vector<std::string> list_com_ports( )
		{
			std::vector<std::string> ports;
			HKEY key = nullptr;

			if ( RegOpenKeyExA( HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key ) != ERROR_SUCCESS )
				return ports;

			for ( DWORD i = 0;; ++i )
			{
				char  name[ 256 ] = {};
				char  data[ 64 ]  = {};
				DWORD name_len    = sizeof( name );
				DWORD data_len    = sizeof( data ) - 1;
				DWORD type        = 0;

				if ( RegEnumValueA( key, i, name, &name_len, nullptr, &type, reinterpret_cast<BYTE*>( data ), &data_len ) != ERROR_SUCCESS )
					break;

				if ( type == REG_SZ )
					ports.emplace_back( data );
			}

			RegCloseKey( key );
			return ports;
		}
	}

	// configured port first; if it does not answer as a Makcu, probe every other COM port.
	// stock MAKCU exposes two ports and only one (the CH343 one) takes km. commands, and the
	// COM number changes between machines / usb slots, so a fixed name is fragile.
	bool Mouse::open_port_locked( ) const
	{
		std::vector<std::string> candidates;

		if ( !port_name_.empty( ) && port_name_ != "auto" )
			candidates.push_back( port_name_ );

		for ( auto& p : list_com_ports( ) )
			if ( std::find( candidates.begin( ), candidates.end( ), p ) == candidates.end( ) )
				candidates.push_back( std::move( p ) );

		if ( candidates.empty( ) )
		{
			status_ = "No COM port found";
			OutputDebugStringA( "[Makcu] no COM port present\n" );
			return false;
		}

		std::string log;

		for ( const auto& name : candidates )
		{
			if ( try_open_port_locked( name ) )
			{
				status_ = "Makcu on " + name;
				OutputDebugStringA( ( "[Makcu] connected on " + name + "\n" ).c_str( ) );
				return true;
			}

			log += ( log.empty( ) ? "" : "; " ) + name + ": " + status_;
		}

		status_ = log;
		OutputDebugStringA( ( "[Makcu] failed -> " + log + "\n" ).c_str( ) );
		return false;
	}

	bool Mouse::try_open_port_locked( const std::string& name ) const
	{
		const std::string path = "\\\\.\\" + name;

		const HANDLE handle = CreateFileA( path.c_str( ), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );

		if ( handle == INVALID_HANDLE_VALUE )
		{
			const DWORD err = GetLastError( );
			status_ = err == ERROR_ACCESS_DENIED ? "access denied (port in use)" : "open failed (err " + std::to_string( err ) + ")";
			return false;
		}

		DCB dcb       = {};
		dcb.DCBlength = sizeof( dcb );

		if ( !GetCommState( handle, &dcb ) )
		{
			CloseHandle( handle );
			status_ = "GetCommState failed";
			return false;
		}

		dcb.BaudRate      = MAKCU_BAUD;
		dcb.ByteSize      = 8;
		dcb.Parity        = NOPARITY;
		dcb.StopBits      = ONESTOPBIT;
		dcb.fBinary       = TRUE;
		dcb.fParity       = FALSE;
		dcb.fOutxCtsFlow  = FALSE;
		dcb.fOutxDsrFlow  = FALSE;
		dcb.fOutX         = FALSE;
		dcb.fInX          = FALSE;
		dcb.fDtrControl   = DTR_CONTROL_ENABLE;
		dcb.fRtsControl   = RTS_CONTROL_DISABLE;

		COMMTIMEOUTS timeouts         = {};
		timeouts.ReadIntervalTimeout  = 20;
		timeouts.ReadTotalTimeoutConstant = 150;
		timeouts.WriteTotalTimeoutConstant = 50;

		if ( !SetCommState( handle, &dcb ) || !SetCommTimeouts( handle, &timeouts ) )
		{
			CloseHandle( handle );
			status_ = "SetCommState/Timeouts failed";
			return false;
		}

		PurgeComm( handle, PURGE_RXCLEAR | PURGE_TXCLEAR );

		// handshake: a Makcu answers km.version() with "km.MAKCU\r\n>>> ".
		// read in a loop (reply may arrive split) until "km." shows up or ~600ms pass.
		constexpr std::string_view probe = "km.version()\r\n";

		DWORD written = 0;

		if ( !WriteFile( handle, probe.data( ), static_cast<DWORD>( probe.size( ) ), &written, nullptr ) ||
		     written != probe.size( ) )
		{
			CloseHandle( handle );
			status_ = "write failed";
			return false;
		}

		std::string reply;

		for ( int attempt = 0; attempt < 4 && reply.find( "km." ) == std::string::npos; ++attempt )
		{
			char  buf[ 128 ] = {};
			DWORD got        = 0;

			if ( ReadFile( handle, buf, sizeof( buf ) - 1, &got, nullptr ) && got > 0 )
				reply.append( buf, got );
		}

		if ( reply.find( "km." ) == std::string::npos )
		{
			CloseHandle( handle );
			status_ = reply.empty( ) ? "no reply (not a Makcu cmd port)" : "unexpected reply";
			return false;
		}

		port_name_ = name; // remember the port that actually worked
		port_      = handle;
		return true;
	}

	void Mouse::close_port_locked( ) const
	{
		if ( port_ != INVALID_HANDLE_VALUE )
		{
			CloseHandle( port_ );
			port_ = INVALID_HANDLE_VALUE;
		}
	}

	// a failed write closes the port, so is_connected() flips and the UI loop retries connect()
	bool Mouse::write_locked( const char* data, const std::size_t len ) const
	{
		if ( port_ == INVALID_HANDLE_VALUE )
			return false;

		// replies are never read, keep the RX queue from filling up
		PurgeComm( port_, PURGE_RXCLEAR );

		DWORD written = 0;

		if ( !WriteFile( port_, data, static_cast<DWORD>( len ), &written, nullptr ) || written != len )
		{
			close_port_locked( );
			return false;
		}

		return true;
	}

	// medius_capi.dll is looked up next to the exe / on PATH. without medius.h at build time this is a no-op.
	bool Mouse::open_medius_locked( ) const
	{
#if VAL_HAS_MEDIUS
		if ( medius_dev_ )
			return true;

		if ( !medius_dll_ )
		{
			const HMODULE dll = LoadLibraryA( "medius_capi.dll" );

			if ( !dll )
				return false;

			auto load = [ & ]<typename T>( T& fn, const char* name )
			{
				fn = reinterpret_cast<T>( GetProcAddress( dll, name ) );
				return fn != nullptr;
			};

			MediusApi api;

			const bool ok = load( api.abi_version, "medius_abi_version" ) &&
			                load( api.find, "medius_device_find_mouse_box" ) &&
			                load( api.free_device, "medius_device_free" ) &&
			                load( api.move_rel, "medius_device_move_rel" ) &&
			                load( api.usage_button, "medius_usage_button" ) &&
			                load( api.press, "medius_device_press" ) &&
			                load( api.soft_release, "medius_device_soft_release" );

			// header and library must agree on struct layout before anything else is called
			if ( !ok || api.abi_version( ) != MEDIUS_ABI_VERSION )
			{
				FreeLibrary( dll );
				return false;
			}

			g_medius    = api;
			medius_dll_ = dll;
		}

		MediusDevice* dev = nullptr;

		// the handshake rejects a box whose protocol version does not match the library
		if ( g_medius.find( &dev ) != MEDIUS_STATUS_OK || !dev )
			return false;

		medius_dev_ = dev;
		return true;
#else
		return false;
#endif
	}

	void Mouse::close_medius_locked( ) const
	{
#if VAL_HAS_MEDIUS
		if ( medius_dev_ )
		{
			g_medius.free_device( static_cast<MediusDevice*>( medius_dev_ ) );
			medius_dev_ = nullptr;
		}
#endif
	}
}
