#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace Core
{
	// shared between the ColorBot thread (writer) and the UI thread (reader).
	// ColorBot only does the extra work (full scan while idle, preview image) when `enabled` is set.
	struct DebugState
	{
		struct Info
		{
			bool aim_active     = false; // aim bind held / auto aim
			bool trigger_active = false;
			bool captured       = false; // screen capture returned a frame
			bool found          = false; // a blob passed MIN_BLOB_AREA
			bool has_lock       = false;
			bool on_target      = false;

			std::int32_t cap_w = 0;
			std::int32_t cap_h = 0;
			std::int32_t lit   = 0; // mask pixels matching the color
			std::int32_t blobs = 0; // all connected components
			std::int32_t big_blobs = 0; // components that pass the min area
			std::int32_t color = 0;

			cv::Point head;
			cv::Point center;
			double dist = -1.0;

			cv::Vec3b mean_bgr;   // mean of the capture, ~0 means a black frame
			cv::Vec3b center_hsv; // HSV under the crosshair, to tune the color ranges

			std::uint64_t frames = 0;
		};

		std::atomic<bool> enabled { false };

		void publish( const Info& info, const cv::Mat& preview_rgba )
		{
			std::lock_guard lock( mutex_ );

			info_ = info;

			if ( !preview_rgba.empty( ) )
				preview_rgba.copyTo( preview_ );
		}

		[[nodiscard]] Info info( ) const
		{
			std::lock_guard lock( mutex_ );
			return info_;
		}

		// copies the latest preview (RGBA, 2 images side by side); false if there is none yet
		[[nodiscard]] bool preview( cv::Mat& out ) const
		{
			std::lock_guard lock( mutex_ );

			if ( preview_.empty( ) )
				return false;

			preview_.copyTo( out );
			return true;
		}

	private:
		mutable std::mutex mutex_;
		Info info_;
		cv::Mat preview_;
	};
}
