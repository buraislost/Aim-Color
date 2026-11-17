// ui/Panels.cpp — draws the header, the Aimbot / Trigger / Settings tabs, footer and the close prompt
// Depends: ui/Panels.hpp, ui/Widgets.hpp, core/Constants.hpp

// ===== Includes =====
#include "Stdafx.hpp"
#include "Core/UI/Panels.hpp"
#include "Core/UI/Widgets.hpp"
#include "Core/Constants.hpp"

#include <shellapi.h>

namespace Core::UI::panel
{
	namespace th = theme;
	namespace wd = widget;

	// ===== Constants =====
	namespace
	{
		constexpr float kHeaderButton = th::kRowHeight;
		constexpr float kWebButtonW   = 48.f;
		constexpr float kClosePopupW  = 208.f;

		// ===== Internal helpers =====
		void draw_logo( const ImVec2 pos )
		{
			auto* dl = ImGui::GetWindowDrawList( );
			dl->AddRectFilled( pos, pos + ImVec2( th::kLogoSize, th::kLogoSize ), ImGui::GetColorU32( th::kAccent ), th::kRadius );

			const ImVec2 glyph = ImGui::CalcTextSize( "B" );
			dl->AddText( { pos.x + ( th::kLogoSize - glyph.x ) * 0.5f, pos.y + ( th::kLogoSize - glyph.y ) * 0.5f },
			             IM_COL32_WHITE, "B" );
		}

		// the single heading size: same font, scaled
		void draw_title( const ImVec2 pos )
		{
			auto* dl = ImGui::GetWindowDrawList( );
			dl->AddText( ImGui::GetFont( ), ImGui::GetFontSize( ) * th::kTitleScale, pos,
			             ImGui::GetColorU32( th::kText ), "Bura" );
			dl->AddText( { pos.x, pos.y + th::kSpace3 + th::kSpace1 }, ImGui::GetColorU32( th::kMuted ), "color tracking" );
		}

		void draw_status( const bool connected, const float right_edge, const float y )
		{
			const char* text = connected ? "Connected" : "No input";
			const ImVec4& c  = connected ? th::kOk : th::kBad;
			const float w    = ImGui::CalcTextSize( text ).x;
			const ImVec2 win = ImGui::GetWindowPos( );
			const float x    = right_edge - w;

			ImGui::SetCursorPos( { x, y + ( kHeaderButton - ImGui::GetTextLineHeight( ) ) * 0.5f } );
			ImGui::TextColored( c, "%s", text );
			ImGui::GetWindowDrawList( )->AddCircleFilled( { win.x + x - th::kSpace2, win.y + y + kHeaderButton * 0.5f }, 4.f,
			                                              ImGui::GetColorU32( c ) );
		}

		void bind_row( const char* caption, const char* id, KeyBind& bind, const BindSystem& binds )
		{
			wd::label( caption );
			binds.render_keybind_button( id, bind, true, { -1.f, 0.f } );
		}

		void aim_group( Config& config, const BindSystem& binds )
		{
			auto& ab = config.aimbot_;

			wd::section( "AIM" );
			bind_row( "Bind", "Aimbot", ab.bind, binds );
			wd::pair_int( "FOV", &ab.fov_x, &ab.fov_y, { 20, 250 }, "X %dpx", "Y %dpx" );
			wd::pair_int( "Offset", &ab.offset_x, &ab.offset_y, { -15, 15 }, "X %dpx", "Y %dpx" );
		}

		void motion_group( Config& config )
		{
			auto& ab = config.aimbot_;

			wd::section( "MOTION" );
			wd::pair_float( "Gain / Smooth", &ab.lock_gain, &ab.smooth, { 0.1f, 5.f }, "G %.2f", "S %.2f" );
			ab.smooth = std::min( ab.smooth, 1.f );
			wd::pair_float( "Speed", &ab.speed_x, &ab.speed_y, { 0.2f, 3.f }, "X %.2f", "Y %.2f" );
		}

		void timing_group( Config& config )
		{
			auto& ab = config.aimbot_;

			wd::section( "TIMING" );
			wd::slider_int( "Hold", &ab.hold_pct, 0, 150, "%d%% of head" );
			wd::pair_int( "React / Settle", &ab.reaction_ms, &ab.lock_settle_ms, { 0, 150 }, "%dms", "%dms" );
			ab.lock_settle_ms = std::clamp( ab.lock_settle_ms, 1, 30 );
		}

		void behaviour_group( Config& config )
		{
			auto& ab = config.aimbot_;

			wd::section( "BEHAVIOUR" );
			const float cell = wd::half_width( );

			wd::toggle( "Auto aim", &ab.auto_aim, cell );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::toggle( "Legit", &ab.legit, cell );
			wd::toggle( "Anti-Strafe", &ab.anti_strafe, cell );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::toggle( "Humanize", &ab.humanize, cell );
		}

