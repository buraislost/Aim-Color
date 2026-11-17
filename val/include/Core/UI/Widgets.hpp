#pragma once

// ui/Widgets.hpp — controls: section, slider field (label + value + thin track), combo, switch, tabs
// Depends: ui/Theme.hpp

#include "Core/UI/Theme.hpp"

namespace Core::UI::widget
{
	namespace th = theme;

	// ===== Constants =====
	inline constexpr float kFieldHeight = 36.f; // label row + track
	inline constexpr float kTrackHeight = 4.f;
	inline constexpr float kKnobRadius  = 7.f;

	// ===== Layout helpers =====
	inline float half_width( )
	{
		return ( ImGui::GetContentRegionAvail( ).x - th::kSpace2 ) * 0.5f;
	}

	inline float full_width( )
	{
		return ImGui::GetContentRegionAvail( ).x;
	}

	// caption followed by a hairline that runs to the right edge
	inline void section( const char* title )
	{
		ImGui::Dummy( { 0.f, 0.f } );

		const ImVec2 pos   = ImGui::GetCursorScreenPos( );
		const ImVec2 text  = ImGui::CalcTextSize( title );
		const float line_y = pos.y + text.y * 0.5f;

		ImGui::TextColored( th::kMuted, "%s", title );
		ImGui::GetWindowDrawList( )->AddLine( { pos.x + text.x + th::kSpace2, line_y }, { pos.x + full_width( ), line_y },
		                                     ImGui::GetColorU32( th::kBorder ) );
	}

	inline void caption( const char* text )
	{
		ImGui::TextColored( th::kMuted, "%s", text );
	}

	// ===== Slider field =====
	namespace detail
	{
		// draws label (left), value (right) and the track; returns true while the user drags. `t` is 0..1
		inline bool track_field( const char* label, const char* value, const float width, float& t )
		{
			const ImVec2 pos = ImGui::GetCursorScreenPos( );
			ImGui::InvisibleButton( "##field", { width, kFieldHeight } );

			bool changed = false;

			if ( ImGui::IsItemActive( ) ) {
				const float span = width - 2.f * kKnobRadius;
				const float next = std::clamp( ( ImGui::GetIO( ).MousePos.x - pos.x - kKnobRadius ) / span, 0.f, 1.f );

				changed = next != t;
				t       = next;
			}

			auto* dl         = ImGui::GetWindowDrawList( );
			const float cy   = pos.y + kFieldHeight - kKnobRadius - 1.f;
			const float x0   = pos.x + kKnobRadius;
			const float x1   = pos.x + width - kKnobRadius;
			const float knob = x0 + ( x1 - x0 ) * t;
			const bool hot   = ImGui::IsItemHovered( ) || ImGui::IsItemActive( );

			dl->AddText( pos, ImGui::GetColorU32( th::kMuted ), label );
			dl->AddText( { pos.x + width - ImGui::CalcTextSize( value ).x, pos.y }, ImGui::GetColorU32( th::kAccentHi ), value );

			dl->AddRectFilled( { x0, cy - kTrackHeight * 0.5f }, { x1, cy + kTrackHeight * 0.5f }, ImGui::GetColorU32( th::kSurfaceHi ), 2.f );
			dl->AddRectFilled( { x0, cy - kTrackHeight * 0.5f }, { knob, cy + kTrackHeight * 0.5f }, ImGui::GetColorU32( th::kAccent ), 2.f );
			dl->AddCircleFilled( { knob, cy }, hot ? kKnobRadius : kKnobRadius - 1.f, ImGui::GetColorU32( th::kText ) );
			dl->AddCircle( { knob, cy }, hot ? kKnobRadius : kKnobRadius - 1.f, ImGui::GetColorU32( th::kAccent ), 0, 2.f );
			return changed;
		}
	}

	// width 0 = whole row. fmt formats the value shown on the right (e.g. "%dpx", "%d%% of head")
	inline void slider_int( const char* label, std::int32_t* v, const int lo, const int hi, const char* fmt = "%d", float width = 0.f )
	{
		char text[ 32 ];
		snprintf( text, sizeof( text ), fmt, *v );

		if ( width <= 0.f ) { width = full_width( ); }

		float t = static_cast<float>( *v - lo ) / static_cast<float>( hi - lo );

		ImGui::PushID( label );

		if ( detail::track_field( label, text, width, t ) ) {
			*v = lo + static_cast<int>( std::lround( t * static_cast<float>( hi - lo ) ) );
		}

		ImGui::PopID( );
	}

