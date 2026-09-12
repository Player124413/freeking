#include "Game.h"
#include "CrashHandler.h"
#include "Shader.h"
#include "Window.h"
#include "Input.h"
#include "TouchControls.h"
#include "Config.h"
#include "Lang.h"
#include "Paths.h"
#include "Matrix4x4.h"
#include "Matrix3x3.h"
#include "LineRenderer.h"
#include "SpriteBatch.h"
#include "Font.h"
#include "Texture2D.h"
#include "TextureSampler.h"
#include "EntityLump.h"
#include "FpsTimer.h"
#include "FreeCamera.h"
#include "DynamicModel.h"
#include "Util.h"
#include "Map.h"
#include "BspFlags.h"
#include "Nav/NavFile.h"
#include "PakFileSystem.h"
#include "PhysicalFileSystem.h"
#include "Renderer.h"
#include "TimeUtil.h"
#include "Audio/AudioClip.h"
#include "Audio/AudioDevice.h"
#include "Skybox.h"
#include "BillboardBatch.h"
#include "GLCompat.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_impl_opengl3.h"
#include "ThirdParty/imgui/imgui_impl_sdl.h"

namespace Freeking
{
	Game* Game::_instance = nullptr;

	Game::Game(int argc, char** argv)
	{
		(void)argc;
		(void)argv;

		_instance = this;

		Config::Load();
		Lang::Init();

		Window::SetMSAASamples(Config::MsaaSamples());

		FileSystem::AddFileSystem(PhysicalFileSystem::Create(Paths::AssetsDir()));

		auto kingpinDir = Paths::KingpinDir();
		if (!kingpinDir.empty())
		{
			FileSystem::AddFileSystem(PhysicalFileSystem::Create(kingpinDir / "main"));

			for (int i = 0; i <= 9; ++i)
			{
				std::string pakName = "main/Pak" + std::to_string(i) + ".pak";
				auto pakPath = kingpinDir / pakName;
				std::error_code ec;
				if (!std::filesystem::exists(pakPath, ec))
				{
					// Some installs use lower-case names.
					pakName = "main/pak" + std::to_string(i) + ".pak";
					pakPath = kingpinDir / pakName;
				}

				FileSystem::AddFileSystem(PakFileSystem::Create(pakPath));
			}
		}

		int windowWidth = 1280;
		int windowHeight = 720;
#ifdef __ANDROID__
		SDL_DisplayMode displayMode = {};
		if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0 && displayMode.w > 0 && displayMode.h > 0)
		{
			windowWidth = displayMode.w;
			windowHeight = displayMode.h;
		}
#endif

		static const std::string windowTitle = "Freeking";
		_window = std::make_unique<Window>(windowTitle, windowWidth, windowHeight);

		_window->GetDrawableSize(_screenWidth, _screenHeight);
		if (_screenWidth <= 0) _screenWidth = windowWidth;
		if (_screenHeight <= 0) _screenHeight = windowHeight;

		Config::RefreshAutoRenderScale(_screenWidth, _screenHeight);

		_audio = std::make_unique<AudioDevice>();
		_audio->SetMasterVolume(Config::SoundVolume());

		Shader::Initialize();

		ImGui::CreateContext();
		ImGui_ImplSDL2_InitForOpenGL(static_cast<SDL_Window*>(*_window), static_cast<SDL_GLContext*>(*_window));
#ifdef __ANDROID__
		ImGui_ImplOpenGL3_Init("#version 300 es");
#else
		ImGui_ImplOpenGL3_Init("#version 130");
#endif

		// System TTF with Cyrillic glyphs for the Russian UI.
		{
			ImGuiIO& io = ImGui::GetIO();
			const char* candidates[] =
			{
#ifdef __ANDROID__
				"/system/fonts/Roboto-Regular.ttf",
#elif defined(_WIN32)
				"C:\\Windows\\Fonts\\arial.ttf",
				"C:\\Windows\\Fonts\\calibri.ttf",
#elif defined(__APPLE__)
				"/System/Library/Fonts/Supplemental/Arial.ttf",
				"/System/Library/Fonts/Helvetica.ttc",
#else
				"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
				"/usr/share/fonts/TTF/DejaVuSans.ttf",
				"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
#endif
				nullptr
			};

			_imguiCyrillic = false;
#ifdef __ANDROID__
			float fontPx = Math::Clamp(_screenHeight * 0.030f, 18.0f, 30.0f);
#else
			float fontPx = 17.0f;
#endif
			for (int i = 0; candidates[i] != nullptr; ++i)
			{
				std::error_code ec;
				if (!std::filesystem::exists(candidates[i], ec))
				{
					continue;
				}

				if (io.Fonts->AddFontFromFileTTF(candidates[i], fontPx, nullptr, io.Fonts->GetGlyphRangesCyrillic()) != nullptr)
				{
					_imguiCyrillic = true;
					break;
				}
			}

			if (!_imguiCyrillic)
			{
				io.Fonts->AddFontDefault();
			}
#ifdef __ANDROID__
			else
			{
				ImGui::GetStyle().ScaleAllSizes(Math::Clamp(fontPx / 16.0f, 1.0f, 2.0f));
			}
#endif
		}

#ifdef __ANDROID__
		Renderer::DebugDraw = false;
#endif

#ifdef __ANDROID__
		_lineRenderer = std::make_shared<LineRenderer>(65536);
#else
		_lineRenderer = std::make_shared<LineRenderer>(2000000);
#endif
		_spriteBatch = std::make_shared<SpriteBatch>(10000);
		SpriteBatch::Debug = _spriteBatch;
		LineRenderer::Debug = _lineRenderer;

		_camera = std::make_unique<FreeCamera>();
		_camera->MoveTo(Vector3f(0, 100, 0));

		_touch = std::make_unique<TouchControls>();
		_touch->SetScreenSize(_screenWidth, _screenHeight);
		_touch->LoadLayout();

		_billboards = std::make_shared<BillboardBatch>();
		LightFlares::Billboards = _billboards.get();

		_crosshair = Texture2D::Library.Get("Textures/crosshair.tga");
		if (_crosshair == nullptr)
		{
			_crosshair = Texture2D::GetFallback();
		}

		_font = Font::Library.Get("Fonts/Roboto-Bold.json");

		_viewmodel = DynamicModel::Library.Get("models/weapons/shotgun/shotgun.mdx");
		_viewmodelHand = DynamicModel::Library.Get("models/weapons/shotgun/hand.mdx");
		_viewmodelAnimator = std::make_unique<FrameAnimator>();
		if (_viewmodel != nullptr)
		{
			for (const auto& frameAnimation : _viewmodel->GetFrameAnimations())
			{
				_viewmodelAnimator->AddAnimation(frameAnimation.name, frameAnimation.firstFrame, frameAnimation.numFrames);
			}

			_viewmodelAnimator->SetAnimation(0);
		}

		RescanMaps();

