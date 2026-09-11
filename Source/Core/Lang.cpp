#include "Lang.h"
#include "Config.h"
#include <SDL.h>
#include <cstring>

namespace Freeking
{
	static bool _russian = false;

	struct LangEntry
	{
		const char* key;
		const char* en;
		const char* ru;
	};

	static const LangEntry _entries[] =
	{
		{ "app_subtitle", "Kingpin: Life of Crime reimplementation", "Реимплементация Kingpin: Life of Crime" },
		{ "menu_new_game", "New game", "Новая игра" },
		{ "menu_continue", "Continue", "Продолжить" },
		{ "menu_maps", "Maps", "Карты" },
		{ "menu_settings", "Settings", "Настройки" },
		{ "menu_help", "Help", "Помощь" },
		{ "menu_quit", "Quit", "Выход" },
		{ "maps_title", "Select map", "Выбор карты" },
		{ "maps_empty", "No maps found.", "Карты не найдены." },
		{ "maps_play", "Play", "Играть" },
		{ "maps_back", "Back", "Назад" },
		{ "maps_last", "Last played", "Последняя" },
		{ "set_title", "Settings", "Настройки" },
		{ "set_sensitivity", "Look sensitivity", "Чувствительность обзора" },
		{ "set_invert_y", "Invert Y", "Инверсия Y" },
		{ "set_render_scale", "Render resolution", "Разрешение рендера" },
		{ "set_render_scale_auto", "Auto", "Авто" },
		{ "set_fps_limit", "FPS limit", "Лимит FPS" },
		{ "set_fps_unlimited", "Unlimited", "Без лимита" },
		{ "set_sound", "Sound volume", "Громкость звука" },
		{ "set_touch", "Touch controls", "Сенсорное управление" },
		{ "set_show_fps", "Show FPS", "Показывать FPS" },
		{ "set_msaa", "Antialiasing (MSAA)", "Сглаживание (MSAA)" },
		{ "set_msaa_restart", "Takes effect after restart.", "Вступит в силу после перезапуска." },
		{ "set_debug", "Debug overlay", "Отладочный оверлей" },
		{ "set_language", "Language", "Язык" },
		{ "set_back", "Back", "Назад" },
		{ "pause_title", "Paused", "Пауза" },
		{ "pause_resume", "Resume", "Продолжить" },
		{ "pause_restart", "Restart map", "Начать карту заново" },
		{ "pause_maps", "Change map", "Сменить карту" },
		{ "pause_settings", "Settings", "Настройки" },
		{ "pause_quit_menu", "Quit to menu", "Выйти в меню" },
		{ "loading", "Loading...", "Загрузка..." },
		{ "error_title", "Error", "Ошибка" },
		{ "error_back", "Back to menu", "В меню" },
		{ "toast_objective", "Find the exit to complete the level!", "Найдите выход, чтобы пройти уровень!" },
		{ "toast_complete", "Level complete!", "Уровень пройден!" },
		{ "toast_noclip_on", "Fly mode: on", "Режим полёта: вкл" },
		{ "toast_noclip_off", "Fly mode: off", "Режим полёта: выкл" },
		{ "toast_use", "Opened", "Открыто" },
		{ "edit_title", "Touch layout", "Раскладка кнопок" },
		{ "edit_touch_enabled", "Touch controls enabled", "Сенсорное управление вкл" },
		{ "edit_hint", "Drag buttons to move, drag the corner to resize.", "Тяните кнопки для перемещения, уголок — для размера." },
		{ "edit_visible", "Visible", "Видимость" },
		{ "edit_size", "Size", "Размер" },
		{ "edit_reset", "Reset layout", "Сбросить раскладку" },
		{ "edit_done", "Done", "Готово" },
		{ "edit_stick", "Joystick", "Джойстик" },
		{ "help_title", "How to play", "Как играть" },
		{ "files_title", "Game files missing", "Файлы игры не найдены" },
		{ "files_back", "Quit", "Выход" },
		{ "help_text",
			"GOAL\nFind the exit on every level to reach the next one. If you get stuck, fly mode (FLY button or V key) will get you out.\n\n"
			"TOUCH\nLeft stick - move. Drag anywhere else - look. FIRE - shoot / open. USE - open doors and press buttons. JUMP - jump. RUN (hold) - sprint. MENU - pause. EDIT - move, resize and hide buttons.\n\n"
			"KEYBOARD + MOUSE\nWASD - move. Mouse - look (hold right button to capture). Left button - shoot / open. E - use. Space - jump. Shift - sprint. V - fly mode. F1 - debug overlay. Esc - pause.",
			"ЦЕЛЬ\nНаходите выход на каждом уровне, чтобы перейти дальше. Если застряли — режим полёта (кнопка FLY или клавиша V) выручит.\n\n"
			"СЕНСОР\nЛевый стик — движение. Ведите пальцем в любом другом месте — обзор. FIRE — выстрел / открыть. USE — открыть двери и нажать кнопки. JUMP — прыжок. RUN (держать) — бег. MENU — пауза. EDIT — двигать, менять размер и скрывать кнопки.\n\n"
			"КЛАВИАТУРА + МЫШЬ\nWASD — движение. Мышь — обзор (держите правую кнопку для захвата). Левая кнопка — выстрел / открыть. E — использовать. Пробел — прыжок. Shift — бег. V — полёт. F1 — отладка. Esc — пауза." },
		{ "files_text_android",
			"Kingpin game files were not found.\n\nYou need your own licensed copy of Kingpin: Life of Crime (Steam or GOG). Copy the \"main\" folder from the installed game to your phone, then import it with the IMPORT button in the launcher.\n\nExpected files: main/Pak0.pak and other .pak files.",
			"Файлы игры Kingpin не найдены.\n\nНужна ваша лицензионная копия Kingpin: Life of Crime (Steam или GOG). Скопируйте папку \"main\" из установленной игры на телефон, затем импортируйте её кнопкой ИМПОРТ в лаучере.\n\nОжидаются файлы: main/Pak0.pak и другие .pak файлы." },
		{ "files_text_desktop",
			"Kingpin game files were not found.\n\nInstall Kingpin: Life of Crime (e.g. from Steam or GOG) so it can be detected, or place the game \"main\" folder next to the freeking executable.\n\nExpected files: main/Pak0.pak and other .pak files.",
			"Файлы игры Kingpin не найдены.\n\nУстановите Kingpin: Life of Crime (например, из Steam или GOG), чтобы игра нашлась автоматически, или положите папку \"main\" рядом с freeking.\n\nОжидаются файлы: main/Pak0.pak и другие .pak файлы." },
	};

	void Lang::Init()
	{
		_russian = Config::IsRussian();

		if (!Config::IsRussianExplicit())
		{
#if SDL_VERSION_ATLEAST(2, 0, 14)
			if (SDL_Locale* locales = SDL_GetPreferredLocales())
			{
				if (locales[0].language != nullptr && std::strcmp(locales[0].language, "ru") == 0)
				{
					_russian = true;
					Config::SetRussian(true);
				}

				SDL_free(locales);
			}
#endif
		}
	}

	bool Lang::IsRussian()
	{
		return _russian;
	}

	void Lang::SetRussian(bool russian)
	{
		_russian = russian;
		Config::SetRussian(russian);
	}

	const char* Lang::T(const char* key)
	{
		for (const auto& entry : _entries)
		{
			if (std::strcmp(entry.key, key) == 0)
			{
				return _russian ? entry.ru : entry.en;
			}
		}

		return key;
	}
}
