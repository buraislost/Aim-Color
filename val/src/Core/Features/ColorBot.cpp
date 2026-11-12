#include "Stdafx.hpp"
#include "Core/Features/ColorBot.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>

#ifdef VAL_DEBUG_DETECTION
namespace
{
	// logs detection state every 30 frames to DebugView / VS output
	// lit = mask pixels, found = blob picked, dist = px from crosshair to head (-1 if none)
	void debug_detection( const cv::Mat& mask, const cv::Point& center, bool found,
	                      const cv::Point& head, bool has_lock, bool on_target )
	{
		static std::uint32_t frame = 0;

		if ( ++frame % 30 != 0 )
			return;

		const int    lit  = cv::countNonZero( mask );
		const double dist = found ? cv::norm( head - center ) : -1.0;

		char buf[ 256 ];
		std::snprintf( buf, sizeof( buf ),
		               "[ColorBot] lit=%d found=%d head=(%d,%d) center=(%d,%d) dist=%.1f lock=%d on=%d\n",
		               lit, found ? 1 : 0, head.x, head.y, center.x, center.y, dist,
		               has_lock ? 1 : 0, on_target ? 1 : 0 );

		OutputDebugStringA( buf );
	}
}
#endif

namespace Core::Features
{
	ColorBot::ColorBot( const ServiceHub& service_hub )
		: monitor_( ),
		  prev_center_( 0, 0 )
	{
		config_              = service_hub.get<Config>( );
		bind_system_         = service_hub.get<BindSystem>( );
		mouse_               = service_hub.get<Mouse>( );
		debug_               = service_hub.get<DebugState>( );
		structuring_element_ = cv::Mat::ones( 3, 3, CV_8U );
	}

	double ColorBot::rand_real( const double lo, const double hi )
	{
		return std::uniform_real_distribution<double>( lo, hi )( rng_ );
	}

	void ColorBot::reset_state( )
	{
		has_lock_         = false;
		is_on_target_     = false;
		on_target_frames_ = 0;
		lost_frames_      = 0;
		trigger_armed_    = false;
		reacting_         = false;
		noise_x_          = 0.0;
		noise_y_          = 0.0;
	}

