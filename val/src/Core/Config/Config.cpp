#define BASE64PP_IMPLEMENTATION

#include "Stdafx.hpp"
#include "Core/Config/Config.hpp"

#include <filesystem>
#include <fstream>

namespace Core
{
	std::string Config::export_to_str( ) const
	{
		const json j               = *this;
		const std::string json_str = j.dump( );

		const std::string encrypted_str = Utility::String::xor_crypt( json_str, encryption_key_ );

		const std::span data_span =
		{
			ReCa<const std::uint8_t*>( encrypted_str.data( ) ),
			encrypted_str.size( )
		};

		return base64pp::encode( data_span );
	}

	bool Config::import_from_str( const std::string& str )
	{
		if ( str.empty( ) )
			return false;

		try
		{
			const auto decoded_data = base64pp::decode( str );

			if ( !decoded_data )
				return false;

			const std::string encrypted_str( decoded_data->begin( ), decoded_data->end( ) );
			const std::string json_str = Utility::String::xor_crypt( encrypted_str, encryption_key_ );

			const json j = json::parse( json_str );

			j.get_to( *this );

			return true;
		}
		catch ( const std::exception& e )
		{
			return false;
		}
	}

	std::filesystem::path Config::file_path( )
	{
		wchar_t buf[ MAX_PATH ] {};
		GetModuleFileNameW( nullptr, buf, MAX_PATH );
		return std::filesystem::path( buf ).replace_filename( L"bura_config.json" );
	}

	bool Config::save_file( ) const
	{
		try
		{
			const auto path = file_path( );
			auto tmp        = path;
			tmp += L".tmp";

			{
				std::ofstream out( tmp, std::ios::binary | std::ios::trunc );

				if ( !out )
					return false;

				const json j = *this;
				out << j.dump( 2 );
			}

			std::filesystem::rename( tmp, path ); // replaces atomically enough for a settings file
			return true;
		}
		catch ( const std::exception& )
		{
			return false;
		}
	}

	bool Config::load_file( )
	{
		try
		{
			std::ifstream in( file_path( ), std::ios::binary );

			if ( !in )
				return false;

			json::parse( in ).get_to( *this );
			return true;
		}
		catch ( const std::exception& )
		{
			return false;
		}
	}
}
