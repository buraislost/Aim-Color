#include "Stdafx.hpp"
#include "Core/BindSystem/BindSystem.hpp"
#include "Core/Config/Config.hpp"
#include "Core/Features/ColorBot.hpp"
#include "Core/Features/DebugState.hpp"
#include "Core/Mouse/Mouse.hpp"
#include "Core/UI/Window.hpp"
#include "Core/Web/WebGui.hpp"

#include <timeapi.h>

#pragma comment( lib, "winmm.lib" )

namespace
{
	// DPI aware: screen metrics and BitBlt use physical pixels, not virtualized ones.
	// 1ms timer: default granularity is ~15.6ms, which stretches every Sleep(1) in Mouse::move
	// and the 1ms ColorBot loop by an order of magnitude.
	struct ProcessTuning
	{
		ProcessTuning( )
		{
			SetProcessDPIAware( );
			timeBeginPeriod( 1 );
		}

		~ProcessTuning( )
		{
			timeEndPeriod( 1 );
		}
	};
}

class Application
{
public:
	Application( )
	{
		service_hub_ = std::make_shared<ServiceHub>( );
	}

	[[nodiscard]] int run( ) const
	{
		try
		{
			const auto mouse = std::make_shared<Core::Mouse>( );
			service_hub_->register_service( mouse );

			const auto config = std::make_shared<Core::Config>( );
			service_hub_->register_service( config );

			// apply the saved backend at startup, not only when "Apply input" is pressed
			mouse->configure( config->settings_.input_backend, config->settings_.com_port );

			service_hub_->register_service( std::make_shared<Core::DebugState>( ) );

			const auto bind_system = std::make_shared<Core::BindSystem>( );
			service_hub_->register_service( bind_system );

			// browser UI, loopback only. a busy port just means no web UI, the ImGui window still works
			std::unique_ptr<Core::Web::WebGui> web;

			try { web = std::make_unique<Core::Web::WebGui>( *service_hub_ ); }
			catch ( ... ) { }

			const auto window = std::make_shared<Core::UI::Window>
			(
				L" ",
				ImVec2( 450.f, 640.f ),
				*service_hub_
			);

			const auto color_thread = std::jthread
			(
				[&]
				{
					const auto feature = std::make_unique<Core::Features::ColorBot>( *service_hub_ );

					while ( window->is_running( ) )
					{
						feature->on_update( );

						using namespace std::chrono_literals;
						std::this_thread::sleep_for( 1ms );
					}
				}
			);

			window->run( );
		}
		catch ( const std::exception& e )
		{
			MessageBoxA( nullptr, e.what( ), "Error", MB_OK | MB_ICONERROR );
			return 1;
		}

		return 0;
	}

private:
	std::shared_ptr<ServiceHub> service_hub_;
};

#ifdef NDEBUG
int __stdcall WinMain( HINSTANCE, HINSTANCE, LPSTR, int )
#else
int main( int, char*[ ] )
#endif
{
	const ProcessTuning tuning;

	const Application app;
	return app.run( );
}
