#pragma once

class ServiceHub
{
public:
	template<typename T>
	void register_service( const std::shared_ptr<T>& service )
	{
		services_[ std::type_index( typeid( T ) ) ] = service;
	}

	template<typename T>
	[[nodiscard]] std::shared_ptr<T> get( ) const
	{
		const auto it = services_.find( std::type_index( typeid( T ) ) );

		if ( it == services_.end( ) )
		{
			throw std::runtime_error( "Service not found: " + std::string( typeid( T ).name( ) ) );
		}

		return std::any_cast<std::shared_ptr<T>>( it->second );
	}

private:
	std::unordered_map<std::type_index, std::any> services_;
};
