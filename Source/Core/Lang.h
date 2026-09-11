#pragma once

namespace Freeking
{
	// Minimal EN/RU localization for menus and messages.
	// Russian glyphs come from a system TTF loaded into ImGui (see Game);
	// the bundled MSDF fonts are ASCII-only, so SpriteBatch text stays
	// English and all localized strings are rendered with ImGui.
	class Lang
	{
	public:

		Lang() = delete;
		~Lang() = delete;

		static void Init();
		static bool IsRussian();
		static void SetRussian(bool russian);

		// Returns the localized string for "key", or "key" itself if missing.
		static const char* T(const char* key);
	};
}
