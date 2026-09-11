#pragma once

#include "ButtonCodes.h"
#include "Vector.h"
#include <SDL.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Freeking
{
	class SpriteBatch;
	class Font;
	class Texture2D;

	// On-screen touch controls: analog joystick (move), free-look drag area
	// and action buttons mapped to the same Button codes as the keyboard, so
	// gameplay code needs no touch-specific branches.
	//
	// The EDIT button opens an edit mode where every button can be moved
	// (drag), resized (drag the corner handle) and hidden (visibility panel),
	// and the whole touch layer can be switched off. The layout persists in
	// the user directory.
	class TouchControls
	{
	public:

		enum class ButtonId : size_t
		{
			Fire = 0,
			Jump,
			Use,
			Run,
			Menu,
			Noclip,
			Edit,
			Count
		};

		struct ButtonLayout
		{
			// Normalized screen coordinates (0..1), origin top-left.
			float x = 0.0f;
			float y = 0.0f;
			float w = 0.1f;
			float h = 0.15f;
			bool visible = true;
		};

		struct JoystickLayout
		{
			// Normalized center + radius as a fraction of screen height.
			float x = 0.14f;
			float y = 0.74f;
			float radius = 0.13f;
			bool visible = true;
		};

		TouchControls();
		~TouchControls();

		void SetScreenSize(int width, int height);
		void SetEnabled(bool enabled);
		bool IsEnabled() const { return _enabled; }
		void SetEditMode(bool editMode);
		bool IsEditMode() const { return _editMode; }

		void HandleEvent(const SDL_Event& e);
		// Releases every pressed button/stick (call on pause/unload/focus loss).
		void ReleaseAll();

		// Accumulated look drag in pixels since the last call (already scaled
		// by the sensitivity setting, but NOT Y-inverted), zeroed by reading.
		Vector2f ConsumeLookDelta();
		// Analog move vector: x = strafe right, y = forward.
		Vector2f GetMoveVector() const { return _moveVector; }

		bool IsAnyButtonDown() const;

		void Render(SpriteBatch* batch, const Font* font);
		void RenderEditPanel();

		void LoadLayout();
		void SaveLayout();
		void ResetLayout();

	private:

		struct ButtonInfo
		{
			ButtonId id;
			const char* name;
			const char* label;
			Button button;
			ButtonLayout layout;
		};

		enum class TouchKind
		{
			Look,
			Button,
			EditMove,
			EditResize,
			StickMove,
			StickResize
		};

		struct ActiveTouch
		{
			TouchKind kind = TouchKind::Look;
			ButtonId button = ButtonId::Fire;
			float grabDX = 0.0f;
			float grabDY = 0.0f;
		};

		void FingerDown(float x, float y, SDL_FingerID finger);
		void FingerUp(SDL_FingerID finger);
		void FingerMove(float x, float y, float dx, float dy, SDL_FingerID finger);

		void MouseDown(float x, float y);
		void MouseUp();
		void MouseMove(float x, float y);

		void SetButtonDown(ButtonId id, bool down);
		bool IsButtonDown(ButtonId id) const;

		bool HitTest(const ButtonLayout& layout, float x, float y) const;
		bool HitTestHandle(const ButtonLayout& layout, float x, float y) const;
		bool HitTestStick(float x, float y) const;
		bool HitTestStickHandle(float x, float y) const;
		void ToPixels(const ButtonLayout& layout, float& x, float& y, float& w, float& h) const;

		void EnsureTextures();

		int _screenWidth = 1;
		int _screenHeight = 1;
		bool _enabled = true;
		bool _editMode = false;

		std::vector<ButtonInfo> _buttons;
		JoystickLayout _stick;

		std::unordered_map<SDL_FingerID, ActiveTouch> _touches;
		bool _mouseTouchActive = false;
		static const SDL_FingerID MouseFingerId = static_cast<SDL_FingerID>(-1);

		Vector2f _lookDelta = Vector2f(0, 0);
		Vector2f _moveVector = Vector2f(0, 0);
		Vector2f _stickKnob = Vector2f(0, 0);
		SDL_FingerID _stickFinger = 0;
		bool _stickActive = false;

		ButtonId _editSelection = ButtonId::Fire;
		bool _editStickSelected = false;
		bool _buttonsDown[static_cast<size_t>(ButtonId::Count)] = {};

		std::shared_ptr<Texture2D> _circleTexture;
		std::shared_ptr<Texture2D> _squareTexture;
	};
}