	void ColorBot::on_update( )
	{
		// each bind is polled exactly once per frame: toggle mode eats the
		// GetAsyncKeyState "pressed since last call" bit, a second poll would miss the press
		const bool aim_active     = config_->aimbot_.auto_aim || bind_system_->is_active( config_->aimbot_.bind );
		const bool trigger_active = bind_system_->is_active( config_->trigger_.bind );

		// debug view on: keep scanning while idle so detection can be inspected without holding a bind.
		// nothing below moves the mouse or clicks unless aim_active / trigger_active is set.
		const bool debug = debug_ && debug_->enabled;

		// nothing armed: skip capture + CV entirely
		if ( !aim_active && !trigger_active && !debug )
		{
			reset_state( );
			return;
		}

		// primary monitor, same origin GetDC(nullptr) captures from
		const std::int32_t screen_w = GetSystemMetrics( SM_CXSCREEN );
		const std::int32_t screen_h = GetSystemMetrics( SM_CYSCREEN );

		const auto center_x   = screen_w / 2;
		const auto center_y   = screen_h / 2;
		const auto half_fov_x = std::clamp( config_->aimbot_.fov_x, FOV_MIN, FOV_MAX ) / 2;
		const auto half_fov_y = std::clamp( config_->aimbot_.fov_y, FOV_MIN, FOV_MAX ) / 2;

		monitor_ =
		{
			.left = center_x - half_fov_x,
			.top = center_y - half_fov_y,
			.right = center_x + half_fov_x,
			.bottom = center_y + half_fov_y
		};

		// 0 = Tritanopia magenta, 1 = Deuteranopia/Protanopia yellow, 2 = Default red
		switch ( config_->settings_.color )
		{
			case 0:
				lower_color_ = cv::Scalar( 140, 90, 120 );
				upper_color_ = cv::Scalar( 168, 255, 255 );
				break;
			case 1:
				lower_color_ = cv::Scalar( 22, 150, 150 );
				upper_color_ = cv::Scalar( 35, 255, 255 );
				break;
			default:
				lower_color_ = cv::Scalar( 0, 150, 120 );
				upper_color_ = cv::Scalar( 8, 255, 255 );
				break;
		}

		screen_center_ = cv::Point( half_fov_x, half_fov_y );

		const cv::Mat mat = screen( );

		if ( mat.empty( ) )
		{
			if ( debug )
			{
				DebugState::Info info;
				info.aim_active     = aim_active;
				info.trigger_active = trigger_active;
				info.captured       = false;
				info.frames         = ++dbg_frames_;
				debug_->publish( info, { } );
			}

			return;
		}

		cv::Mat hsv, mask;
		cv::cvtColor( mat, hsv, cv::COLOR_BGR2HSV );
		cv::inRange( hsv, lower_color_, upper_color_, mask );

		// red wraps around hue 0, so it needs a second range
		if ( config_->settings_.color == 2 )
		{
			cv::Mat wrap_mask;
			cv::inRange( hsv, cv::Scalar( 170, 150, 120 ), cv::Scalar( 180, 255, 255 ), wrap_mask );
			cv::bitwise_or( mask, wrap_mask, mask );
		}

		cv::morphologyEx( mask, mask, cv::MORPH_CLOSE, structuring_element_, cv::Point( -1, -1 ), 2 );

		cv::Point head;
		const bool found = find_target( mask, head );

#ifdef VAL_DEBUG_DETECTION
		debug_detection( mask, screen_center_, found, head, has_lock_, is_on_target_ );
#endif

		if ( debug )
		{
			DebugState::Info info;
			info.aim_active     = aim_active;
			info.trigger_active = trigger_active;
			info.captured       = true;
			info.found          = found;
			info.has_lock       = has_lock_;
			info.on_target      = is_on_target_;
			info.cap_w          = mat.cols;
			info.cap_h          = mat.rows;
			info.lit            = cv::countNonZero( mask );
			info.blobs          = dbg_blobs_;
			info.big_blobs      = dbg_big_blobs_;
			info.color          = config_->settings_.color;
			info.head           = head;
			info.center         = screen_center_;
			info.dist           = found ? cv::norm( head - screen_center_ ) : -1.0;
			info.frames         = ++dbg_frames_;

			const cv::Scalar mean = cv::mean( mat );
			info.mean_bgr         = cv::Vec3b( cv::saturate_cast<uchar>( mean[ 0 ] ), cv::saturate_cast<uchar>( mean[ 1 ] ),
			                                   cv::saturate_cast<uchar>( mean[ 2 ] ) );

			info.center_hsv = hsv.at<cv::Vec3b>( std::clamp( screen_center_.y, 0, hsv.rows - 1 ),
			                                     std::clamp( screen_center_.x, 0, hsv.cols - 1 ) );

			// preview image is the expensive part, ~30 fps is plenty
			cv::Mat preview;
			const auto now = Clock::now( );

			if ( now - dbg_last_preview_ >= std::chrono::milliseconds( 33 ) )
			{
				dbg_last_preview_ = now;

				cv::Mat left = mat.clone( ), right;
				cv::cvtColor( mask, right, cv::COLOR_GRAY2BGR );

				const int area = std::max( 1, config_->trigger_.scan_area );

				for ( cv::Mat* m : { &left, &right } )
				{
					cv::rectangle( *m, screen_center_ - cv::Point( area, area ), screen_center_ + cv::Point( area, area ),
					               cv::Scalar( 255, 128, 0 ), 1 );
					cv::drawMarker( *m, screen_center_, cv::Scalar( 0, 255, 0 ), cv::MARKER_CROSS, 12, 1 );

					if ( found )
						cv::circle( *m, head, 4, cv::Scalar( 0, 255, 255 ), 1 );
				}

				cv::Mat both;
				cv::hconcat( left, right, both );
				cv::cvtColor( both, preview, cv::COLOR_BGR2RGBA );
			}

			debug_->publish( info, preview );
		}

		if ( !found )
		{
			trigger_armed_ = false;

			// keep the lock for a few frames so a flicker doesn't switch targets
			if ( has_lock_ && ++lost_frames_ > LOCK_LOST_FRAMES )
			{
				has_lock_         = false;
				is_on_target_     = false;
				on_target_frames_ = 0;
			}

			return;
		}

		if ( !has_lock_ )
		{
			prev_center_ = head; // new lock: no smoothing jump from stale position

			// human reaction time: hold off on the first correction for a random 90-160ms
			lock_acquired_at_ = Clock::now( );
			reaction_ms_      = rand_real( 90.0, 160.0 );
			reacting_         = true;
			noise_x_          = 0.0;
			noise_y_          = 0.0;
		}

		has_lock_    = true;
		lost_frames_ = 0;
		locked_head_ = head;

		// on-target is judged against the offset aim point, the same point move() dead-zones on;
		// judging the raw head left it unreachable at high res where the offset is large.
		// offsets and thresholds are tuned at 1080p, scale them with the real resolution
		const double px_scale = std::max( 0.5, screen_h / 1080.0 );

		const cv::Point aim_offset( cvRound( config_->aimbot_.offset_x * px_scale ),
		                            cvRound( config_->aimbot_.offset_y * px_scale ) );

		const double distance = cv::norm( head + aim_offset - screen_center_ );

		if ( distance <= ON_TARGET_THRESHOLD * px_scale )
		{
			on_target_frames_++;

			if ( on_target_frames_ >= ON_TARGET_FRAMES_REQUIRED )
			{
				is_on_target_ = true;
			}
		}
		else
		{
			on_target_frames_ = 0;
			is_on_target_     = false;
		}

		// smooth the head position (1.0 = raw, lower = laggier), then offset the aim point
		const double smooth = std::clamp<double>( config_->aimbot_.smooth, 0.05, 1.0 );

		const cv::Point smoothed
		(
			cvRound( smooth * head.x + ( 1 - smooth ) * prev_center_.x ),
			cvRound( smooth * head.y + ( 1 - smooth ) * prev_center_.y )
		);

		prev_center_ = smoothed;

		const cv::Point aim = smoothed + aim_offset;

		if ( aim_active )
		{
			// reaction gate: skipped once already on target so tracking stays tight
			if ( reacting_ )
			{
				const auto elapsed_ms = std::chrono::duration<double, std::milli>( Clock::now( ) - lock_acquired_at_ ).count( );

				if ( is_on_target_ || elapsed_ms >= reaction_ms_ )
					reacting_ = false;
			}

			if ( !reacting_ )
			{
				if ( config_->aimbot_.lock )
					lock_move( aim - screen_center_ );
				else
					move( aim - screen_center_ );
			}
		}
		else
		{
			is_on_target_     = false;
			on_target_frames_ = 0;

			// aim not engaged: re-arm the reaction delay so it counts from the moment the bind is pressed
			reacting_         = true;
			lock_acquired_at_ = Clock::now( );
			reaction_ms_      = rand_real( 90.0, 160.0 );
		}

		if ( trigger_active )
			update_trigger( head );
		else
			trigger_armed_ = false;
	}