	inline void slider_float( const char* label, float* v, const float lo, const float hi, const char* fmt = "%.2f", float width = 0.f )
	{
		char text[ 32 ];
		snprintf( text, sizeof( text ), fmt, *v );

		if ( width <= 0.f ) { width = full_width( ); }

		float t = ( *v - lo ) / ( hi - lo );

		ImGui::PushID( label );

		if ( detail::track_field( label, text, width, t ) ) {
			*v = lo + t * ( hi - lo );
		}

		ImGui::PopID( );
	}

	// ===== Combo / button fields (caption above the control) =====
	inline void combo( const char* label, std::int32_t* v, const char* const* items, const int count, float width = 0.f )
	{
		if ( width <= 0.f ) { width = full_width( ); }

		ImGui::PushID( label );
		ImGui::BeginGroup( );
		caption( label );
		ImGui::SetNextItemWidth( width );
		ImGui::Combo( "##v", v, items, count );
		ImGui::EndGroup( );
		ImGui::PopID( );
	}

	inline bool accent_button( const char* text, const ImVec2 size )
	{
		ImGui::PushStyleColor( ImGuiCol_Button, th::kAccent );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, th::kAccentHi );
		const bool pressed = ImGui::Button( text, size );
		ImGui::PopStyleColor( 2 );
		return pressed;
	}

	// ===== Switch cell: label left, pill right. width 0 = whole row =====
	inline void toggle( const char* name, bool* v, float width = 0.f )
	{
		if ( width <= 0.f ) { width = full_width( ); }

		ImGui::PushID( name );
		const ImVec2 pos  = ImGui::GetCursorScreenPos( );
		const ImVec2 size = { width, th::kRowHeight };

		if ( ImGui::InvisibleButton( "##t", size ) ) {
			*v = !*v;
		}

		auto* dl           = ImGui::GetWindowDrawList( );
		const float text_y = pos.y + ( size.y - ImGui::GetTextLineHeight( ) ) * 0.5f;
		const ImVec2 track = { pos.x + size.x - 40.f, pos.y + ( size.y - 16.f ) * 0.5f };

		dl->AddRectFilled( pos, pos + size, ImGui::GetColorU32( ImGui::IsItemHovered( ) ? th::kSurfaceHi : th::kBackground ), th::kRadius );
		dl->AddText( { pos.x + th::kSpace2, text_y }, ImGui::GetColorU32( *v ? th::kText : th::kMuted ), name );
		dl->AddRectFilled( track, track + ImVec2( 32.f, 16.f ), ImGui::GetColorU32( *v ? th::kAccent : th::kSurfaceHi ), 8.f );
		dl->AddCircleFilled( { track.x + ( *v ? 24.f : 8.f ), track.y + 8.f }, 6.f, ImGui::GetColorU32( *v ? th::kText : th::kMuted ) );
		ImGui::PopID( );
	}

	// ===== Segmented tab strip; `sel` is the active index =====
	inline void tabs( const char* const* names, const int count, int& sel )
	{
		const float w = ( full_width( ) - th::kSpace1 * ( count - 1 ) ) / static_cast<float>( count );

		for ( int i = 0; i < count; ++i ) {
			const bool on = i == sel;

			if ( i > 0 ) {
				ImGui::SameLine( 0.f, th::kSpace1 );
			}

			ImGui::PushStyleColor( ImGuiCol_Button, on ? th::kAccent : th::kSurface );
			ImGui::PushStyleColor( ImGuiCol_ButtonHovered, on ? th::kAccent : th::kSurfaceHi );
			ImGui::PushStyleColor( ImGuiCol_Text, on ? th::kText : th::kMuted );

			if ( ImGui::Button( names[ i ], { w, th::kRowHeight } ) ) {
				sel = i;
			}

			ImGui::PopStyleColor( 3 );
		}
	}
}
