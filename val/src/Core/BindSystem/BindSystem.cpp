#include "Stdafx.hpp"
#include "Core/BindSystem/BindSystem.hpp"

namespace Core
{
	bool BindSystem::is_active( KeyBind& bind ) const
	{
		if ( bind.key_code == 0 &&
		     bind.mode == KeyMode::HoldOn )
		{
			return false;
		}

		switch ( bind.mode )
		{
			case KeyMode::HoldOn:
			{
				return GetAsyncKeyState( bind.key_code ) & 0x8000;
			}
			case KeyMode::Toggle:
			{
				if ( GetAsyncKeyState( bind.key_code ) & 1 )
				{
					bind.is_toggled = !bind.is_toggled;
				}

				return bind.is_toggled;
			}
		}

		return false;
	}

	void BindSystem::render_keybind_button( const char* name, KeyBind& bind, const bool show_options, const ImVec2& size ) const
	{
		bool found = false;

		for ( const auto& key : registered_binds_ | std::views::keys )
		{
			if ( key == &bind )
			{
				found = true;
				break;
			}
		}

		if ( !found )
		{
			registered_binds_.emplace_back( &bind, name );
		}

		ImGui::PushID( name );

		auto& is_getting_key = getting_key_states_[ name ];
		auto& button_text    = button_text_states_[ name ];

		if ( is_getting_key )
		{
			button_text = "...";

			for ( int i = 1; i < 256; ++i )
			{
				if ( GetAsyncKeyState( i ) & 0x8000 )
				{
					bind.key_code  = i == VK_ESCAPE ? 0 : i;
					is_getting_key = false;
					button_text    = get_key_name( bind.key_code );
					break;
				}
			}
		}
		else
		{
			button_text = get_key_name( bind.key_code );
		}

		const std::string final_button_label = button_text + "##" + name;

		if ( ImGui::Button( final_button_label.c_str( ), size ) )
		{
			is_getting_key = true;
		}

		ImGui::PushStyleVar( ImGuiStyleVar_PopupRounding, 8.0f );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 8, 8 ) );

		if ( show_options && ImGui::BeginPopupContextItem( "bind_context_menu" ) )
		{
			if ( ImGui::MenuItem( "Hold On", nullptr, bind.mode == KeyMode::HoldOn ) )
			{
				bind.mode = KeyMode::HoldOn;
			}
			if ( ImGui::MenuItem( "Toggle", nullptr, bind.mode == KeyMode::Toggle ) )
			{
				bind.mode = KeyMode::Toggle;
			}
			ImGui::EndPopup( );
		}

		ImGui::PopStyleVar( 2 );

		ImGui::PopID( );
	}

	std::string BindSystem::get_key_name( const std::int32_t id ) const
	{
		static std::unordered_map<std::int32_t, std::string> key_names =
		{
			{ 0, "None" },
			{ VK_LBUTTON, "M1" },
			{ VK_RBUTTON, "M2" },
			{ VK_MBUTTON, "M3" },
			{ VK_XBUTTON1, "M4" },
			{ VK_XBUTTON2, "M5" },
			{ VK_BACK, "Back" },
			{ VK_TAB, "Tab" },
			{ VK_CLEAR, "Clear" },
			{ VK_RETURN, "Enter" },
			{ VK_SHIFT, "Shift" },
			{ VK_CONTROL, "Ctrl" },
			{ VK_MENU, "Alt" },
			{ VK_PAUSE, "Pause" },
			{ VK_CAPITAL, "Caps Lock" },
			{ VK_ESCAPE, "Escape" },
			{ VK_SPACE, "Space" },
			{ VK_PRIOR, "Page Up" },
			{ VK_NEXT, "Page Down" },
			{ VK_END, "End" },
			{ VK_HOME, "Home" },
			{ VK_LEFT, "Left Key" },
			{ VK_UP, "Up Key" },
			{ VK_RIGHT, "Right Key" },
			{ VK_DOWN, "Down Key" },
			{ VK_SELECT, "Select" },
			{ VK_PRINT, "Print Screen" },
			{ VK_INSERT, "Insert" },
			{ VK_DELETE, "Delete" },
			{ VK_HELP, "Help" },
			{ VK_SLEEP, "Sleep" },
			{ VK_MULTIPLY, "*" },
			{ VK_ADD, "+" },
			{ VK_SUBTRACT, "-" },
			{ VK_DECIMAL, "." },
			{ VK_DIVIDE, "/" },
			{ VK_NUMLOCK, "Num Lock" },
			{ VK_SCROLL, "Scroll" },
			{ VK_LSHIFT, "Left Shift" },
			{ VK_RSHIFT, "Right Shift" },
			{ VK_LCONTROL, "Left Ctrl" },
			{ VK_RCONTROL, "Right Ctrl" },
			{ VK_LMENU, "Left Alt" },
			{ VK_RMENU, "Right Alt" },
		};

		if ( id >= 0x30 && id <= 0x5A )
			return std::string( 1, StCa<char>( id ) );

		if ( id >= 0x60 && id <= 0x69 )
			return "Num " + std::to_string( id - 0x60 );

		if ( id >= 0x70 && id <= 0x87 )
			return "F" + std::to_string( ( id - 0x70 ) + 1 );

		return key_names[ id ];
	}
}
