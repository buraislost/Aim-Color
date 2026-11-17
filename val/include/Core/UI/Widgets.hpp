#pragma once

// ui/Widgets.hpp — small reusable controls: label column + control, 2-up rows, switch, segmented tabs
// Depends: ui/Theme.hpp

#include "Core/UI/Theme.hpp"

namespace Core::UI::widget
{
	namespace th = theme;

	// section caption, 16px above and 8px below
	inline void section( const char* title )
	{
		ImGui::Dummy( { 0.f, 0.f } );
		ImGui::TextColored( th::kMuted, "%s", title );
	}

	// left label column; the control that follows takes the rest of the row
	inline void label( const char* text )
	{
		ImGui::AlignTextToFramePadding( );
		ImGui::TextColored( th::kMuted, "%s", text );
		ImGui::SameLine( th::kLabelWidth );
	}

	inline float half_width( )
	{
		return ( ImGui::GetContentRegionAvail( ).x - th::kSpace2 ) * 0.5f;
	}

	inline void slider_int( const char* name, std::int32_t* v, const int lo, const int hi, const char* fmt = "%d" )
	{
		label( name );
		ImGui::PushID( name );
		ImGui::SetNextItemWidth( -1.f );
		ImGui::SliderInt( "##v", v, lo, hi, fmt );
		ImGui::PopID( );
	}

	struct IntRange
	{
		int lo;
		int hi;
	};

	// two sliders on one labelled row; the formats carry the axis ("X %dpx" / "Y %dpx")
	inline void pair_int( const char* name, std::int32_t* a, std::int32_t* b, const IntRange r, const char* fa, const char* fb )
	{
		label( name );
		ImGui::PushID( name );
		ImGui::SetNextItemWidth( half_width( ) );
		ImGui::SliderInt( "##a", a, r.lo, r.hi, fa );
		ImGui::SameLine( 0.f, th::kSpace2 );
		ImGui::SetNextItemWidth( half_width( ) );
		ImGui::SliderInt( "##b", b, r.lo, r.hi, fb );
		ImGui::PopID( );
	}

	struct FloatRange
	{
		float lo;
		float hi;
	};

	inline void pair_float( const char* name, float* a, float* b, const FloatRange r, const char* fa, const char* fb )
	{
		label( name );
		ImGui::PushID( name );
		ImGui::SetNextItemWidth( half_width( ) );
		ImGui::SliderFloat( "##a", a, r.lo, r.hi, fa );
		ImGui::SameLine( 0.f, th::kSpace2 );
		ImGui::SetNextItemWidth( half_width( ) );
		ImGui::SliderFloat( "##b", b, r.lo, r.hi, fb );
		ImGui::PopID( );
	}

	inline void combo( const char* name, std::int32_t* v, const char* const* items, const int count )
	{
		label( name );
		ImGui::PushID( name );
		ImGui::SetNextItemWidth( -1.f );
		ImGui::Combo( "##v", v, items, count );
		ImGui::PopID( );
	}

	// switch cell: label on the left, pill on the right. width 0 = fill the row
	inline void toggle( const char* name, bool* v, float width = 0.f )
	{
		if ( width <= 0.f ) {
			width = ImGui::GetContentRegionAvail( ).x;
		}

		ImGui::PushID( name );
		const ImVec2 pos  = ImGui::GetCursorScreenPos( );
		const ImVec2 size = { width, th::kRowHeight };

		if ( ImGui::InvisibleButton( "##t", size ) ) {
			*v = !*v;
		}

		auto* dl          = ImGui::GetWindowDrawList( );
		const bool hot    = ImGui::IsItemHovered( );
		const float text_y = pos.y + ( size.y - ImGui::GetTextLineHeight( ) ) * 0.5f;

		dl->AddRectFilled( pos, pos + size, ImGui::GetColorU32( hot ? th::kSurfaceHi : th::kBackground ), th::kRadius );
		dl->AddText( { pos.x + th::kSpace2, text_y }, ImGui::GetColorU32( th::kText ), name );

		const ImVec2 track = { pos.x + size.x - 40.f, pos.y + ( size.y - 16.f ) * 0.5f };
		dl->AddRectFilled( track, track + ImVec2( 32.f, 16.f ), ImGui::GetColorU32( *v ? th::kAccent : th::kSurfaceHi ), 8.f );
		dl->AddCircleFilled( { track.x + ( *v ? 24.f : 8.f ), track.y + 8.f }, 6.f,
		                     ImGui::GetColorU32( *v ? th::kText : th::kMuted ) );
		ImGui::PopID( );
	}

	// segmented tab strip, `sel` is the active index
	inline void tabs( const char* const* names, const int count, int& sel )
	{
		const float w = ( ImGui::GetContentRegionAvail( ).x - th::kSpace1 * ( count - 1 ) ) / static_cast<float>( count );

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

	inline bool accent_button( const char* text, const ImVec2 size )
	{
		ImGui::PushStyleColor( ImGuiCol_Button, th::kAccent );
		ImGui::PushStyleColor( ImGuiCol_ButtonHovered, th::kAccentHi );
		const bool pressed = ImGui::Button( text, size );
		ImGui::PopStyleColor( 2 );
		return pressed;
	}
}
