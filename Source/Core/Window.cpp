#include "Window.h"
#include "GLCompat.h"
#include <iostream>

namespace Freeking
{
#ifdef __ANDROID__
	int Window::_msaaSamples = 0;
#else
	int Window::_msaaSamples = 4;
#endif

	Window::Window(const std::string& title, const int width, const int height)
	{
		SDL_version compiledVersion, linkedVersion;
		SDL_VERSION(&compiledVersion);
		SDL_GetVersion(&linkedVersion);

		std::clog << "Initializing SDL..." << std::endl;
		std::clog << "SDL Version/Compiled " << uint32_t(compiledVersion.major) << "." <<
			uint32_t(compiledVersion.minor) << "." << uint32_t(compiledVersion.patch) << std::endl;
		std::clog << "SDL Version/Linked " << uint32_t(linkedVersion.major) << "." <<
			uint32_t(linkedVersion.minor) << "." << uint32_t(linkedVersion.patch) << std::endl;

		if (SDL_WasInit(0) == 0)
		{
			SDL_SetMainReady();

			if (SDL_Init(0) != 0)
			{
				throw std::runtime_error("Could not initialize SDL: " + std::string(SDL_GetError()));
			}

			if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
			{
				throw std::runtime_error("Could not initialize SDL Video Subsystem: " + std::string(SDL_GetError()));
			}

			if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0)
			{
				throw std::runtime_error("Could not initialize SDL Timer Subsystem: " + std::string(SDL_GetError()));
			}

			if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
			{
				std::clog << "Warning: could not initialize SDL Audio Subsystem: " << SDL_GetError() << std::endl;
			}
		}

#ifdef __ANDROID__
		SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

		if (_msaaSamples > 0)
		{
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, _msaaSamples);
		}
		else
		{
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}

		const uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI |
			SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		if (_msaaSamples > 0)
		{
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, _msaaSamples);
		}
		else
		{
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}

		const uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
#endif

		if (auto window = std::unique_ptr<SDL_Window, SDLDestroyer>(SDL_CreateWindow(
			title.c_str(),
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			width,
			height,
			flags));
			window != nullptr)
		{
			_window = std::move(window);
		}
		else
		{
			throw std::runtime_error("Failed creating window: " + std::string(SDL_GetError()));
		}

#ifndef __ANDROID__
		if (auto renderer = SDL_CreateRenderer(_window.get(), -1, SDL_RENDERER_ACCELERATED);
			renderer != nullptr)
		{
			_renderer = std::move(std::unique_ptr<SDL_Renderer, SDLDestroyer>(renderer));
		}
		else
		{
			throw std::runtime_error("SDL2 Renderer couldn't be created: " + std::string(SDL_GetError()));
		}
#endif

		if (auto context = SDL_GL_CreateContext(_window.get());
			context != nullptr)
		{
			_glContext = std::move(std::unique_ptr<SDL_GLContext, SDLDestroyer>(&context));
		}
		else
		{
			throw std::runtime_error("Failed to initialize the OpenGL context: " + std::string(SDL_GetError()));
		}

#ifdef __ANDROID__
		// No loader needed on Android: GLES entry points are linked directly.
		std::cout << "OpenGL ES: " << glGetString(GL_VERSION) << "\n"
			"Vendor: " << glGetString(GL_VENDOR) << "\n" <<
			"Renderer: " << glGetString(GL_RENDERER) << "\n" <<
			std::endl;

		GLint majorVersion = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
		if (majorVersion < 3)
		{
			throw std::runtime_error("OpenGL ES 3.0 or higher is required.");
		}
#else
		if (auto version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress))
		{
			std::cout << "OpenGL: " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version) << "\n"
				"Vendor: " << glGetString(GL_VENDOR) << "\n" <<
				"Renderer: " << glGetString(GL_RENDERER) << "\n" <<
				"Version: " << glGetString(GL_VERSION) << "\n" <<
				std::endl;
		}
		else
		{
			throw std::runtime_error("Failed to initialize the OpenGL context.");
		}

		if (!GLAD_GL_VERSION_4_6)
		{
			throw std::runtime_error("Your OpenGL version is too low, expected 4.6 or higher.");
		}
#endif

#ifdef __ANDROID__
		// VSync on mobile saves battery and matches the display refresh.
		if (SDL_GL_SetSwapInterval(1) != 0)
		{
			SDL_GL_SetSwapInterval(0);
		}
#else
		SDL_GL_SetSwapInterval(0);
#endif
		SDL_ShowWindow(_window.get());
	}

	void Window::SetTitle(const std::string& title)
	{
		SDL_SetWindowTitle(_window.get(), title.c_str());
	}

	const std::string Window::GetTitle() const
	{
		return SDL_GetWindowTitle(_window.get());
	}

	void Window::GetDrawableSize(int& width, int& height) const
	{
		SDL_GL_GetDrawableSize(_window.get(), &width, &height);
	}

	void Window::Swap()
	{
		SDL_GL_SwapWindow(_window.get());
	}
}