	// non-blocking: arm when the head enters the scan box, fire once the jittered delay passed.
	// leaving the box, or holding a movement key with move_check on, disarms.
	void ColorBot::update_trigger( const cv::Point& head )
	{
		const cv::Point diff = head - screen_center_;
		const int area       = std::max( 1, config_->trigger_.scan_area );

		const bool inside  = std::abs( diff.x ) <= area && std::abs( diff.y ) <= area;
		const bool blocked = config_->trigger_.move_check && is_movement_key_pressed( );

		if ( !inside || blocked )
		{
			trigger_armed_ = false;
			return;
		}

		const auto now = Clock::now( );

		if ( !trigger_armed_ )
		{
			// jittered click delay, +-25%
			const double base_delay = static_cast<double>( std::max( 0, config_->trigger_.delay ) );
			const auto jittered_ms  = static_cast<std::int64_t>( base_delay * rand_real( 0.75, 1.25 ) );

			trigger_armed_   = true;
			trigger_fire_at_ = now + std::chrono::milliseconds( jittered_ms );
			return;
		}

		if ( now < trigger_fire_at_ || now < trigger_next_shot_ )
			return;

		mouse_->left_click( );

		trigger_armed_     = false; // next shot needs a fresh delay
		trigger_next_shot_ = now + std::chrono::milliseconds( TRIGGER_COOLDOWN_MS );
	}

