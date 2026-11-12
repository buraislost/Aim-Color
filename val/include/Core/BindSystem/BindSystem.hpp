#pragma once

#include "Core/BindSystem/KeyBind.hpp"

namespace Core
{
	class BindSystem
	{
	public:
		explicit BindSystem( ) = default;

		bool is_active( KeyBind& ) const;

		void render_keybind_button( const char*, KeyBind&, bool = true, const ImVec2& = { 80.f, 20.f } ) const;

	private:
		[[nodiscard]] std::string get_key_name( std::int32_t ) const;

		mutable std::vector<std::pair<KeyBind*, const char*>> registered_binds_;
		mutable std::unordered_map<int, bool> key_was_pressed_;

		mutable std::unordered_map<std::string, bool> getting_key_states_;
		mutable std::unordered_map<std::string, std::string> button_text_states_;
	};
}
