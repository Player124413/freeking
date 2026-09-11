#pragma once

#include <SDL.h>
#include <memory>
#include <string>

namespace Freeking
{
	struct SDLDestroyer
	{
		void operator()(SDL_Window* window) const
		{
			SDL_DestroyWindow(window);
		}

		void operator()(SDL_Renderer* renderer) const
		{
			SDL_DestroyRenderer(renderer);
		}

		void operator()(SDL_GLContext* glcontext) const
		{
			SDL_GL_DeleteContext(*glcontext);
		}
	};

	class Window
	{
	public:

		// Must be called before constructing the Window. On desktop the
		// default is 4x MSAA, on Android MSAA is off by default (big mobile
		// GPUs cost) and can be enabled from the settings (takes effect on
		// next launch).
		static void SetMSAASamples(int samples) { _msaaSamples = samples; }
		static int GetMSAASamples() { return _msaaSamples; }

		Window(const std::string& title, int width, int height);

		operator SDL_Window* ()
		{
			return _window.get();
		}

		operator SDL_GLContext* ()
		{
			return _glContext.get();
		}

		const std::string GetTitle() const;
		void SetTitle(const std::string&);

		void GetDrawableSize(int& width, int& height) const;

		void Swap();

	private:

		static int _msaaSamples;

		std::unique_ptr<SDL_Window, SDLDestroyer> _window;
		std::unique_ptr<SDL_Renderer, SDLDestroyer> _renderer;
		std::unique_ptr<SDL_GLContext, SDLDestroyer> _glContext;
	};
}