	// picks one blob as the target. head = mean x of the blob's topmost row.
	// while locked, prefer the blob nearest the last locked head (within STICKY_RADIUS);
	// otherwise take the blob nearest the crosshair.
	bool ColorBot::find_target( const cv::Mat& mask, cv::Point& out ) const
	{
		cv::Mat labels, stats, centroids;
		const std::int32_t count = cv::connectedComponentsWithStats( mask, labels, stats, centroids, 8, CV_32S );

		bool has_lock_candidate   = false;
		bool has_center_candidate = false;

		double best_lock_dist   = std::numeric_limits<double>::max( );
		double best_center_dist = std::numeric_limits<double>::max( );

		cv::Point best_lock_head;
		cv::Point best_center_head;

		dbg_blobs_     = count - 1;
		dbg_big_blobs_ = 0;

		for ( std::int32_t i = 1; i < count; ++i )
		{
			if ( stats.at<std::int32_t>( i, cv::CC_STAT_AREA ) < MIN_BLOB_AREA )
				continue;

			++dbg_big_blobs_;

			const std::int32_t top   = stats.at<std::int32_t>( i, cv::CC_STAT_TOP );
			const std::int32_t left  = stats.at<std::int32_t>( i, cv::CC_STAT_LEFT );
			const std::int32_t width = stats.at<std::int32_t>( i, cv::CC_STAT_WIDTH );

			double sum_x = 0.0;
			std::int32_t n = 0;

			for ( std::int32_t x = left; x < left + width; ++x )
			{
				if ( labels.at<std::int32_t>( top, x ) == i )
				{
					sum_x += x;
					++n;
				}
			}

			if ( n == 0 )
				continue;

			const cv::Point head( StCa<std::int32_t>( sum_x / n ), top );

			const double center_dist = cv::norm( head - screen_center_ );

			if ( center_dist < best_center_dist )
			{
				best_center_dist   = center_dist;
				best_center_head   = head;
				has_center_candidate = true;
			}

			if ( has_lock_ )
			{
				const double lock_dist = cv::norm( head - locked_head_ );

				if ( lock_dist < best_lock_dist )
				{
					best_lock_dist      = lock_dist;
					best_lock_head      = head;
					has_lock_candidate  = true;
				}
			}
		}

		if ( has_lock_candidate && best_lock_dist <= STICKY_RADIUS )
		{
			out = best_lock_head;
			return true;
		}

		if ( has_center_candidate )
		{
			out = best_center_head;
			return true;
		}

		return false;
	}

	cv::Mat ColorBot::screen( ) const
	{
		const std::int32_t capture_width  = monitor_.right - monitor_.left;
		const std::int32_t capture_height = monitor_.bottom - monitor_.top;

		const Utility::RAII::UniqueHdcScreen screen_dc( GetDC( nullptr ) );

		if ( !screen_dc )
			return {};

		const Utility::RAII::UniqueHdc memory_dc
		(
			CreateCompatibleDC( screen_dc.get( ) )
		);

		if ( !memory_dc )
			return {};

		const Utility::RAII::UniqueBitmap bitmap
		(
			CreateCompatibleBitmap
			(
				screen_dc.get( ),
				capture_width,
				capture_height
			)
		);

		if ( !bitmap )
			return {};

		Utility::RAII::UniqueGdiSelect old_bitmap_restorer
		(
			SelectObject( memory_dc.get( ), bitmap.get( ) ),
			Utility::RAII::GdiSelectRestorer( memory_dc.get( ) )
		);

		BitBlt
		(
			memory_dc.get( ),
			0,
			0,
			capture_width,
			capture_height,
			screen_dc.get( ),
			monitor_.left,
			monitor_.top,
			SRCCOPY
		);

		BITMAPINFOHEADER info = {};
		info.biSize           = sizeof( BITMAPINFOHEADER );
		info.biWidth          = capture_width;
		info.biHeight         = -capture_height;
		info.biPlanes         = 1;
		info.biBitCount       = 32;
		info.biCompression    = BI_RGB;

		const cv::Mat frame( capture_height, capture_width, CV_8UC4 );

		GetDIBits
		(
			memory_dc.get( ),
			bitmap.get( ),
			0,
			capture_height,
			frame.data,
			ReCa<BITMAPINFO*>( &info ),
			DIB_RGB_COLORS
		);

		cv::Mat bgr_frame;
		cv::cvtColor( frame, bgr_frame, cv::COLOR_BGRA2BGR );

		return bgr_frame;
	}

