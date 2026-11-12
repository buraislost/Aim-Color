#pragma once

namespace Core
{
	enum class KeyMode : std::uint8_t
	{
		HoldOn,
		Toggle,
	};

	struct KeyBind
	{
		int key_code    = 0;
		KeyMode mode    = KeyMode::HoldOn;
		bool is_toggled = false;
	};
}