		void config_buttons( Config& config, Mouse& mouse, char* com_buf )
		{
			const float cell = wd::half_width( );

			if ( ImGui::Button( "Copy to clipboard", { cell, th::kRowHeight } ) ) {
				ImGui::SetClipboardText( config.export_to_str( ).c_str( ) );
			}

			ImGui::SameLine( 0.f, th::kSpace2 );

			if ( !ImGui::Button( "Import from clipboard", { cell, th::kRowHeight } ) ) {
				return;
			}

			if ( !config.import_from_str( ImGui::GetClipboardText( ) ) ) {
				MessageBoxA( nullptr, "Failed to import config!", "Bura", MB_OK | MB_ICONERROR );
				return;
			}

			strncpy_s( com_buf, 16, config.settings_.com_port.c_str( ), _TRUNCATE );
			mouse.configure( config.settings_.input_backend, config.settings_.com_port );
		}
	}

	// ===== Public API =====
	HeaderResult header( const bool connected )
	{
		HeaderResult out;
		const float width = ImGui::GetWindowWidth( );
		const float y     = ImGui::GetCursorPosY( );
		const ImVec2 pos  = ImGui::GetCursorScreenPos( );
		float right       = width - th::kSpace3;

		draw_logo( pos );
		draw_title( { pos.x + th::kLogoSize + th::kSpace2, pos.y } );

		right -= kHeaderButton;
		ImGui::SetCursorPos( { right, y } );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, th::kBad );
		out.close_clicked = ImGui::Button( "x", { kHeaderButton, kHeaderButton } );
		ImGui::PopStyleColor( );

		if ( kWebUiEnabled ) {
			right -= th::kSpace2 + kWebButtonW;
			ImGui::SetCursorPos( { right, y } );
			out.web_clicked = ImGui::Button( "Web", { kWebButtonW, kHeaderButton } );
		}

		draw_status( connected, right - th::kSpace3, y );
		ImGui::SetCursorPos( { th::kSpace3, y + kHeaderButton + th::kSpace2 } );
		return out;
	}

	void aimbot( Config& config, const BindSystem& binds )
	{
		static const char* const colors[ ] = { "Magenta", "Yellow", "Red" };

		wd::section( "TARGET" );
		wd::combo( "Color", &config.settings_.color, colors, 3 );
		wd::slider_int( "Tolerance", &config.aimbot_.tolerance, 0, 60 );

		aim_group( config, binds );
		motion_group( config );
		timing_group( config );
		behaviour_group( config );
	}

	void trigger( Config& config, const BindSystem& binds )
	{
		auto& tr = config.trigger_;

		wd::section( "TRIGGER" );
		bind_row( "Bind", "Trigger", tr.bind, binds );
		wd::slider_int( "Scan area", &tr.scan_area, 1, 30, "%dpx" );
		wd::slider_int( "Delay", &tr.delay, 1, 300, "%dms" );
		wd::toggle( "Move check", &tr.move_check );
	}

	void settings( Config& config, Mouse& mouse, char* com_buf )
	{
		static const char* const backends[ ] = { "SendInput (local)", "Makcu (serial)", "Medius (capi dll)" };
		auto& st = config.settings_;

		wd::section( "INPUT" );
		wd::combo( "Device", &st.input_backend, backends, 3 );

		if ( st.input_backend == 1 ) {
			wd::label( "COM port" );
			ImGui::SetNextItemWidth( -1.f );

			if ( ImGui::InputText( "##com_port", com_buf, 16 ) ) {
				st.com_port = com_buf;
			}
		}

		if ( wd::accent_button( "Apply input", { -1.f, th::kRowHeight } ) ) {
			mouse.configure( st.input_backend, st.com_port );
		}

		wd::section( "CONFIG" );
		config_buttons( config, mouse, com_buf );
	}

	void footer( const Mouse& mouse )
	{
		ImGui::TextColored( th::kMuted, "Input: %s", mouse.status( ).c_str( ) );
	}

	CloseChoice close_prompt( const bool open )
	{
		if ( open ) {
			ImGui::OpenPopup( "Close Bura" );
		}

		const ImVec2 center = { ImGui::GetWindowWidth( ) * 0.5f, ImGui::GetWindowHeight( ) * 0.5f };
		ImGui::SetNextWindowPos( center, ImGuiCond_Always, { 0.5f, 0.5f } );

		if ( !ImGui::BeginPopupModal( "Close Bura", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove ) ) {
			return CloseChoice::None;
		}

		CloseChoice choice = CloseChoice::None;
		const float half   = ( kClosePopupW - th::kSpace2 ) * 0.5f;

		ImGui::TextColored( th::kMuted, "Keep Bura running in the system tray?" );

		if ( wd::accent_button( "Minimize to tray", { kClosePopupW, th::kRowHeight } ) ) {
			choice = CloseChoice::Tray;
		}

		if ( ImGui::Button( "Quit", { half, th::kRowHeight } ) ) {
			choice = CloseChoice::Quit;
		}

		ImGui::SameLine( 0.f, th::kSpace2 );

		if ( ImGui::Button( "Cancel", { half, th::kRowHeight } ) ) {
			ImGui::CloseCurrentPopup( );
		}

		if ( choice != CloseChoice::None ) {
			ImGui::CloseCurrentPopup( );
		}

		ImGui::EndPopup( );
		return choice;
	}
}
