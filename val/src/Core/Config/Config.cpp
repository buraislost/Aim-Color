#define BASE64PP_IMPLEMENTATION

#include "Stdafx.hpp"
#include "Core/Config/Config.hpp"

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
}
