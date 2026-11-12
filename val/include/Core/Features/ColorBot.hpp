#pragma once

#include <chrono>
#include <random>

#include "Core/Config/Config.hpp"
#include "Core/BindSystem/BindSystem.hpp"
#include "Core/Mouse/Mouse.hpp"
#include "Core/Features/DebugState.hpp"

namespace Core::Features
{
	class ColorBot
	{
	public:
		explicit ColorBot( const ServiceHub& );

		void on_update( );

	private:
		using Clock = std::chrono::steady_clock;

		static constexpr double DEADZONE_RADIUS                 = 8.0;
		static constexpr double HOLD_RADIUS                     = 4.0;
		static constexpr double ON_TARGET_THRESHOLD             = 12.0;
		static constexpr std::int32_t ON_TARGET_FRAMES_REQUIRED = 5;
		static constexpr std::int32_t MIN_BLOB_AREA             = 12;
		static constexpr double STICKY_RADIUS                   = 40.0;
		static constexpr std::int32_t LOCK_LOST_FRAMES          = 15;

		// min gap between two trigger shots
		static constexpr std::int32_t TRIGGER_COOLDOWN_MS = 80;

		// capture box limits, config import can bypass the UI sliders
		static constexpr std::int32_t FOV_MIN = 20;
		static constexpr std::int32_t FOV_MAX = 250;

		std::shared_ptr<Config> config_;
		std::shared_ptr<BindSystem> bind_system_;
		std::shared_ptr<Mouse> mouse_;
		std::shared_ptr<DebugState> debug_;

		// set by find_target for the debug view
		mutable std::int32_t dbg_blobs_     = 0;
		mutable std::int32_t dbg_big_blobs_ = 0;
		std::uint64_t dbg_frames_           = 0;
		Clock::time_point dbg_last_preview_ = { };

		RECT monitor_;
		cv::Scalar lower_color_;
		cv::Scalar upper_color_;

		cv::Mat structuring_element_;
		cv::Point screen_center_;
		cv::Point prev_center_;

		bool is_on_target_             = false;
		std::int32_t on_target_frames_ = 0;

		// target lock
		bool has_lock_              = false;
		cv::Point locked_head_;
		std::int32_t lost_frames_   = 0;

		// trigger state: armed = head entered the scan area, fires once fire_at_ passes
		bool trigger_armed_                 = false;
		Clock::time_point trigger_fire_at_  = { };
		Clock::time_point trigger_next_shot_ = { };

		// lock mode: time of the last correction sent
		Clock::time_point last_lock_move_ = { };

		// humanize: reaction delay before a fresh lock starts moving, and correlated (AR(1)) drift noise
		Clock::time_point lock_acquired_at_ = { };
		double reaction_ms_                 = 0.0;
		bool   reacting_                    = false;
		double noise_x_                     = 0.0;
		double noise_y_                     = 0.0;

		std::mt19937 rng_ { std::random_device { }( ) };

		[[nodiscard]] double rand_real( double lo, double hi );

		[[nodiscard]] cv::Mat screen( ) const;
		[[nodiscard]] bool find_target( const cv::Mat& mask, cv::Point& out ) const;

		[[nodiscard]] bool is_movement_key_pressed( ) const;

		void reset_state( );
		void update_trigger( const cv::Point& head );
		void move( const cv::Point& delta );
		void lock_move( const cv::Point& delta );
	};
}
