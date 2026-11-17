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
		constexpr float kLauncherLogo    = 64.f;
		constexpr float kLauncherBtnW    = 280.f;
		constexpr float kLauncherBtnH    = 48.f;
		constexpr float kLauncherLogoY   = 160.f;
		constexpr float kLauncherTitleY  = 248.f;
		constexpr float kLauncherStatusY = 312.f;
		constexpr float kLauncherButtonY = 400.f;

		// ===== Internal helpers =====
		void draw_logo( const ImVec2 pos, const float size )
		{
			auto* dl = ImGui::GetWindowDrawList( );
			dl->AddRectFilled( pos, pos + ImVec2( size, size ), ImGui::GetColorU32( th::kAccent ), th::kRadius );

			const ImVec2 glyph = ImGui::CalcTextSize( "B" );
			dl->AddText( { pos.x + ( size - glyph.x ) * 0.5f, pos.y + ( size - glyph.y ) * 0.5f },
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

		void bind_field( const char* caption, const char* id, KeyBind& bind, const BindSystem& binds )
		{
			wd::caption( caption );
			binds.render_keybind_button( id, bind, true, { -1.f, 0.f } );
		}

		void target_group( Config& config )
		{
			static const char* const colors[ ] = { "Magenta", "Yellow", "Red" };
			const float w = wd::half_width( );

			wd::combo( "Color", &config.settings_.color, colors, 3, w );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::slider_int( "Tolerance", &config.aimbot_.tolerance, 0, 60, "%d", w );
		}

		void aim_group( Config& config, const BindSystem& binds )
		{
			auto& ab      = config.aimbot_;
			const float w = wd::half_width( );

			wd::section( "AIM" );
			bind_field( "Bind", "Aimbot", ab.bind, binds );

			wd::slider_int( "FOV X", &ab.fov_x, 20, 250, "%dpx", w );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::slider_int( "FOV Y", &ab.fov_y, 20, 250, "%dpx", w );

			wd::slider_int( "Offset X", &ab.offset_x, -15, 15, "%dpx", w );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::slider_int( "Offset Y", &ab.offset_y, -15, 15, "%dpx", w );
		}

		void motion_group( Config& config )
		{
			auto& ab      = config.aimbot_;
			const float w = wd::half_width( );

			wd::section( "MOTION" );
			wd::slider_float( "Gain", &ab.lock_gain, 0.1f, 5.f, "%.2f", w );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::slider_float( "Smooth", &ab.smooth, 0.1f, 1.f, "%.2f", w );

			wd::slider_float( "Speed X", &ab.speed_x, 0.2f, 3.f, "%.2f", w );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::slider_float( "Speed Y", &ab.speed_y, 0.2f, 3.f, "%.2f", w );

			wd::slider_int( "Hold", &ab.hold_pct, 0, 150, "%d%%", w );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::slider_int( "Settle", &ab.lock_settle_ms, 1, 30, "%dms", w );

			wd::slider_int( "Reaction delay", &ab.reaction_ms, 0, 150, "%dms", w );
			ImGui::SameLine( 0.f, th::kSpace2 );
			wd::slider_int( "Y limit", &ab.y_limit, 0, 30, ab.y_limit == 0 ? "none" : "%dpx", w );
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
			wd::toggle( "Y axis", &ab.y_axis, cell );
		}

		void center_text( const char* text, const ImVec4& color )
		{
			const float w = ImGui::CalcTextSize( text ).x;
			ImGui::SetCursorPosX( ( ImGui::GetWindowWidth( ) - w ) * 0.5f );
			ImGui::TextColored( color, "%s", text );
		}

		void center_logo( )
		{
			ImGui::SetCursorPos( { ( ImGui::GetWindowWidth( ) - kLauncherLogo ) * 0.5f, kLauncherLogoY } );
			draw_logo( ImGui::GetCursorScreenPos( ), kLauncherLogo );
			ImGui::Dummy( { kLauncherLogo, kLauncherLogo } );
		}

		void center_title( )
		{
			ImGui::SetCursorPosY( kLauncherTitleY );
			ImGui::SetWindowFontScale( th::kTitleScale );
			center_text( "Bura", th::kText );
			ImGui::SetWindowFontScale( 1.f );
			center_text( "color tracking", th::kMuted );
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
	HeaderResult header( const bool connected, const bool show_home )
	{
		HeaderResult out;
		const float width = ImGui::GetWindowWidth( );
		const float y     = ImGui::GetCursorPosY( );
		const ImVec2 pos  = ImGui::GetCursorScreenPos( );
		float right       = width - th::kSpace3;

		draw_logo( pos, th::kLogoSize );
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

		if ( show_home ) {
			right -= th::kSpace2 + kWebButtonW;
			ImGui::SetCursorPos( { right, y } );
			out.home_clicked = ImGui::Button( "Home", { kWebButtonW, kHeaderButton } );
		}

		draw_status( connected, right - th::kSpace3, y );
		ImGui::SetCursorPos( { th::kSpace3, y + kHeaderButton + th::kSpace2 } );
		return out;
	}

	void aimbot( Config& config, const BindSystem& binds )
	{
		target_group( config );
		aim_group( config, binds );
		motion_group( config );
		behaviour_group( config );
	}

	void trigger( Config& config, const BindSystem& binds )
	{
		auto& tr      = config.trigger_;
		const float w = wd::half_width( );

		wd::section( "TRIGGER" );
		bind_field( "Bind", "Trigger", tr.bind, binds );
		wd::slider_int( "Scan area", &tr.scan_area, 1, 30, "%dpx", w );
		ImGui::SameLine( 0.f, th::kSpace2 );
		wd::slider_int( "Delay", &tr.delay, 1, 300, "%dms", w );
		wd::toggle( "Move check", &tr.move_check );
	}

	void settings( Config& config, Mouse& mouse, char* com_buf )
	{
		static const char* const backends[ ] = { "SendInput (local)", "Makcu (serial)", "Medius (capi dll)" };
		auto& st = config.settings_;

		wd::section( "INPUT" );
		wd::combo( "Device", &st.input_backend, backends, 3 );

		if ( st.input_backend == 1 ) {
			wd::caption( "COM port" );
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

	// cover screen shown first: big logo, status and one button into the menu
	LauncherResult launcher( const bool connected, const Mouse& mouse )
	{
		LauncherResult out;
		const float w = ImGui::GetWindowWidth( );
		const float h = ImGui::GetWindowHeight( );

		ImGui::SetCursorPos( { w - th::kSpace3 - th::kRowHeight, th::kSpace3 } );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, th::kBad );
		out.close_clicked = ImGui::Button( "x", { th::kRowHeight, th::kRowHeight } );
		ImGui::PopStyleColor( );

		center_logo( );
		center_title( );
		ImGui::SetCursorPosY( kLauncherStatusY );
		center_text( connected ? "Connected" : "No input", connected ? th::kOk : th::kBad );

		ImGui::SetCursorPos( { ( w - kLauncherBtnW ) * 0.5f, kLauncherButtonY } );
		out.open_menu = wd::accent_button( "Open menu", { kLauncherBtnW, kLauncherBtnH } );

		ImGui::SetCursorPos( { th::kSpace3, h - th::kSpace3 - th::kRowHeight } );
		ImGui::TextColored( th::kMuted, "Input: %s", mouse.status( ).c_str( ) );
		return out;
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