	bool ColorBot::is_movement_key_pressed( ) const
	{
		constexpr std::array movement_keys = { 'W', 'A', 'S', 'D' };

		auto is_key_down = [ ]( const char key ) -> bool
		{
			return ( GetAsyncKeyState( key ) & 0x8000 ) != 0;
		};

		return std::ranges::any_of( movement_keys, is_key_down );
	}

	// snap: send the whole error (aim point - crosshair) as one report.
	// the game needs a frame or two to show the correction, so measuring again right away
	// would still see the old error and overshoot; lock_settle_ms spaces the corrections.
	void ColorBot::lock_move( const cv::Point& delta )
	{
		const auto now    = Clock::now( );
		const auto settle = std::chrono::milliseconds( std::clamp( config_->aimbot_.lock_settle_ms, 1, 30 ) );

		if ( now - last_lock_move_ < settle )
			return;

		const double gain = std::clamp<double>( config_->aimbot_.lock_gain, 0.1, 5.0 );

		const auto ix = static_cast<std::int32_t>( std::lround( delta.x * gain ) );
		const auto iy = static_cast<std::int32_t>( std::lround( delta.y * gain ) );

		if ( ix == 0 && iy == 0 )
			return;

		mouse_->move_raw( ix, iy );
		last_lock_move_ = now;
	}

	// delta = aim point - crosshair. when locked on target, hold still inside HOLD_RADIUS
	// and drop tremor/curve so the crosshair doesn't shake.
	void ColorBot::move( const cv::Point& delta )
	{
		const double distance = cv::norm( delta );

		if ( distance < ( is_on_target_ ? HOLD_RADIUS : DEADZONE_RADIUS ) )
			return;

		// imported config can hold 0 / negative, 10 / sensitivity would blow up
		const double sensitivity = std::clamp<double>( config_->settings_.sensitivity, 0.1, 5.0 );

		const double ease = std::clamp( distance / 60.0, 0.25, 1.0 );

		const double gain = sensitivity * 0.5 * ease *
		                    ( is_on_target_ ? 0.4 : rand_real( 0.85, 1.15 ) );

		double move_x = delta.x * gain;
		double move_y = delta.y * gain;

		if ( !is_on_target_ )
		{
			// AR(1): drift wanders smoothly instead of jittering frame to frame.
			// stationary amplitude ~0.4px per axis
			noise_x_ = 0.9 * noise_x_ + 0.3 * rand_real( -1.0, 1.0 );
			noise_y_ = 0.9 * noise_y_ + 0.3 * rand_real( -1.0, 1.0 );

			move_x += noise_x_;
			move_y += noise_y_;
		}

		const double max_move = is_on_target_ ? 8.0 : 15.0;
		const double move_len = std::sqrt( move_x * move_x + move_y * move_y );

		if ( move_len > max_move )
		{
			const double scale = max_move / move_len;
			move_x *= scale;
			move_y *= scale;
		}

		const auto ix = static_cast<std::int32_t>( std::lround( move_x ) );
		const auto iy = static_cast<std::int32_t>( std::lround( move_y ) );

		if ( ix == 0 && iy == 0 )
			return;

		const double len    = std::max( std::sqrt( move_x * move_x + move_y * move_y ), 1.0 );
		const double perp_x = -move_y / len;
		const double perp_y =  move_x / len;
		const double curve  = is_on_target_ ? 0.0 : rand_real( -0.12, 0.12 ) * len;

		const double ctrl_x = move_x * 0.5 + perp_x * curve;
		const double ctrl_y = move_y * 0.5 + perp_y * curve;

		double duration = 10.0 / sensitivity;

		if ( is_on_target_ )
		{
			duration *= 2.0;
		}
		else
		{
			duration *= rand_real( 0.8, 1.3 ) * std::clamp( len / 10.0, 0.5, 1.5 );
		}

		mouse_->move
		(
			ix,
			iy,
			static_cast<std::uint32_t>( std::max( 1.0, duration ) ),
			static_cast<std::int32_t>( std::lround( ctrl_x ) ),
			static_cast<std::int32_t>( std::lround( ctrl_y ) )
		);
	}
}
