#include <SDL.h>
#include "Game.h"
#include "CrashHandler.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main(int argc, char** argv)
{
	Freeking::InstallCrashHandlers();

	try
	{
		Freeking::Game game(argc, argv);
		game.Run();
	}
	catch (const std::exception& e)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Freeking", e.what(), nullptr);

		return EXIT_FAILURE;
	}
	catch (...)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Freeking", "Unknown fatal error.", nullptr);

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
