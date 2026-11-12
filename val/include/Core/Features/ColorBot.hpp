#pragma once

#include <chrono>
#include <deque>
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

		// one enemy head as seen in the mask: x = centre of the head, y = top of the outline, head_h = head height in px
		struct Target
		{
			cv::Point2d head { 0.0, 0.0 };
			double head_h = 0.0;
		};

		// a correction already sent to the mouse, not yet visible on screen
		struct Sent
		{
			Clock::time_point t;
			std::int32_t dx;
			std::int32_t dy;
		};

		// pixel thresholds are tuned at 1080p and scaled with the real resolution
		static constexpr double DEADZONE_PX = 0.8;

		static constexpr std::int32_t MIN_BLOB_AREA = 12; // smaller blobs are noise

		// head height / head width, measured on the range bot from front, side and back (1.1 - 1.2)
		static constexpr double HEAD_H_RATIO = 1.15;

		// the head is never more than this fraction of the whole blob. matters for far / half-hidden enemies:
		// a blob 10px tall has shoulders inside rows 3-8, the width rule alone would call the head 9px tall.
		static constexpr double HEAD_MAX_FRAC = 0.45;

		// hold: crosshair inside the head box -> the aim stops moving. it resumes when the crosshair is
		// HOLD_RELEASE_MUL times farther out than the box, so a drifting enemy is followed again.
		static constexpr double HOLD_RELEASE_MUL = 1.6;
		static constexpr double ANCHOR_RADIUS    = 60.0; // while holding, only blobs this close to the held head count

		// the blob top flickers by a pixel or two per frame; the aim point is low-passed with this time constant
		static constexpr double AIM_FILTER_MS = 18.0;
		static constexpr double JUMP_PX       = 40.0; // aim point moved farther than this in a frame: new target, no filtering

		static constexpr std::int32_t LOST_MS    = 120; // no target for this long: drop hold / filter / reaction
		static constexpr std::int32_t PENDING_MS = 24;  // how long a sent correction is assumed to be still on its way to the screen
		static constexpr double EASE_MIN         = 0.35; // fraction of the gain left right next to the aim point

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
		cv::Mat structuring_element_;
		cv::Point screen_center_;

		bool is_on_target_ = false;

		// low-passed aim point and head height
		bool filter_valid_            = false;
		cv::Point2d aim_f_            = { 0.0, 0.0 };
		double head_h_f_              = 0.0;
		Clock::time_point filter_time_ = { };
		Clock::time_point last_seen_   = { };

		// hold state
		bool holding_ = false;
		cv::Point2d hold_head_ = { 0.0, 0.0 };

		// aim state
		bool acquired_                 = false;
		Clock::time_point acquired_at_ = { };
		double reaction_ms_            = 0.0;
		Clock::time_point last_move_   = { };
		double rem_x_                  = 0.0; // sub-count remainder carried to the next report
		double rem_y_                  = 0.0;
		std::deque<Sent> sent_;

		// trigger: armed while the crosshair stays on the head, fires once `delay` ms passed
		bool trigger_armed_                  = false;
		Clock::time_point trigger_since_     = { };
		Clock::time_point trigger_next_shot_ = { };

		std::mt19937 rng_ { std::random_device { }( ) };

		[[nodiscard]] double rand_real( double lo, double hi );

		[[nodiscard]] cv::Mat screen( ) const;
		void build_mask( const cv::Mat& hsv, cv::Mat& mask ) const;
		[[nodiscard]] bool find_target( const cv::Mat& mask, Target& out, const cv::Point2d* anchor ) const;

		[[nodiscard]] bool is_movement_key_pressed( ) const;

		void reset_state( );
		void aim( const cv::Point2d& delta, double head_h, double px_scale );
		void update_trigger( const cv::Point2d& delta, double head_h );
	};
}
