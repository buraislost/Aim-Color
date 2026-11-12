#pragma once

#include <mutex>
#include <string>

namespace Core
{
	class Mouse
	{
	public:
		enum class Backend : std::int32_t
		{
			SendInput = 0,
			Makcu     = 1, // stock MAKCU firmware, ASCII over serial
			Medius    = 2  // medius firmware, through medius_capi.dll
		};

		Mouse( );
		~Mouse( );

		// switch backend. for Makcu, port is a "COM3"-style name. thread-safe.
		void configure( std::int32_t backend, const std::string& port );

		void connect( ) const;

		void left_click( ) const;

		// one report, no interpolation, no sleep
		void move_raw( std::int32_t dx, std::int32_t dy ) const;

		void move( std::int32_t, std::int32_t, std::uint32_t, std::int32_t, std::int32_t ) const;

		[[nodiscard]] bool is_connected( ) const;

		// last connect result / error, for the UI ("Connected on COM4", "COM3: access denied", ...)
		[[nodiscard]] std::string status( ) const;

	private:
		mutable std::string status_;
		bool try_open_port_locked( const std::string& name ) const;
		mutable std::mutex mutex_;
		Backend backend_ = Backend::SendInput;
		mutable std::string port_name_; // updated to the port that actually answered

		// Makcu
		mutable HANDLE port_ = INVALID_HANDLE_VALUE;

		// Medius: MediusDevice* and the loaded medius_capi.dll, opaque here so this header
		// does not depend on medius.h
		mutable void*   medius_dev_ = nullptr;
		mutable HMODULE medius_dll_ = nullptr;

		void send_move( std::int32_t dx, std::int32_t dy ) const;

		bool open_port_locked( ) const;
		void close_port_locked( ) const;
		bool write_locked( const char* data, std::size_t len ) const;

		bool open_medius_locked( ) const;
		void close_medius_locked( ) const;
	};
}
