// ui/Theme.cpp — pushes the design tokens into the global ImGui style
// Depends: ui/Theme.hpp

// ===== Includes =====
#include "Stdafx.hpp"
#include "Core/UI/Theme.hpp"

namespace Core::UI::theme
{
	// ===== Internal helpers =====
	namespace
	{
		void apply_metrics( ImGuiStyle& st )
		{
			st.WindowRounding    = kSpace3;
			st.ChildRounding     = kRadius;
			st.FrameRounding     = kRadius;
			st.PopupRounding     = kRadius;
			st.GrabRounding      = kRadius;
			st.ScrollbarRounding = kRadius;
			st.ScrollbarSize     = kSpace2;
			st.GrabMinSize       = kSpace3;

			st.WindowPadding = { kSpace3, kSpace3 };
			st.FramePadding  = { kSpace2, kSpace2 };
			st.ItemSpacing   = { kSpace2, kSpace2 };

			st.WindowBorderSize = 1.f;
			st.ChildBorderSize  = 1.f;
			st.FrameBorderSize  = 0.f;
		}

		void apply_surfaces( ImVec4* c )
		{
			c[ ImGuiCol_Text ]          = kText;
			c[ ImGuiCol_TextDisabled ]  = kMuted;
			c[ ImGuiCol_WindowBg ]      = kBackground;
			c[ ImGuiCol_ChildBg ]       = kSurface;
			c[ ImGuiCol_PopupBg ]       = kSurface;
			c[ ImGuiCol_Border ]        = kBorder;
			c[ ImGuiCol_Separator ]     = kBorder;
			c[ ImGuiCol_ScrollbarBg ]   = kSurface;
			c[ ImGuiCol_ScrollbarGrab ] = kSurfaceHi;
		}

		void apply_controls( ImVec4* c )
		{
			c[ ImGuiCol_FrameBg ]          = kBackground;
			c[ ImGuiCol_FrameBgHovered ]   = kSurfaceHi;
			c[ ImGuiCol_FrameBgActive ]    = kSurfaceHi;
			c[ ImGuiCol_CheckMark ]        = kAccentHi;
			c[ ImGuiCol_SliderGrab ]       = kAccent;
			c[ ImGuiCol_SliderGrabActive ] = kAccentHi;
			c[ ImGuiCol_Button ]           = kSurfaceHi;
			c[ ImGuiCol_ButtonHovered ]    = kAccent;
			c[ ImGuiCol_ButtonActive ]     = kAccentHi;
			c[ ImGuiCol_Header ]           = rgb( 124, 92, 252, 0.28f );
			c[ ImGuiCol_HeaderHovered ]    = rgb( 124, 92, 252, 0.45f );
			c[ ImGuiCol_HeaderActive ]     = kAccent;
		}
	}

	// ===== Public API =====
	void apply( )
	{
		ImGuiStyle& st = ImGui::GetStyle( );

		apply_metrics( st );
		apply_surfaces( st.Colors );
		apply_controls( st.Colors );
	}
}
