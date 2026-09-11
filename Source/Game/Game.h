#pragma once

#include "Vector.h"
#include "LinearColor.h"
#include "Matrix4x4.h"
#include <SDL.h>
#include <memory>
#include <string>
#include <vector>
#include <stdint.h>

namespace Freeking
{
	class Window;
	class FreeCamera;
	class TouchControls;
	class AudioDevice;
	class SpriteBatch;
	class LineRenderer;
	class Font;
	class Texture2D;
	class DynamicModel;
	class FrameAnimator;
	class Map;
	class Skybox;
	class BillboardBatch;
	struct NavNode;

	class Game
	{
	public:

		static Game* Instance() { return _instance; }

		Game(int argc, char** argv);
		~Game();

		void Run();

		// Queues a map change (safe to call from entity logic; the switch
		// happens at the start of the next frame).
		void QueueMapChange(const std::string& mapName);

	private:

		enum class State
		{
			MainMenu,
			Loading,
			Playing,
			Paused,
			NoFiles,
			Error
		};

		enum class MenuPage
		{
			Main,
			Maps,
			Settings,
			Help
		};

		enum class PausePage
		{
			Main,
			Maps,
			Settings
		};

		struct Tracer
		{
			Vector3f a;
			Vector3f b;
			LinearColor color;
			double ttl;
		};

		void MainLoop();
		void HandleEvent(const SDL_Event& e);
		void OnResize(int width, int height);
		void LockMouse(bool lockMouse);

		void ConsumePendingMap();
		void LoadMapSync(const std::string& mapName);
		void UnloadMap();
		void RescanMaps();
		std::string DefaultMap() const;

		void UpdatePlaying(double dt);
		void TryFire();
		void TryUse();
		void CheckTouchTriggers();
		void CheckExits();

		void RenderFrame();
		void RenderWorld();
		void RenderViewmodel();
		void RenderDebugOverlay(const Matrix4x4& projectionMatrix, const Matrix4x4& viewMatrix);
		void RenderUI();
		void RenderMenus();
		void RenderMainMenu();
		void RenderPauseMenu();
		void RenderMapsPage(bool inPause);
		void RenderSettingsPanel();
		void RenderHelpPage();
		void RenderNoFilesPage();
		void RenderErrorPage();
		void RenderHud();
		void RenderToast();

		void EnsureScaler();
		void DestroyScaler();

		void ShowToast(const std::string& text, double seconds);

		static Game* _instance;

		std::unique_ptr<class Window> _window;
		std::unique_ptr<class FreeCamera> _camera;
		std::unique_ptr<class TouchControls> _touch;
		std::unique_ptr<class AudioDevice> _audio;

		std::shared_ptr<class SpriteBatch> _spriteBatch;
		std::shared_ptr<class LineRenderer> _lineRenderer;
		std::shared_ptr<class Font> _font;
		std::shared_ptr<class Texture2D> _crosshair;
		std::shared_ptr<class DynamicModel> _viewmodel;
		std::shared_ptr<class DynamicModel> _viewmodelHand;
		std::unique_ptr<class FrameAnimator> _viewmodelAnimator;
		std::shared_ptr<class BillboardBatch> _billboards;

		std::shared_ptr<class Map> _map;
		std::unique_ptr<class Skybox> _skybox;
		std::vector<struct NavNode> _navNodes;

		std::vector<std::string> _mapList;

		struct ExitZone
		{
			Vector3f mins;
			Vector3f maxs;
			std::string map;
		};
		std::vector<ExitZone> _exits;
		std::vector<Tracer> _tracers;

		std::string _weaponSound;

		State _state = State::MainMenu;
		MenuPage _menuPage = MenuPage::Main;
		PausePage _pausePage = PausePage::Main;
		std::string _pendingMap;
		std::string _errorMessage;

		std::string _toastText;
		double _toastUntil = 0.0;

		int _screenWidth = 1;
		int _screenHeight = 1;
		int _renderWidth = 1;
		int _renderHeight = 1;

		uint32_t _scaleFBO = 0;
		uint32_t _scaleColor = 0;
		uint32_t _scaleDepth = 0;
		int _scaleW = 0;
		int _scaleH = 0;

		bool _mouseLocked = false;
		bool _lockMouseOnLoad = false;
		bool _running = true;
		bool _imguiCyrillic = false;

		double _fireCooldown = 0.0;
		double _exitCooldown = 0.0;
		float _crosshairPulse = 0.0f;
		bool _wasNoclip = false;
	};
}
