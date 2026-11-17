#include "Stdafx.hpp"
#include "Core/Features/ColorBot.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>

namespace Core::Features
{
	ColorBot::ColorBot( const ServiceHub& service_hub )
		: monitor_( )
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
		is_on_target_  = false;
		trigger_armed_ = false;
		holding_       = false;
		acquired_      = false;
		filter_valid_  = false;
		rem_x_         = 0.0;
		rem_y_         = 0.0;
		sent_.clear( );
	}

	// per frame: capture a box around the crosshair -> colour mask -> pick the head nearest the crosshair
	// -> aim point = centre of that head (low-passed) -> move toward it, stop once inside the head, click on it
	void ColorBot::on_update( )
	{
		// each bind is polled exactly once per frame: toggle mode eats the
		// GetAsyncKeyState "pressed since last call" bit, a second poll would miss the press
		const bool aim_active     = config_->aimbot_.auto_aim || bind_system_->is_active( config_->aimbot_.bind );
		const bool trigger_active = bind_system_->is_active( config_->trigger_.bind );

		// debug view on: keep scanning while idle so detection can be inspected without holding a bind.
		// nothing below moves the mouse or clicks unless aim_active / trigger_active is set.
		const bool debug = debug_ && debug_->wanted( );

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
		build_mask( hsv, mask );

		Target tgt;
		const bool found = find_target( mask, tgt, holding_ ? &hold_head_ : nullptr );

		const auto now = Clock::now( );

		// offsets and thresholds are tuned at 1080p, scale them with the real resolution
		const double px_scale = std::max( 0.5, screen_h / 1080.0 );
		const cv::Point2d center( screen_center_.x, screen_center_.y );

		cv::Point2d delta( 0.0, 0.0 ); // aim point minus crosshair: how far the mouse still has to travel
		double head_h = 0.0;

		if ( found )
		{
			last_seen_ = now;

			const cv::Point2d offset( config_->aimbot_.offset_x * px_scale, config_->aimbot_.offset_y * px_scale );
			const cv::Point2d raw_aim( tgt.head.x, tgt.head.y + tgt.head_h * 0.5 ); // centre of the head

			// low-pass: the blob top flickers by a pixel or two between frames, aiming at the raw value is the jitter
			if ( !filter_valid_ || cv::norm( raw_aim - aim_f_ ) > JUMP_PX * px_scale )
			{
				aim_f_        = raw_aim;
				head_h_f_     = tgt.head_h;
				filter_valid_ = true;
			}
			else
			{
				const double dt = std::chrono::duration<double, std::milli>( now - filter_time_ ).count( );
				const double a  = 1.0 - std::exp( -dt / AIM_FILTER_MS );

				aim_f_    += ( raw_aim - aim_f_ ) * a;
				head_h_f_ += ( tgt.head_h - head_h_f_ ) * a;
			}

			filter_time_ = now;

			delta  = aim_f_ + offset - center;
			head_h = head_h_f_;
		}
		else if ( now - last_seen_ > std::chrono::milliseconds( LOST_MS ) )
		{
			// target gone for good: forget hold, filter and reaction so the next one starts fresh
			holding_      = false;
			acquired_     = false;
			filter_valid_ = false;
			rem_x_        = 0.0;
			rem_y_        = 0.0;
			sent_.clear( );
		}

		// head box the crosshair has to be inside to count as "on the head"
		const double hold_half = head_h > 0.0
			? std::max( 2.0 * px_scale, std::clamp( config_->aimbot_.hold_pct, 0, 150 ) / 100.0 * 0.5 * head_h )
			: 0.0;

		const bool inside = found && std::abs( delta.x ) <= hold_half && std::abs( delta.y ) <= hold_half;

		is_on_target_ = inside;

		if ( debug )
		{
			DebugState::Info info;
			info.aim_active     = aim_active;
			info.trigger_active = trigger_active;
			info.captured       = true;
			info.found          = found;
			info.has_lock       = holding_;
			info.on_target      = is_on_target_;
			info.cap_w          = mat.cols;
			info.cap_h          = mat.rows;
			info.lit            = cv::countNonZero( mask );
			info.blobs          = dbg_blobs_;
			info.big_blobs      = dbg_big_blobs_;
			info.color          = config_->settings_.color;
			info.head           = cv::Point( cvRound( tgt.head.x ), cvRound( tgt.head.y ) );
			info.center         = screen_center_;
			info.dist           = found ? cv::norm( delta ) : -1.0;
			info.frames         = ++dbg_frames_;

			const cv::Scalar mean = cv::mean( mat );
			info.mean_bgr         = cv::Vec3b( cv::saturate_cast<uchar>( mean[ 0 ] ), cv::saturate_cast<uchar>( mean[ 1 ] ),
			                                   cv::saturate_cast<uchar>( mean[ 2 ] ) );

			info.center_hsv = hsv.at<cv::Vec3b>( std::clamp( screen_center_.y, 0, hsv.rows - 1 ),
			                                     std::clamp( screen_center_.x, 0, hsv.cols - 1 ) );

			// preview image is the expensive part, ~30 fps is plenty
			cv::Mat preview;

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
					{
						// yellow circle = where the aim is going, radius = the head box
						const cv::Point aim_px( cvRound( center.x + delta.x ), cvRound( center.y + delta.y ) );
						cv::circle( *m, aim_px, std::max( 2, cvRound( hold_half ) ), cv::Scalar( 0, 255, 255 ), 1 );
					}
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
			return;
		}

		// hold: crosshair already on the head -> stop chasing the colour (and stop being dragged to the body)
		if ( !aim_active )
		{
			holding_  = false;
			acquired_ = false;
		}
		else if ( holding_ )
		{
			hold_head_ = tgt.head; // follow slow drift of the held head

			if ( std::abs( delta.x ) > hold_half * HOLD_RELEASE_MUL || std::abs( delta.y ) > hold_half * HOLD_RELEASE_MUL )
				holding_ = false; // target walked away: track again
		}
		else if ( config_->aimbot_.hold_pct > 0 && inside )
		{
			holding_   = true;
			hold_head_ = tgt.head;
		}

		if ( aim_active && !acquired_ )
		{
			// human-ish reaction time before the first correction on a fresh target
			acquired_      = true;
			acquired_at_   = now;
			reaction_ms_   = rand_real( 20.0, 40.0 );
		}

		if ( aim_active && !holding_ )
			aim( delta, head_h, px_scale );

		if ( trigger_active )
			update_trigger( delta, head_h );
		else
			trigger_armed_ = false;
	}

	// colour -> binary mask. OpenCV hue is 0-179. the S/V floors are low on purpose: the outline is only
	// 1-2px wide and fades into the body / background, a high floor throws most of it away.
	void ColorBot::build_mask( const cv::Mat& hsv, cv::Mat& mask ) const
	{
		switch ( config_->settings_.color )
		{
			case 0: // magenta (Tritanopia)
				cv::inRange( hsv, cv::Scalar( 140, 80, 100 ), cv::Scalar( 170, 255, 255 ), mask );
				break;

			case 1: // yellow (Deuteranopia / Protanopia), Bura range: a bright core (V>=235) plus looser pixels
			        // (H22-35, V>=150) that only count when a core pixel sits within 3px (blur / AA edge).
			        // tan walls and wood have no core nearby, so they never reach the target picker.
			{
				cv::Mat core, loose, near_core;
				cv::inRange( hsv, cv::Scalar( 25, 110, 235 ), cv::Scalar( 35, 255, 255 ), core );
				cv::inRange( hsv, cv::Scalar( 22, 90, 150 ), cv::Scalar( 35, 255, 255 ), loose );
				cv::dilate( core, near_core, cv::Mat::ones( 7, 7, CV_8U ) );
				cv::bitwise_and( loose, near_core, loose );
				cv::bitwise_or( core, loose, mask );
				break;
			}

			default: // red wraps around hue 0, so it needs two ranges
			{
				cv::Mat wrap;
				cv::inRange( hsv, cv::Scalar( 0, 130, 110 ), cv::Scalar( 8, 255, 255 ), mask );
				cv::inRange( hsv, cv::Scalar( 172, 130, 110 ), cv::Scalar( 179, 255, 255 ), wrap );
				cv::bitwise_or( mask, wrap, mask );
				break;
			}
		}

		// bridge 1-2px gaps in the outline so one enemy stays one blob
		cv::morphologyEx( mask, mask, cv::MORPH_CLOSE, structuring_element_, cv::Point( -1, -1 ), 2 );
	}

	// one report per `Settle` ms:
	//  - the error is measured against where the crosshair WILL be: corrections sent in the last PENDING_MS
	//    are not on screen yet, counting them again was the overshoot / shake
	//  - the step eases out near the head (EASE_MIN at the aim point, full gain from 1.5 heads away)
	//  - +-10% random on each step, and a 20-40ms reaction before the first one, so it does not look scripted
	//  - fractions of a count are carried over, small errors close smoothly instead of flipping between 0 and 1
	// counts = error * gain * smooth * ease; gain converts screen px to mouse counts (depends on in-game sens).
	void ColorBot::aim( const cv::Point2d& delta, const double head_h, const double px_scale )
	{
		const auto now = Clock::now( );

		if ( std::chrono::duration<double, std::milli>( now - acquired_at_ ).count( ) < reaction_ms_ )
			return;

		const auto settle = std::chrono::milliseconds( std::clamp( config_->aimbot_.lock_settle_ms, 1, 30 ) );

		if ( now - last_move_ < settle )
			return;

		last_move_ = now; // every evaluation counts, even one that sends nothing: the remainder must not pile up per frame

		const double gain   = std::clamp<double>( config_->aimbot_.lock_gain, 0.1, 5.0 );
		const double smooth = std::clamp<double>( config_->aimbot_.smooth, 0.05, 1.0 );

		while ( !sent_.empty( ) && now - sent_.front( ).t > std::chrono::milliseconds( PENDING_MS ) )
			sent_.pop_front( );

		double pending_x = 0.0;
		double pending_y = 0.0;

		for ( const auto& s : sent_ )
		{
			pending_x += s.dx;
			pending_y += s.dy;
		}

		const double ex   = delta.x - pending_x / gain;
		const double ey   = delta.y - pending_y / gain;
		const double dist = std::hypot( ex, ey );

		if ( dist < DEADZONE_PX * px_scale )
		{
			rem_x_ = 0.0;
			rem_y_ = 0.0;
			return;
		}

		const double ref  = std::max( 8.0 * px_scale, 1.5 * head_h );
		const double ease = std::clamp( dist / ref, EASE_MIN, 1.0 );
		const double k    = gain * smooth * ease * rand_real( 0.9, 1.1 );

		const double fx = ex * k + rem_x_;
		const double fy = ey * k + rem_y_;

		const auto ix = static_cast<std::int32_t>( std::lround( fx ) );
		const auto iy = static_cast<std::int32_t>( std::lround( fy ) );

		rem_x_ = fx - ix;
		rem_y_ = fy - iy;

		if ( ix == 0 && iy == 0 )
			return;

		mouse_->move_raw( ix, iy );
		sent_.push_back( { now, ix, iy } );
	}

	// fires when the crosshair stays on the head for `delay` ms. "on the head" = inside the scan box, which
	// is never smaller than 35% of the head so it also works on big close-up heads.
	// leaving the box, or holding a movement key with move_check on, disarms.
	void ColorBot::update_trigger( const cv::Point2d& delta, const double head_h )
	{
		const double area = std::max<double>( std::max( 1, config_->trigger_.scan_area ), 0.35 * head_h );

		const bool inside  = std::abs( delta.x ) <= area && std::abs( delta.y ) <= area;
		const bool blocked = config_->trigger_.move_check && is_movement_key_pressed( );

		if ( !inside || blocked )
		{
			trigger_armed_ = false;
			return;
		}

		const auto now = Clock::now( );

		if ( !trigger_armed_ )
		{
			trigger_armed_ = true;
			trigger_since_ = now;
			return;
		}

		if ( now - trigger_since_ < std::chrono::milliseconds( std::max( 0, config_->trigger_.delay ) ) ||
		     now < trigger_next_shot_ )
			return;

		mouse_->left_click( );

		trigger_armed_     = false; // next shot needs a fresh delay
		trigger_next_shot_ = now + std::chrono::milliseconds( TRIGGER_COOLDOWN_MS );
	}

	// picks the head nearest the crosshair. for each blob: top = first row, head width = widest row among
	// rows 3-8 below the top (the head outline; shoulders start later), head height = width * HEAD_H_RATIO,
	// head x = centre of that widest row. with an anchor (holding) the nearest-to-anchor blob wins instead,
	// so a body / neighbour blob that looks closer cannot steal the lock.
	bool ColorBot::find_target( const cv::Mat& mask, Target& out, const cv::Point2d* anchor ) const
	{
		cv::Mat labels, stats, centroids;
		const std::int32_t count = cv::connectedComponentsWithStats( mask, labels, stats, centroids, 8, CV_32S );

		dbg_blobs_     = count - 1;
		dbg_big_blobs_ = 0;

		bool found       = false;
		double best_dist = std::numeric_limits<double>::max( );

		const cv::Point2d center( screen_center_.x, screen_center_.y );

		for ( std::int32_t i = 1; i < count; ++i )
		{
			if ( stats.at<std::int32_t>( i, cv::CC_STAT_AREA ) < MIN_BLOB_AREA )
				continue;

			++dbg_big_blobs_;

			const std::int32_t top    = stats.at<std::int32_t>( i, cv::CC_STAT_TOP );
			const std::int32_t left   = stats.at<std::int32_t>( i, cv::CC_STAT_LEFT );
			const std::int32_t width  = stats.at<std::int32_t>( i, cv::CC_STAT_WIDTH );
			const std::int32_t height = stats.at<std::int32_t>( i, cv::CC_STAT_HEIGHT );

			const std::int32_t row_begin = top + std::min( 3, height - 1 );
			const std::int32_t row_end   = top + std::min( 8, height - 1 );

			std::int32_t best_w = 0;
			double best_cx      = 0.0;

			for ( std::int32_t y = row_begin; y <= row_end; ++y )
			{
				std::int32_t x_min = std::numeric_limits<std::int32_t>::max( );
				std::int32_t x_max = -1;

				for ( std::int32_t x = left; x < left + width; ++x )
				{
					if ( labels.at<std::int32_t>( y, x ) == i )
					{
						x_min = std::min( x_min, x );
						x_max = std::max( x_max, x );
					}
				}

				if ( x_max < 0 )
					continue;

				const std::int32_t w = x_max - x_min + 1;

				if ( w > best_w )
				{
					best_w  = w;
					best_cx = ( x_min + x_max ) * 0.5;
				}
			}

			if ( best_w == 0 )
				continue;

			const double head_h = std::clamp( std::min( best_w * HEAD_H_RATIO, height * HEAD_MAX_FRAC ), 3.0, 120.0 );

			const cv::Point2d head( best_cx, top );
			const cv::Point2d head_center( best_cx, top + head_h * 0.5 );

			const double dist = anchor ? cv::norm( head - *anchor ) : cv::norm( head_center - center );

			if ( anchor && dist > ANCHOR_RADIUS )
				continue;

			if ( dist < best_dist )
			{
				best_dist  = dist;
				out.head   = head;
				out.head_h = head_h;
				found      = true;
			}
		}

		return found;
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
}
