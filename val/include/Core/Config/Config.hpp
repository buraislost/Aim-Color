#pragma once

#include "Core/BindSystem/KeyBind.hpp"

NLOHMANN_JSON_SERIALIZE_ENUM
(
	Core::KeyMode,
	{
	{ Core::KeyMode::HoldOn, "HoldOn" },
	{ Core::KeyMode::Toggle, "Toggle" },
	}
)

namespace Core
{
	using json = nlohmann::json;

	inline void to_json( json& j, const KeyBind& p )
	{
		j = json { { "key_code", p.key_code }, { "mode", p.mode } };
	}

	inline void from_json( const json& j, KeyBind& p )
	{
		j.at( "key_code" ).get_to( p.key_code );
		j.at( "mode" ).get_to( p.mode );
	}

	class Config
	{
		std::uint32_t encryption_key_ = 0x13371337;

	public:
		[[nodiscard]] std::string export_to_str( ) const;
		[[nodiscard]] bool import_from_str( const std::string& );

		// every field is serialized; WITH_DEFAULT keeps configs exported by older builds loadable
		struct Aimbot
		{
			KeyBind bind {};

			std::int32_t fov_x    = 60;
			std::int32_t fov_y    = 50;
			// extra px added to the aim point, which already sits at the centre of the head (0 = dead centre)
			std::int32_t offset_x = 0;
			std::int32_t offset_y = 0;

			// crosshair inside the head box (this % of the head size) -> hold still instead of chasing the colour. 0 = off
			std::int32_t hold_pct = 60;

			// 1 = close the whole gap per report (snap), lower = ease in
			float smooth = 0.5f;

			// true = aim runs without holding the bind
			bool auto_aim = false;

			// aim sends counts = error * lock_gain * smooth, one report every lock_settle_ms.
			// lock_gain = mouse counts per screen pixel of error (depends on in-game sens).
			// `lock` is kept only so configs exported by older builds still load, it is unused.
			bool lock                  = false;
			float lock_gain            = 1.f;
			std::int32_t lock_settle_ms = 10;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT( Aimbot, bind, fov_x, fov_y, offset_x, offset_y, hold_pct, smooth, auto_aim,
			                                              lock, lock_gain, lock_settle_ms )
		} aimbot_;

		struct Trigger
		{
			KeyBind bind {};

			std::int32_t delay     = 20;
			std::int32_t scan_area = 7;

			bool move_check = false;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT( Trigger, bind, delay, scan_area, move_check )
		} trigger_;

		struct Settings
		{
			float sensitivity  = 0.4f;
			std::int32_t color = 0;

			// 0 = SendInput (local), 1 = Makcu over serial, 2 = Medius (medius_capi.dll)
			std::int32_t input_backend = 0;
			std::string com_port       = "COM3";

			NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT( Settings, sensitivity, color, input_backend, com_port )
		} settings_;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT( Config, aimbot_, trigger_, settings_ )
	};
}