		if (_mapList.empty())
		{
			_state = State::NoFiles;
		}
	}

	Game::~Game()
	{
		_running = false;

		UnloadMap();

		_viewmodel.reset();
		_viewmodelHand.reset();
		_viewmodelAnimator.reset();
		_crosshair.reset();
		_font.reset();
		_billboards.reset();
		_touch.reset();
		_camera.reset();
		_lineRenderer.reset();
		_spriteBatch.reset();

		Config::Save();

		DestroyScaler();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		_audio.reset();
		_window.reset();
		SDL_Quit();

		if (_instance == this)
		{
			_instance = nullptr;
		}
	}

	void Game::QueueMapChange(const std::string& mapName)
	{
		if (mapName.empty())
		{
			return;
		}

		// Already queued or already there.
		if (_pendingMap == mapName)
		{
			return;
		}

		if (_map != nullptr && _map->GetName() == mapName)
		{
			return;
		}

		_pendingMap = mapName;
	}

	void Game::RescanMaps()
	{
		_mapList.clear();

		for (const auto& file : FileSystem::ListFiles("maps", ".bsp"))
		{
			std::string name = file;
			if (name.size() > 5 && (name.compare(0, 5, "maps/") == 0 || name.compare(0, 5, "maps\\") == 0))
			{
				name = name.substr(5);
			}

			if (name.size() > 4)
			{
				name = name.substr(0, name.size() - 4);
			}

			if (!name.empty())
			{
				_mapList.push_back(name);
			}
		}

		std::sort(_mapList.begin(), _mapList.end());
		_mapList.erase(std::unique(_mapList.begin(), _mapList.end()), _mapList.end());
	}

	std::string Game::DefaultMap() const
	{
		if (_mapList.empty())
		{
			return {};
		}

		for (const auto& map : _mapList)
		{
			if (FileSystem::ToLower(map) == "sr1")
			{
				return map;
			}
		}

		return _mapList.front();
	}

	void Game::UnloadMap()
	{
		ClearCrashContext();

		// Clear first: if a Map constructor threw midway, Current may point
		// at a dead object (the destructor never ran for it).
		Map::Current = nullptr;
		_map.reset();
		_skybox.reset();
		_navNodes.clear();
		_exits.clear();
		_tracers.clear();
		_weaponSound.clear();
		_toastUntil = 0.0;

		// Drop per-map assets; objects still referenced (viewmodels, UI)
		// stay alive through their shared owners.
		Texture2D::Library.Clear();
		DynamicModel::Library.Clear();
		AudioClip::Library.Clear();
	}

	void Game::LoadMapSync(const std::string& mapName)
	{
		UnloadMap();

		// Fail loud with the GPU's own error instead of a black world:
		// without these shaders nothing 3D can render.
		{
			const std::shared_ptr<Shader> critical[] =
			{
				Shader::Library.Lightmapped,
				Shader::Library.DynamicModel
			};

			std::string failures;
			for (const auto& shader : critical)
			{
				if (shader == nullptr || !shader->IsValid())
				{
					failures += (shader == nullptr ? "(missing shader)\n" : shader->GetName() + ":\n" + shader->GetErrorLog() + "\n");
				}
			}

			if (!failures.empty())
			{
				throw std::runtime_error("3D shaders failed on this GPU, cannot render the map:\n" + failures);
			}
		}

		std::cout << "Loading map: " << mapName << std::endl;
		SetCrashContext("loading map " + mapName);

		auto map = std::make_shared<Map>(mapName);

		Vector3f spawnPos(0, 100, 0);
		float spawnYaw = 0.0f;
		bool hasSpawn = false;
		std::string skyName;
		bool hasSky = false;

		for (const auto& entDef : map->GetEntityProperties())
		{
			std::string classname = entDef.GetClassnameProperty();
			if (classname.empty())
			{
				continue;
			}

			if (classname == "worldspawn")
			{
				if (entDef.TryGetString("sky", skyName) && !skyName.empty())
				{
					hasSky = true;
				}
			}
			else if (classname == "info_player_start" && !hasSpawn)
			{
				if (const auto& origin = entDef.GetOriginProperty())
				{
					spawnPos = Util::ConvertVector(origin);
					hasSpawn = true;
				}

				if (const auto& angle = entDef.GetAngleProperty())
				{
					spawnYaw = angle;
				}
			}
			else if (classname == "trigger_changelevel" || classname == "target_changelevel")
			{
				std::string targetMap;
				if (!entDef.TryGetString("map", targetMap) || targetMap.empty())
				{
					continue;
				}

				ExitZone exit;
				exit.map = targetMap;

				std::string modelKey;
				int modelIndex = -1;
				if (entDef.TryGetString("model", modelKey) && modelKey.size() > 1 && modelKey[0] == '*')
				{
					Util::TryParseInt(modelKey.substr(1), modelIndex);
				}

				if (modelIndex >= 0)
				{
					const auto& brushModel = map->GetBrushModel(static_cast<uint32_t>(modelIndex));
					if (brushModel == nullptr)
					{
						continue;
					}

					exit.mins = brushModel->BoundsMin;
					exit.maxs = brushModel->BoundsMax;
				}
				else if (const auto& origin = entDef.GetOriginProperty())
				{
					Vector3f center = Util::ConvertVector(origin);
					exit.mins = center + Vector3f(-48, -48, -48);
					exit.maxs = center + Vector3f(48, 48, 48);
				}
				else
				{
					continue;
				}

				_exits.push_back(exit);
			}
		}

		if (hasSky)
		{
			try
			{
				_skybox = std::make_unique<Skybox>(skyName);
			}
			catch (const std::exception& e)
			{
				std::cout << "Skybox failed: " << e.what() << std::endl;
				_skybox.reset();
			}
		}

		auto navData = FileSystem::GetFileData("navdata/" + mapName + ".nav");
		if (!navData.empty())
		{
			_navNodes = NavFile::ReadNodes(navData.data(), navData.size());
		}

		// Pick a weapon sound for the viewmodel firearm.
		{
			auto sounds = FileSystem::ListFiles("sound", ".wav");
			std::string fallback;
			for (const auto& sound : sounds)
			{
				std::string lower = FileSystem::ToLower(sound);
				if (lower.find("weapon") == std::string::npos)
				{
					continue;
				}

				if (fallback.empty())
				{
					fallback = sound;
				}

				if (lower.find("shotgun") != std::string::npos &&
					(lower.find("fire") != std::string::npos || lower.find("shot") != std::string::npos))
				{
					_weaponSound = sound;
					break;
				}

				if (_weaponSound.empty() && lower.find("pistol") != std::string::npos &&
					(lower.find("fire") != std::string::npos || lower.find("shot") != std::string::npos))
				{
					_weaponSound = sound;
				}
			}

			if (_weaponSound.empty())
			{
				_weaponSound = fallback;
			}
		}

		_map = std::move(map);
		SetCrashContext("playing map " + mapName);

		if (hasSpawn)
		{
			_camera->Teleport(spawnPos, spawnYaw);
		}
		else
		{
			_camera->Teleport(Vector3f(0, 100, 0), 0.0f);
		}

		_camera->SetNoclip(false);
		_wasNoclip = false;
		_fireCooldown = 0.0;
		_exitCooldown = 1.5;
		_touch->ReleaseAll();

		Config::SetLastMap(mapName);
		Config::Save();

		if (_lockMouseOnLoad)
		{
			_lockMouseOnLoad = false;
			LockMouse(true);
		}

		_state = State::Playing;

		ShowToast(Lang::T("toast_objective"), 4.0);
	}

	void Game::OnResize(int width, int height)
	{
		(void)width;
		(void)height;

		_window->GetDrawableSize(_screenWidth, _screenHeight);
		if (_screenWidth <= 0) _screenWidth = 1;
		if (_screenHeight <= 0) _screenHeight = 1;

		_touch->SetScreenSize(_screenWidth, _screenHeight);
		DestroyScaler();
	}

	void Game::LockMouse(bool lockMouse)
	{
		if (_mouseLocked == lockMouse)
		{
			return;
		}

		_mouseLocked = lockMouse;

		SDL_SetRelativeMouseMode(lockMouse ? SDL_TRUE : SDL_FALSE);

		if (!lockMouse)
		{
			SDL_ShowCursor(SDL_ENABLE);
		}

		int w, h;
		SDL_GetWindowSize(static_cast<SDL_Window*>(*_window), &w, &h);
		SDL_WarpMouseInWindow(static_cast<SDL_Window*>(*_window), w / 2, h / 2);

		Input::ResetMouseDelta();
	}

	void Game::ShowToast(const std::string& text, double seconds)
	{
		_toastText = text;
		_toastUntil = Time::Now() + seconds;
	}

	void Game::HandleEvent(const SDL_Event& e)
	{
		if (e.type == SDL_QUIT)
		{
			_running = false;

			return;
		}

		if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
		{
			OnResize(e.window.data1, e.window.data2);
		}
		else if (e.type == SDL_APP_WILLENTERBACKGROUND)
		{
			if (_state == State::Playing)
			{
				_state = State::Paused;
				_pausePage = PausePage::Main;
				LockMouse(false);
				_touch->ReleaseAll();
			}

			_audio->SetPaused(true);
			Config::Save();
			_touch->SaveLayout();
		}
		else if (e.type == SDL_APP_DIDENTERFOREGROUND)
		{
			_audio->SetPaused(false);
			Input::ResetMouseDelta();
		}

		ImGui_ImplSDL2_ProcessEvent(&e);

		Input::HandleEvent(e);

		// Touch gameplay input is only live while playing; menus are driven
		// through ImGui so stray menu taps must not inject buttons or look.
		if (_state == State::Playing)
		{
			_touch->HandleEvent(e);
		}
	}

	void Game::ConsumePendingMap()
	{
		if (_pendingMap.empty())
		{
			return;
		}

		std::string mapName = _pendingMap;
		_pendingMap.clear();

		_state = State::Loading;
		_lockMouseOnLoad = _mouseLocked;
		LockMouse(false);
		_touch->ReleaseAll();

		// Present one loading frame before the blocking load. This runs
		// outside the main loop's ImGui frame, so open one explicitly.
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(static_cast<SDL_Window*>(*_window));
		ImGui::NewFrame();
		RenderFrame();

		try
		{
			LoadMapSync(mapName);
		}
		catch (const std::exception& e)
		{
			std::cout << "Map load failed: " << e.what() << std::endl;
			_errorMessage = e.what();
			_state = State::Error;
			UnloadMap();
		}
	}

	void Game::TryFire()
	{
		if (_map == nullptr || _fireCooldown > 0.0)
		{
			return;
		}

		_fireCooldown = 0.45;
		_crosshairPulse = 1.0f;

		if (_audio != nullptr && !_weaponSound.empty())
		{
			_audio->Play(AudioClip::Library.Get(_weaponSound), Vector3f(0, 0, 0), false, true);
		}

		Matrix4x4 projectionMatrix = Matrix4x4::Perspective(80, (float)_renderWidth / (float)_renderHeight, 0.1f, 5000.0f);
		Vector3f eye = _camera->GetPosition();
		Vector3f direction = _camera->NormalisedScreenPointToDirection(projectionMatrix, Vector2f(0, 0));

		TraceResult tr = _map->LineTrace(eye, eye + direction * 4096.0f, BspContentFlags::MaskSolid);
		Vector3f hitPoint = tr.hit ? tr.endPosition : (eye + direction * 4096.0f);

		if (tr.hit && tr.entity != nullptr)
		{
			tr.entity->Trigger();
			tr.entity->TakeDamage();
		}

		Vector3f right = _camera->GetRotation().Right();
		Vector3f up = _camera->GetRotation().Up();
		Vector3f muzzle = eye + direction * 12.0f + right * 5.0f - up * 5.0f;

		_tracers.push_back({ muzzle, hitPoint, LinearColor(1.0f, 0.9f, 0.4f, 0.9f), 0.07 });

		if (tr.hit)
		{
			for (int i = 0; i < 5; ++i)
			{
				Vector3f sparkDir(
					Util::RandomFloat(-1.0f, 1.0f),
					Util::RandomFloat(-1.0f, 1.0f),
					Util::RandomFloat(-1.0f, 1.0f));
				if (sparkDir.SquaredLength() < 0.001f)
				{
					sparkDir = Vector3f(0, 1, 0);
				}

				sparkDir = sparkDir.Normalise();
				_tracers.push_back({ hitPoint, hitPoint + sparkDir * 7.0f, LinearColor(1.0f, 0.6f, 0.15f, 1.0f), 0.18 });
			}
		}
	}

	void Game::TryUse()
	{
		if (_map == nullptr)
		{
			return;
		}

		Matrix4x4 projectionMatrix = Matrix4x4::Perspective(80, (float)_renderWidth / (float)_renderHeight, 0.1f, 5000.0f);
		Vector3f eye = _camera->GetPosition();
		Vector3f direction = _camera->NormalisedScreenPointToDirection(projectionMatrix, Vector2f(0, 0));

		TraceResult tr = _map->LineTrace(eye, eye + direction * 160.0f, BspContentFlags::MaskSolid);
		if (tr.hit && tr.entity != nullptr)
		{
			tr.entity->Trigger();
			ShowToast(Lang::T("toast_use"), 1.2);
		}
	}

	static bool BoxesOverlap(const Vector3f& minsA, const Vector3f& maxsA, const Vector3f& minsB, const Vector3f& maxsB)
	{
		return minsA.x <= maxsB.x && maxsA.x >= minsB.x &&
			minsA.y <= maxsB.y && maxsA.y >= minsB.y &&
			minsA.z <= maxsB.z && maxsA.z >= minsB.z;
	}

	void Game::CheckTouchTriggers()
	{
		if (_map == nullptr)
		{
			return;
		}

		// Camera origin sits 62 units above the feet.
		Vector3f feet = _camera->GetPosition() + Vector3f(0, -62, 0);
		Vector3f playerMins = feet + Vector3f(-16, 0, -16);
		Vector3f playerMaxs = feet + Vector3f(16, 72, 16);

		for (const auto& entity : _map->GetWorldEntities())
		{
			if (entity == nullptr || entity->IsHidden())
			{
				continue;
			}

			const std::string& classname = entity->GetClassname();
			if (classname != "func_door" && classname != "func_door_rotating" && classname != "func_button")
			{
				continue;
			}

			Vector3f mins = entity->GetPosition() + entity->GetLocalMinBounds() + Vector3f(-6, -6, -6);
			Vector3f maxs = entity->GetPosition() + entity->GetLocalMaxBounds() + Vector3f(6, 6, 6);

			if (BoxesOverlap(playerMins, playerMaxs, mins, maxs))
			{
				entity->Trigger();
			}
		}
	}

	void Game::CheckExits()
	{
		if (_map == nullptr || _exits.empty() || _exitCooldown > 0.0)
		{
			return;
		}

		Vector3f feet = _camera->GetPosition() + Vector3f(0, -62, 0);
		Vector3f playerMins = feet + Vector3f(-16, 0, -16);
		Vector3f playerMaxs = feet + Vector3f(16, 72, 16);

		for (const auto& exit : _exits)
		{
			if (BoxesOverlap(playerMins, playerMaxs, exit.mins, exit.maxs))
			{
				ShowToast(Lang::T("toast_complete"), 3.0);
				QueueMapChange(exit.map);

				break;
			}
		}
	}

	void Game::UpdatePlaying(double dt)
	{
		if (_fireCooldown > 0.0) _fireCooldown -= dt;
		if (_exitCooldown > 0.0) _exitCooldown -= dt;
		if (_crosshairPulse > 0.0f) _crosshairPulse = Math::Max(0.0f, _crosshairPulse - static_cast<float>(dt) * 6.0f);

		for (auto it = _tracers.begin(); it != _tracers.end();)
		{
			it->ttl -= dt;
			if (it->ttl <= 0.0)
			{
				it = _tracers.erase(it);
			}
			else
			{
				++it;
			}
		}

		if (Input::JustPressed(Button::KeyESCAPE))
		{
			_state = State::Paused;
			_pausePage = PausePage::Main;
			LockMouse(false);
			_touch->ReleaseAll();
			Config::Save();

			return;
		}

		if (Input::JustPressed(Button::F1))
		{
			Renderer::DebugDraw = !Renderer::DebugDraw;
		}

		// Desktop mouse capture: click the view to capture, Esc releases.
		// The capturing click itself must not fire the weapon.
		bool wasLocked = _mouseLocked;
		if (!_mouseLocked && Input::JustPressed(Button::MouseLeft) && !ImGui::GetIO().WantCaptureMouse)
		{
			LockMouse(true);
		}

		float lookX = 0.0f;
		float lookY = 0.0f;
		float lookScale = 0.25f * Config::Sensitivity();
		if (_mouseLocked)
		{
			lookX += Input::GetMouseDeltaX() * lookScale;
			lookY += Input::GetMouseDeltaY() * lookScale;
		}

		if (_touch->IsEnabled() && !_touch->IsEditMode())
		{
			// Touch look already carries sensitivity; only match the mouse
			// degrees-per-pixel factor. Inversion is applied once below.
			Vector2f touchLook = _touch->ConsumeLookDelta();
			lookX += touchLook.x * 0.25f;
			lookY += touchLook.y * 0.25f;
		}

		if (lookX != 0.0f || lookY != 0.0f)
		{
			if (Config::InvertY())
			{
				_camera->LookDelta(lookX, -lookY);
			}
			else
			{
				_camera->LookDelta(lookX, lookY);
			}
		}

		auto inputForce = Vector3f(0.0f, 0.0f, 0.0f);
		if (Input::IsDown(Button::KeyW) || Input::IsDown(Button::KeyUp)) inputForce += Vector3f(0.0f, 0.0f, 1.0f);
		if (Input::IsDown(Button::KeyS) || Input::IsDown(Button::KeyDown)) inputForce -= Vector3f(0.0f, 0.0f, 1.0f);
		if (Input::IsDown(Button::KeyA) || Input::IsDown(Button::KeyLeft)) inputForce += Vector3f(1.0f, 0.0f, 0.0f);
		if (Input::IsDown(Button::KeyD) || Input::IsDown(Button::KeyRight)) inputForce -= Vector3f(1.0f, 0.0f, 0.0f);

		if (_touch->IsEnabled() && !_touch->IsEditMode())
		{
			Vector2f stick = _touch->GetMoveVector();
			inputForce += Vector3f(-stick.x, 0.0f, stick.y);
		}

		if (inputForce.SquaredLength() > 0.0f)
		{
			inputForce = inputForce.Normalise();
			float speed = Input::IsDown(Button::KeyLSHIFT) ? 1000.0f : (Input::IsDown(Button::KeyLCONTROL) ? 50.0f : 500.0f);
			inputForce *= _camera->IsNoclip() ? speed : 1.0f;
		}

		_camera->Move(inputForce, static_cast<float>(dt));

		if (_camera->IsNoclip() != _wasNoclip)
		{
			_wasNoclip = _camera->IsNoclip();
			ShowToast(_wasNoclip ? Lang::T("toast_noclip_on") : Lang::T("toast_noclip_off"), 2.0);
		}

		bool touchFireArmed = _touch->IsEnabled() && !_touch->IsEditMode();
		if (Input::IsDown(Button::MouseLeft) && ((_mouseLocked && wasLocked) || touchFireArmed) && !ImGui::GetIO().WantCaptureMouse)
		{
			TryFire();
		}

		if (Input::JustPressed(Button::KeyE))
		{
			TryUse();
		}

		_map->Tick(dt);

		CheckTouchTriggers();
		CheckExits();

		_audio->SetListenerTransform(_camera->GetPosition(), _camera->GetRotation());
		_audio->FlushQueue();
	}

	void Game::EnsureScaler()
	{
		float scale = Config::RenderScale();
		bool wantScaler = scale < 0.999f && Window::GetMSAASamples() == 0 &&
			(_state == State::Playing || _state == State::Paused || _state == State::Loading);

		int targetW = Math::Max(320, static_cast<int>(_screenWidth * scale));
		int targetH = Math::Max(180, static_cast<int>(_screenHeight * scale));

		if (!wantScaler)
		{
			DestroyScaler();

			return;
		}

		if (_scaleFBO != 0 && _scaleW == targetW && _scaleH == targetH)
		{
			return;
		}

		DestroyScaler();

		glGenFramebuffers(1, &_scaleFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, _scaleFBO);

		glGenTextures(1, &_scaleColor);
		glBindTexture(GL_TEXTURE_2D, _scaleColor);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, targetW, targetH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _scaleColor, 0);

		glGenRenderbuffers(1, &_scaleDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, _scaleDepth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, targetW, targetH);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _scaleDepth);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cout << "Scaler FBO incomplete, rendering direct." << std::endl;
			DestroyScaler();

			return;
		}

		_scaleW = targetW;
		_scaleH = targetH;
	}

	void Game::DestroyScaler()
	{
		if (_scaleDepth != 0)
		{
			glDeleteRenderbuffers(1, &_scaleDepth);
			_scaleDepth = 0;
		}

		if (_scaleColor != 0)
		{
			glDeleteTextures(1, &_scaleColor);
			_scaleColor = 0;
		}

		if (_scaleFBO != 0)
		{
			glDeleteFramebuffers(1, &_scaleFBO);
			_scaleFBO = 0;
		}

		_scaleW = 0;
		_scaleH = 0;
	}

	void Game::RenderWorld()
	{
		EnsureScaler();

		bool scaled = _scaleFBO != 0;
		_renderWidth = scaled ? _scaleW : _screenWidth;
		_renderHeight = scaled ? _scaleH : _screenHeight;

		if (scaled)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, _scaleFBO);
		}

		glViewport(0, 0, _renderWidth, _renderHeight);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glCullFace(GL_BACK);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (_map == nullptr)
		{
			if (scaled)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}

			return;
		}

		Matrix4x4 projectionMatrix = Matrix4x4::Perspective(80, (float)_renderWidth / (float)_renderHeight, 0.1f, 5000.0f);
		Matrix4x4 viewMatrix = _camera->GetTransform();
		Matrix4x4 viewProjectionMatrix = projectionMatrix * viewMatrix;

		Renderer::ProjectionMatrix = projectionMatrix;
		Renderer::ViewMatrix = viewMatrix;
		Renderer::ViewportWidth = static_cast<float>(_renderWidth);
		Renderer::ViewportHeight = static_cast<float>(_renderHeight);

		Shader::GlobalUniforms.Uniforms.viewMatrix = viewMatrix;
		Shader::GlobalUniforms.Uniforms.projectionMatrix = projectionMatrix;
		Shader::GlobalUniforms.Uniforms.viewProjectionMatrix = viewProjectionMatrix;
		Shader::GlobalUniforms.Update();

		_map->Render();

		if (_skybox != nullptr)
		{
			glDepthMask(GL_FALSE);
			glDepthFunc(GL_LEQUAL);
			Matrix4x4 skyboxView = viewMatrix;
			skyboxView.Translate(0);

			Shader::GlobalUniforms.Uniforms.viewMatrix = skyboxView;
			Shader::GlobalUniforms.Update();

			_skybox->Draw();

			Shader::GlobalUniforms.Uniforms.viewMatrix = viewMatrix;
			Shader::GlobalUniforms.Update();

			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
		}

		_billboards->Draw(0.016, _camera->GetPosition(), _camera->GetRotation().Forward());

		RenderViewmodel();

		if (!_tracers.empty())
		{
			for (const auto& tracer : _tracers)
			{
				_lineRenderer->DrawLine(tracer.a, tracer.b, tracer.color);
			}

			_lineRenderer->Flush();
			_lineRenderer->Clear();
		}

		if (Renderer::DebugDraw)
		{
			RenderDebugOverlay(projectionMatrix, viewMatrix);

			glDisable(GL_DEPTH_TEST);
			_lineRenderer->Flush();
			glEnable(GL_DEPTH_TEST);

			_lineRenderer->Clear();
		}

		if (scaled)
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, _scaleFBO);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(0, 0, _scaleW, _scaleH, 0, 0, _screenWidth, _screenHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}

	void Game::RenderViewmodel()
	{
		if (_viewmodel == nullptr || _viewmodelAnimator == nullptr)
		{
			return;
		}

		glClear(GL_DEPTH_BUFFER_BIT);

		const auto& shader = Shader::Library.DynamicModel;
		if (shader == nullptr)
		{
			return;
		}

		shader->Bind();

		Matrix4x4 viewmodelProjectionMatrix = Matrix4x4::Perspective(73, (float)_renderWidth / (float)_renderHeight, 0.1f, 100.0f);

		Shader::GlobalUniforms.Uniforms.projectionMatrix = viewmodelProjectionMatrix;
		Shader::GlobalUniforms.Uniforms.viewProjectionMatrix = viewmodelProjectionMatrix;
		Shader::GlobalUniforms.Update();

		Matrix4x4 modelMatrix = Matrix4x4::Translation(_camera->GetViewModelOffset()) *
			Matrix3x3::RotationY(Math::DegreesToRadians(90)).ToMatrix4x4();

		shader->SetParameterValue("modelMatrix", modelMatrix);
		shader->SetParameterValue("normalBuffer", DynamicModel::GetNormalBuffer().get());
		if (_skybox != nullptr && _skybox->GetCubemap() != nullptr)
		{
			shader->SetParameterValue("cubemap", _skybox->GetCubemap(),
				TextureSampler::Library.Get({ TextureWrapMode::ClampEdge, TextureFilterMode::Linear }).get());
		}

		if (!_viewmodel->Skins.empty())
		{
			auto diffuse = Texture2D::Library.Get(_viewmodel->Skins[0]);
			if (diffuse == nullptr)
			{
				diffuse = Texture2D::GetFallback();
			}

			shader->SetParameterValue("diffuse", diffuse.get());
			_viewmodel->SetFrameUniforms(shader.get(), _viewmodelAnimator->GetFrame(),
				_viewmodelAnimator->GetNextFrame(), _viewmodelAnimator->GetFrameDelta());
			_viewmodel->Draw();
		}

		if (_viewmodelHand != nullptr && !_viewmodelHand->Skins.empty())
		{
			auto diffuse = Texture2D::Library.Get(_viewmodelHand->Skins[0]);
			if (diffuse == nullptr)
			{
				diffuse = Texture2D::GetFallback();
			}

			shader->SetParameterValue("diffuse", diffuse.get());
			_viewmodelHand->SetFrameUniforms(shader.get(), _viewmodelAnimator->GetFrame(),
				_viewmodelAnimator->GetNextFrame(), _viewmodelAnimator->GetFrameDelta());
			_viewmodelHand->Draw();
		}
	}

	void Game::RenderDebugOverlay(const Matrix4x4& projectionMatrix, const Matrix4x4& viewMatrix)
	{
		if (_map == nullptr)
		{
			return;
		}

		for (const auto& entDef : _map->GetEntityProperties())
		{
			if (!entDef.GetOriginProperty())
			{
				continue;
			}

			auto origin = *entDef.GetOriginProperty();
			origin = Vector3f(origin.x, origin.z, -origin.y);
			float distance = origin.LengthBetween(_camera->GetPosition());

			if (distance >= 512.0f)
			{
				continue;
			}

			Vector2f screenPosition;
			if (Util::WorldPointToNormalisedScreenPoint(origin, screenPosition, projectionMatrix, viewMatrix, 512.0f))
			{
				float alpha = 1.0f - (distance / 512.0f);
				_lineRenderer->DrawSphere(origin, 4.0f, 4, 4, LinearColor(0, 1, 1, alpha));
				screenPosition = Util::ScreenSpaceToPixelPosition(screenPosition, Vector4i(0, 0, _renderWidth, _renderHeight));
				screenPosition.x = Math::Round(screenPosition.x);
				screenPosition.y = Math::Round(screenPosition.y);
				auto text = *entDef.GetClassnameProperty() + " (" + *entDef.GetNameProperty() + ")";
				_spriteBatch->DrawText(_font.get(), text, screenPosition + Vector2f(2, 2), LinearColor(0, 0, 0, alpha), 0.5f);
				_spriteBatch->DrawText(_font.get(), text, screenPosition, LinearColor(1, 1, 1, alpha), 0.5f);
			}
		}

		for (size_t i = 0; i < _navNodes.size(); ++i)
		{
			const auto& navpoint = _navNodes[i];
			auto origin = Vector3(navpoint.Position.y, navpoint.Position.w, -navpoint.Position.z);

			float distance = origin.LengthBetween(_camera->GetPosition());

			if (distance >= 512.0f)
			{
				continue;
			}

			Vector2f screenPosition;
			if (Util::WorldPointToNormalisedScreenPoint(origin, screenPosition, projectionMatrix, viewMatrix, 512.0f))
			{
				float alpha = 1.0f - (distance / 512.0f);
				_lineRenderer->DrawAABBox(origin, Vector3f(-5, -5, -5), Vector3f(5, 5, 5), LinearColor(0, 1, 0, alpha));
				screenPosition = Util::ScreenSpaceToPixelPosition(screenPosition, Vector4i(0, 0, _renderWidth, _renderHeight));
				screenPosition.x = Math::Round(screenPosition.x);
				screenPosition.y = Math::Round(screenPosition.y);
				auto text = "node #" + std::to_string(i);
				_spriteBatch->DrawText(_font.get(), text, screenPosition + Vector2f(2, 2), LinearColor(0, 0, 0, alpha), 0.5f);
				_spriteBatch->DrawText(_font.get(), text, screenPosition, LinearColor(1, 1, 1, alpha), 0.5f);
			}
		}
	}

	void Game::RenderFrame()
	{
		// UI draw lists are built here (menus), the 3D world and the 2D
		// overlay are drawn after.
		RenderMenus();

		RenderWorld();
		RenderUI();
	}

	void Game::RenderUI()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, _screenWidth, _screenHeight);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		auto orthoProjection = Matrix4x4::Ortho(0, (float)_screenWidth, (float)_screenHeight, 0, -1.0f, 1.0f);
		Shader::GlobalUniforms.Uniforms.projectionMatrix = orthoProjection;
		Shader::GlobalUniforms.Update();

		if (_state == State::Playing || _state == State::Paused)
		{
			// Crosshair.
			float crossSize = 32.0f * (1.0f + _crosshairPulse * 0.4f);
			_spriteBatch->Draw(_crosshair.get(),
				Vector2f((_screenWidth * 0.5f) - crossSize * 0.5f, (_screenHeight * 0.5f) - crossSize * 0.5f),
				Vector2f(crossSize, crossSize), LinearColor::White.WithAlpha(0.8f));

			if (_state == State::Playing)
			{
				_touch->Render(_spriteBatch.get(), _font.get());
			}
		}

		_spriteBatch->Flush();
		_spriteBatch->Clear();

		RenderHud();
		RenderToast();

		if (_state == State::Playing && _touch->IsEditMode())
		{
			_touch->RenderEditPanel();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		_window->Swap();
	}

	static void BeginFullscreenWindow(const char* id)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(io.DisplaySize);
		ImGui::Begin(id, nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
	}

	static bool CenteredButton(const char* label, float width)
	{
		float x = (ImGui::GetWindowContentRegionWidth() - width) * 0.5f;
		if (x < 0.0f) x = 0.0f;
		ImGui::SetCursorPosX(x);

		return ImGui::Button(label, ImVec2(width, 0));
	}

	static void CenteredText(const char* text)
	{
		float x = (ImGui::GetWindowContentRegionWidth() - ImGui::CalcTextSize(text).x) * 0.5f;
		if (x < 0.0f) x = 0.0f;
		ImGui::SetCursorPosX(x);
		ImGui::TextUnformatted(text);
	}

	void Game::RenderMenus()
	{
		switch (_state)
		{
		case State::MainMenu:
			RenderMainMenu();
			break;
		case State::Loading:
		{
			BeginFullscreenWindow("loading");
			ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMax().y * 0.4f);
			ImGui::SetWindowFontScale(1.6f);
			CenteredText(Lang::T("loading"));
			ImGui::SetWindowFontScale(1.0f);
			ImGui::End();
			break;
		}
		case State::Paused:
			RenderPauseMenu();
			break;
		case State::NoFiles:
			RenderNoFilesPage();
			break;
		case State::Error:
			RenderErrorPage();
			break;
		case State::Playing:
			break;
		}
	}

	void Game::RenderMainMenu()
	{
		BeginFullscreenWindow("mainmenu");

		float contentWidth = ImGui::GetWindowContentRegionWidth();
		float buttonWidth = contentWidth < 480.0f ? contentWidth * 0.8f : 360.0f;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y * 0.08f);
		ImGui::SetWindowFontScale(2.4f);
		CenteredText("FREEKING");
		ImGui::SetWindowFontScale(1.0f);
		CenteredText(Lang::T("app_subtitle"));
		ImGui::Spacing();
		ImGui::Spacing();

		if (_menuPage == MenuPage::Main)
		{
			std::string lastMap = Config::LastMap();
			bool hasContinue = !lastMap.empty() &&
				std::find(_mapList.begin(), _mapList.end(), lastMap) != _mapList.end();

			if (hasContinue && CenteredButton(Lang::T("menu_continue"), buttonWidth))
			{
				QueueMapChange(lastMap);
			}
			if (CenteredButton(Lang::T("menu_new_game"), buttonWidth))
			{
				QueueMapChange(DefaultMap());
			}
			if (CenteredButton(Lang::T("menu_maps"), buttonWidth))
			{
				_menuPage = MenuPage::Maps;
			}
			if (CenteredButton(Lang::T("menu_settings"), buttonWidth))
			{
				_menuPage = MenuPage::Settings;
			}
			if (CenteredButton(Lang::T("menu_help"), buttonWidth))
			{
				_menuPage = MenuPage::Help;
			}
			if (CenteredButton(Lang::T("menu_quit"), buttonWidth))
			{
				_running = false;
			}
		}
		else if (_menuPage == MenuPage::Maps)
		{
			RenderMapsPage(false);
		}
		else if (_menuPage == MenuPage::Settings)
		{
			RenderSettingsPanel();
			ImGui::Spacing();
			if (CenteredButton(Lang::T("set_back"), buttonWidth))
			{
				_menuPage = MenuPage::Main;
				Config::Save();
			}
		}
		else if (_menuPage == MenuPage::Help)
		{
			RenderHelpPage();
		}

		ImGui::End();
	}

	void Game::RenderPauseMenu()
	{
		if (_pausePage == PausePage::Settings)
		{
			BeginFullscreenWindow("pausesettings");
			CenteredText(Lang::T("pause_settings"));
			ImGui::Spacing();
			RenderSettingsPanel();
			ImGui::Spacing();

			float contentWidth = ImGui::GetWindowContentRegionWidth();
			float buttonWidth = contentWidth < 480.0f ? contentWidth * 0.8f : 360.0f;
			if (CenteredButton(Lang::T("set_back"), buttonWidth))
			{
				_pausePage = PausePage::Main;
				Config::Save();
			}

			ImGui::End();

			return;
		}

		if (_pausePage == PausePage::Maps)
		{
			BeginFullscreenWindow("pausemaps");
			CenteredText(Lang::T("pause_maps"));
			ImGui::Spacing();
			RenderMapsPage(true);
			ImGui::End();

			return;
		}

		BeginFullscreenWindow("pause");

		float contentWidth = ImGui::GetWindowContentRegionWidth();
		float buttonWidth = contentWidth < 480.0f ? contentWidth * 0.8f : 360.0f;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y * 0.15f);
		ImGui::SetWindowFontScale(2.0f);
		CenteredText(Lang::T("pause_title"));
		ImGui::SetWindowFontScale(1.0f);
		ImGui::Spacing();

		if (CenteredButton(Lang::T("pause_resume"), buttonWidth))
		{
			_state = State::Playing;
		}
		if (CenteredButton(Lang::T("pause_restart"), buttonWidth) && _map != nullptr)
		{
			QueueMapChange(_map->GetName());
		}
		if (CenteredButton(Lang::T("pause_maps"), buttonWidth))
		{
			_pausePage = PausePage::Maps;
		}
		if (CenteredButton(Lang::T("pause_settings"), buttonWidth))
		{
			_pausePage = PausePage::Settings;
		}
		if (CenteredButton(Lang::T("pause_quit_menu"), buttonWidth))
		{
			UnloadMap();
			_state = State::MainMenu;
			_menuPage = MenuPage::Main;
			LockMouse(false);
			Config::Save();
		}

		ImGui::End();

		if (Input::JustPressed(Button::KeyESCAPE))
		{
			_state = State::Playing;
		}
	}

	void Game::RenderMapsPage(bool inPause)
	{
		CenteredText(Lang::T("maps_title"));
		ImGui::Spacing();

		if (_mapList.empty())
		{
			CenteredText(Lang::T("maps_empty"));
		}
		else
		{
			float contentWidth = ImGui::GetWindowContentRegionWidth();
			float listWidth = contentWidth < 560.0f ? contentWidth * 0.9f : 480.0f;
			float x = (contentWidth - listWidth) * 0.5f;
			if (x < 0.0f) x = 0.0f;
			ImGui::SetCursorPosX(x);
			ImGui::BeginChild("maplist", ImVec2(listWidth, ImGui::GetContentRegionAvail().y * 0.55f), true);

			std::string lastMap = Config::LastMap();
			for (const auto& map : _mapList)
			{
				std::string label = map;
				if (map == lastMap)
				{
					label += std::string(" [") + Lang::T("maps_last") + "]";
				}

				if (ImGui::Button(label.c_str(), ImVec2(-1, 0)))
				{
					QueueMapChange(map);
				}
			}

			ImGui::EndChild();
		}

		ImGui::Spacing();

		float contentWidth = ImGui::GetWindowContentRegionWidth();
		float buttonWidth = contentWidth < 480.0f ? contentWidth * 0.8f : 360.0f;
		if (CenteredButton(Lang::T("maps_back"), buttonWidth))
		{
			if (inPause)
			{
				_pausePage = PausePage::Main;
			}
			else
			{
				_menuPage = MenuPage::Main;
			}
		}
	}

	void Game::RenderSettingsPanel()
	{
		float contentWidth = ImGui::GetWindowContentRegionWidth();
		float panelWidth = contentWidth < 620.0f ? contentWidth * 0.92f : 540.0f;
		float x = (contentWidth - panelWidth) * 0.5f;
		if (x < 0.0f) x = 0.0f;
		ImGui::SetCursorPosX(x);
		ImGui::BeginChild("settings", ImVec2(panelWidth, 0), false, ImGuiWindowFlags_NoBackground);

		float sensitivity = Config::Sensitivity();
		if (ImGui::SliderFloat(Lang::T("set_sensitivity"), &sensitivity, 0.1f, 3.0f))
		{
			Config::SetSensitivity(sensitivity);
		}

		bool invertY = Config::InvertY();
		if (ImGui::Checkbox(Lang::T("set_invert_y"), &invertY))
		{
			Config::SetInvertY(invertY);
		}

		float renderScale = Config::RenderScale();
		if (ImGui::SliderFloat(Lang::T("set_render_scale"), &renderScale, 0.5f, 1.0f))
		{
			Config::SetRenderScale(renderScale);
			DestroyScaler();
		}

		int fpsLimit = Config::FpsLimit();
		int fpsIndex = (fpsLimit == 30) ? 1 : ((fpsLimit == 60) ? 2 : ((fpsLimit == 120) ? 3 : 0));
		char fpsPreview[64];
		std::snprintf(fpsPreview, sizeof(fpsPreview), "%s", fpsIndex == 0 ? Lang::T("set_fps_unlimited") : std::to_string(fpsLimit).c_str());
		if (ImGui::BeginCombo(Lang::T("set_fps_limit"), fpsPreview))
		{
			const int values[] = { 0, 30, 60, 120 };
			for (int i = 0; i < 4; ++i)
			{
				bool selected = (fpsIndex == i);
				char label[64];
				if (i == 0)
				{
					std::snprintf(label, sizeof(label), "%s", Lang::T("set_fps_unlimited"));
				}
				else
				{
					std::snprintf(label, sizeof(label), "%d", values[i]);
				}

				if (ImGui::Selectable(label, selected))
				{
					Config::SetFpsLimit(values[i]);
				}
			}

			ImGui::EndCombo();
		}

		float soundVolume = Config::SoundVolume();
		if (ImGui::SliderFloat(Lang::T("set_sound"), &soundVolume, 0.0f, 1.0f))
		{
			Config::SetSoundVolume(soundVolume);
			_audio->SetMasterVolume(soundVolume);
		}

		bool touchEnabled = Config::TouchEnabled();
		if (ImGui::Checkbox(Lang::T("set_touch"), &touchEnabled))
		{
			_touch->SetEnabled(touchEnabled);
		}

		bool showFps = Config::ShowFps();
		if (ImGui::Checkbox(Lang::T("set_show_fps"), &showFps))
		{
			Config::SetShowFps(showFps);
		}

		bool debugDraw = Renderer::DebugDraw;
		if (ImGui::Checkbox(Lang::T("set_debug"), &debugDraw))
		{
			Renderer::DebugDraw = debugDraw;
		}

		int msaa = Config::MsaaSamples();
		int msaaIndex = (msaa == 2) ? 1 : ((msaa == 4) ? 2 : 0);
		char msaaPreview[16];
		std::snprintf(msaaPreview, sizeof(msaaPreview), "%dx", msaa);
		if (ImGui::BeginCombo(Lang::T("set_msaa"), msaaPreview))
		{
			const int values[] = { 0, 2, 4 };
			for (int i = 0; i < 3; ++i)
			{
				char label[16];
				std::snprintf(label, sizeof(label), "%dx", values[i]);
				if (ImGui::Selectable(label, msaaIndex == i))
				{
					Config::SetMsaaSamples(values[i]);
				}
			}

			ImGui::EndCombo();
		}

		ImGui::TextDisabled("%s", Lang::T("set_msaa_restart"));

		if (_imguiCyrillic)
		{
			bool russian = Lang::IsRussian();
			int langIndex = russian ? 1 : 0;
			const char* langNames[] = { "English", "Russkiy" };
			if (ImGui::BeginCombo(Lang::T("set_language"), langNames[langIndex]))
			{
				for (int i = 0; i < 2; ++i)
				{
					if (ImGui::Selectable(langNames[i], langIndex == i))
					{
						Lang::SetRussian(i == 1);
					}
				}

				ImGui::EndCombo();
			}
		}

		ImGui::EndChild();
	}

	void Game::RenderHelpPage()
	{
		CenteredText(Lang::T("help_title"));
		ImGui::Spacing();

		float contentWidth = ImGui::GetWindowContentRegionWidth();
		float textWidth = contentWidth < 700.0f ? contentWidth * 0.92f : 620.0f;
		float x = (contentWidth - textWidth) * 0.5f;
		if (x < 0.0f) x = 0.0f;
		ImGui::SetCursorPosX(x);
		ImGui::BeginChild("helptext", ImVec2(textWidth, ImGui::GetContentRegionAvail().y * 0.6f), true);
		ImGui::TextWrapped("%s", Lang::T("help_text"));
		ImGui::EndChild();

		ImGui::Spacing();

		float buttonWidth = contentWidth < 480.0f ? contentWidth * 0.8f : 360.0f;
		if (CenteredButton(Lang::T("maps_back"), buttonWidth))
		{
			_menuPage = MenuPage::Main;
		}
	}

	void Game::RenderNoFilesPage()
	{
		BeginFullscreenWindow("nofiles");

		float contentWidth = ImGui::GetWindowContentRegionWidth();
		float textWidth = contentWidth < 700.0f ? contentWidth * 0.92f : 620.0f;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y * 0.1f);
		ImGui::SetWindowFontScale(1.8f);
		CenteredText(Lang::T("files_title"));
		ImGui::SetWindowFontScale(1.0f);
		ImGui::Spacing();

		float x = (contentWidth - textWidth) * 0.5f;
		if (x < 0.0f) x = 0.0f;
		ImGui::SetCursorPosX(x);
		ImGui::BeginChild("filestext", ImVec2(textWidth, ImGui::GetContentRegionAvail().y * 0.5f), true);
