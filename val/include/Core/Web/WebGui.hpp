#pragma once

#include <memory>
#include <thread>

class ServiceHub;

namespace Core::Web
{
	// browser UI on http://127.0.0.1:8765 (loopback only): same settings as the ImGui window + live debug view
	class WebGui
	{
	public:
		explicit WebGui( ServiceHub& hub, unsigned short port = 8765 );
		~WebGui( );

		WebGui( const WebGui& )            = delete;
		WebGui& operator=( const WebGui& ) = delete;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
		std::thread thread_;
	};
}
