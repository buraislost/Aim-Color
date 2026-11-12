#pragma once

namespace Utility::RAII
{
	struct ScreenDcDeleter
	{
		void operator()( const HDC hdc ) const
		{
			if ( hdc )
			{
				ReleaseDC( nullptr, hdc );
			}
		}
	};

	struct DcDeleter
	{
		void operator()( const HDC hdc ) const
		{
			if ( hdc )
			{
				DeleteDC( hdc );
			}
		}
	};

	struct GdiObjectDeleter
	{
		void operator()( const HGDIOBJ obj ) const
		{
			if ( obj )
			{
				DeleteObject( obj );
			}
		}
	};

	struct GdiSelectRestorer
	{
		HDC hdc_to_restore = nullptr;

		explicit GdiSelectRestorer( const HDC hdc )
			: hdc_to_restore( hdc )
		{
		}

		void operator()( const HGDIOBJ old_object ) const
		{
			if ( hdc_to_restore && old_object )
			{
				SelectObject( hdc_to_restore, old_object );
			}
		}
	};

	using UniqueHdc       = std::unique_ptr<std::remove_pointer_t<HDC>, DcDeleter>;
	using UniqueHdcScreen = std::unique_ptr<std::remove_pointer_t<HDC>, ScreenDcDeleter>;
	using UniqueBitmap    = std::unique_ptr<std::remove_pointer_t<HBITMAP>, GdiObjectDeleter>;
	using UniqueGdiSelect = std::unique_ptr<std::remove_pointer_t<HGDIOBJ>, GdiSelectRestorer>;
}