#ifdef __ANDROID__
		ImGui::TextWrapped("%s", Lang::T("files_text_android"));
#else
		ImGui::TextWrapped("%s", Lang::T("files_text_desktop"));
#endif
		ImGui::EndChild();

		ImGui::Spacing();

		float buttonWidth = contentWidth < 480.0f ? contentWidth * 0.8f : 360.0f;
		if (CenteredButton(Lang::T("files_back"), buttonWidth))
		{
			_running = false;
		}

		ImGui::End();
	}

	void Game::RenderErrorPage()
	{
		BeginFullscreenWindow("errorpage");

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y * 0.2f);
		ImGui::SetWindowFontScale(1.8f);
		CenteredText(Lang::T("error_title"));
		ImGui::SetWindowFontScale(1.0f);
		ImGui::Spacing();

		float contentWidth = ImGui::GetWindowContentRegionWidth();
		float textWidth = contentWidth < 700.0f ? contentWidth * 0.92f : 620.0f;
		float x = (contentWidth - textWidth) * 0.5f;
		if (x < 0.0f) x = 0.0f;
		ImGui::SetCursorPosX(x);
		ImGui::TextWrapped("%s", _errorMessage.c_str());

		ImGui::Spacing();

		float buttonWidth = contentWidth < 480.0f ? contentWidth * 0.8f : 360.0f;
		if (CenteredButton(Lang::T("error_back"), buttonWidth))
		{
			_state = State::MainMenu;
			_menuPage = MenuPage::Main;
		}

		ImGui::End();
	}

	void Game::RenderHud()
	{
		if (_state != State::Playing && _state != State::Paused)
		{
			return;
		}

		if (!Config::ShowFps() && _map == nullptr)
		{
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.35f);
		if (ImGui::Begin("hud", nullptr,
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoInputs))
		{
			if (_map != nullptr)
			{
				ImGui::Text("%s", _map->GetName().c_str());
			}

			if (Config::ShowFps())
			{
				ImGui::Text("%.0f FPS", io.Framerate);
			}
		}

		ImGui::End();
	}

	void Game::RenderToast()
	{
		if (_toastText.empty() || Time::Now() >= _toastUntil)
		{
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.82f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowBgAlpha(0.6f);
		if (ImGui::Begin("toast", nullptr,
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoInputs))
		{
			ImGui::TextUnformatted(_toastText.c_str());
		}

		ImGui::End();
	}

	void Game::MainLoop()
	{
		Time::SetTimeApplicationStart();

		uint64_t now = SDL_GetPerformanceCounter();
		uint64_t last = 0;
		double deltaTime = 0.0;

		SDL_Event e;

		while (_running)
		{
			if (!_pendingMap.empty())
			{
				ConsumePendingMap();

				if (!_running)
				{
					break;
				}
			}

			Time::Update();

			last = now;
			now = SDL_GetPerformanceCounter();
			deltaTime = ((now - last) / (double)SDL_GetPerformanceFrequency());
			if (deltaTime > 0.1)
			{
				deltaTime = 0.1;
			}
			if (deltaTime < 0.0)
			{
				deltaTime = 0.0;
			}

			Input::PreEvent();

			while (SDL_PollEvent(&e))
			{
				HandleEvent(e);

				if (!_running)
				{
					break;
				}
			}

			if (!_running)
			{
				break;
			}

			if (_state == State::Playing && _map != nullptr)
			{
				UpdatePlaying(deltaTime);
			}
			else if (_state == State::Playing && _map == nullptr)
			{
				_state = State::MainMenu;
			}

			if (_viewmodelAnimator != nullptr && _state == State::Playing)
			{
				_viewmodelAnimator->Tick(deltaTime);
			}

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame(static_cast<SDL_Window*>(*_window));
			ImGui::NewFrame();

			RenderFrame();

			int fpsLimit = Config::FpsLimit();
			if (fpsLimit > 0)
			{
				uint64_t frameEnd = SDL_GetPerformanceCounter();
				double frameMs = ((frameEnd - now) / (double)SDL_GetPerformanceFrequency()) * 1000.0;
				double targetMs = 1000.0 / fpsLimit;
				if (frameMs < targetMs)
				{
					SDL_Delay(static_cast<uint32_t>(targetMs - frameMs));
				}
			}
		}
	}

	void Game::Run()
	{
		MainLoop();

		Config::Save();
		_touch->SaveLayout();
	}
}
