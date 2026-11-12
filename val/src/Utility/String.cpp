#include "Stdafx.hpp"
#include "Utility/String.hpp"

namespace Utility::String
{
	std::string xor_crypt( const std::string& data, const std::uint32_t key )
	{
		std::string result = data;

		const auto key_bytes = ReCa<const char*>( &key );

		for ( std::size_t i = 0; i < data.size( ); ++i )
		{
			result[ i ] = data[ i ] ^ key_bytes[ i % sizeof( key ) ];
		}

		return result;
	}
}
