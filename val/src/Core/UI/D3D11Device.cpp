#include "Stdafx.hpp"
#include "Core/UI/D3D11Device.hpp"

namespace Core::UI
{
	D3D11Device::D3D11Device( const HWND hwnd )
	{
		DXGI_SWAP_CHAIN_DESC sd   = {};
		sd.BufferCount            = 2;
		sd.BufferDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate = { .Numerator = 60, .Denominator = 1 };
		sd.BufferUsage            = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow           = hwnd;
		sd.SampleDesc             = { .Count = 1, .Quality = 0 };
		sd.Windowed               = TRUE;
		sd.SwapEffect             = DXGI_SWAP_EFFECT_DISCARD;
		sd.Flags                  = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		constexpr D3D_FEATURE_LEVEL feature_levels[ ] =
		{
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_0
		};

		D3D_FEATURE_LEVEL selected_level;

		const HRESULT hr = D3D11CreateDeviceAndSwapChain
		(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			0,
			feature_levels,
			_countof( feature_levels ),
			D3D11_SDK_VERSION,
			&sd,
			&swap_chain_,
			&device_,
			&selected_level,
			&device_context_
		);

		if ( FAILED( hr ) )
			throw std::runtime_error( "Failed to create D3D11 device and swap chain." );

		create_render_target( );
	}

	D3D11Device::~D3D11Device( )
	{
		cleanup_render_target( );
	}

	void D3D11Device::create_render_target( )
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
		HRESULT hr = swap_chain_->GetBuffer( 0, IID_PPV_ARGS( &back_buffer ) );

		if ( FAILED( hr ) )
			throw std::runtime_error( "Failed to get back buffer" );

		hr = device_->CreateRenderTargetView( back_buffer.Get( ), nullptr, &render_target_view_ );

		if ( FAILED( hr ) )
			throw std::runtime_error( "Failed to create render target view" );
	}

	void D3D11Device::cleanup_render_target( )
	{
		render_target_view_.Reset( );
	}
}
