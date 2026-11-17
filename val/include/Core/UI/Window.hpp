#pragma once

#include "Core/UI/D3D11Device.hpp"

#include <shellapi.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

namespace Core::UI
{
	class Window
	{
	public:
		explicit Window( const std::wstring&, ImVec2, ServiceHub& );
		~Window( );

		[[nodiscard]] ServiceHub* get_service_hub( ) const;
		[[nodiscard]] bool is_running( ) const;

		void run( );
		void close( );

		// closing asks: minimize to tray / quit / cancel
		void request_close( ) { close_prompt_ = true; }

	private:
		void initialize_imgui( ) const;
		void shutdown_imgui( ) const;

		static LRESULT CALLBACK wnd_proc( HWND, UINT, WPARAM, LPARAM );

		HWND hwnd_ {};
		WNDCLASSEXW wc_ {};
		ImVec2 size_;
		std::pair<std::uint32_t, std::uint32_t> offset_;

		std::shared_ptr<D3D11Device> d3d_device_;

		// debug tab: RGBA preview uploaded to a dynamic texture
		Microsoft::WRL::ComPtr<ID3D11Texture2D> preview_tex_;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> preview_srv_;
		int preview_w_ = 0;
		int preview_h_ = 0;

		void upload_preview( const cv::Mat& rgba );
		void render_debug_tab( );

		ServiceHub* service_hub_ = nullptr;

		// tray
		void hide_to_tray( );
		void restore_from_tray( );
		void show_tray_menu( );
		NOTIFYICONDATAW nid_ {};
		bool in_tray_ = false;
		std::atomic<bool> close_prompt_ = false;

		std::atomic<bool> running_ = true;
	};
}
