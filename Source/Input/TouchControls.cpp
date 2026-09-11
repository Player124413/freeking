#include "TouchControls.h"
#include "Input.h"
#include "Config.h"
#include "Lang.h"
#include "Paths.h"
#include "SpriteBatch.h"
#include "Font.h"
#include "Texture2D.h"
#include "Maths.h"
#include "json.hpp"
#include "ThirdParty/imgui/imgui.h"
#include <fstream>
#include <cmath>

namespace Freeking
{
	const SDL_FingerID TouchControls::MouseFingerId;

	using json = nlohmann::json;

	TouchControls::TouchControls()
	{
		ResetLayout();
		_enabled = Config::TouchEnabled();
	}

	TouchControls::~TouchControls()
	{
	}

	void TouchControls::ResetLayout()
	{
		_buttons.clear();

		auto add = [this](ButtonId id, const char* name, const char* label, Button button,
			float x, float y, float w, float h, bool visible)
		{
			ButtonInfo info;
			info.id = id;
			info.name = name;
			info.label = label;
			info.button = button;
			info.layout = ButtonLayout{ x, y, w, h, visible };
			_buttons.push_back(info);
		};

		add(ButtonId::Fire, "fire", "FIRE", Button::MouseLeft, 0.865f, 0.620f, 0.115f, 0.230f, true);
		add(ButtonId::Jump, "jump", "JUMP", Button::KeySPACE, 0.735f, 0.700f, 0.100f, 0.200f, true);
		add(ButtonId::Use, "use", "USE", Button::KeyE, 0.735f, 0.470f, 0.100f, 0.200f, true);
		add(ButtonId::Run, "run", "RUN", Button::KeyLSHIFT, 0.600f, 0.720f, 0.090f, 0.180f, true);
		add(ButtonId::Menu, "menu", "MENU", Button::KeyESCAPE, 0.455f, 0.020f, 0.090f, 0.120f, true);
		add(ButtonId::Noclip, "noclip", "FLY", Button::KeyV, 0.020f, 0.020f, 0.080f, 0.120f, true);
		add(ButtonId::Edit, "edit", "EDIT", Button::None, 0.895f, 0.020f, 0.085f, 0.120f, true);

		_stick = JoystickLayout{ 0.140f, 0.740f, 0.130f, true };
		_editSelection = ButtonId::Fire;
		_editStickSelected = false;
	}

	void TouchControls::SetScreenSize(int width, int height)
	{
		if (width > 0) _screenWidth = width;
		if (height > 0) _screenHeight = height;
	}

	void TouchControls::SetEnabled(bool enabled)
	{
		_enabled = enabled;
		Config::SetTouchEnabled(enabled);

		if (!enabled)
		{
			ReleaseAll();
			_editMode = false;
		}
	}

	void TouchControls::SetEditMode(bool editMode)
	{
		if (editMode && !_enabled)
		{
			return;
		}

		_editMode = editMode;
		ReleaseAll();

		if (!editMode)
		{
			SaveLayout();
		}
	}

	void TouchControls::ReleaseAll()
	{
		for (auto& button : _buttons)
		{
			SetButtonDown(button.id, false);
		}

		_touches.clear();
		_mouseTouchActive = false;
		_stickActive = false;
		_moveVector = Vector2f(0, 0);
		_stickKnob = Vector2f(0, 0);
	}

	Vector2f TouchControls::ConsumeLookDelta()
	{
		Vector2f delta = _lookDelta;
		_lookDelta = Vector2f(0, 0);

		return delta;
	}

	bool TouchControls::IsAnyButtonDown() const
	{
		for (bool down : _buttonsDown)
		{
			if (down)
			{
				return true;
			}
		}

		return _stickActive;
	}

	void TouchControls::SetButtonDown(ButtonId id, bool down)
	{
		size_t index = static_cast<size_t>(id);
		if (index >= static_cast<size_t>(ButtonId::Count))
		{
			return;
		}

		if (_buttonsDown[index] == down)
		{
			return;
		}

		_buttonsDown[index] = down;
		Input::InjectButton(_buttons[index].button, down);
	}

	bool TouchControls::IsButtonDown(ButtonId id) const
	{
		size_t index = static_cast<size_t>(id);
		if (index >= static_cast<size_t>(ButtonId::Count))
		{
			return false;
		}

		return _buttonsDown[index];
	}

	void TouchControls::ToPixels(const ButtonLayout& layout, float& x, float& y, float& w, float& h) const
	{
		x = layout.x * _screenWidth;
		y = layout.y * _screenHeight;
		w = layout.w * _screenWidth;
		h = layout.h * _screenHeight;
	}

	bool TouchControls::HitTest(const ButtonLayout& layout, float x, float y) const
	{
		float px, py, pw, ph;
		ToPixels(layout, px, py, pw, ph);

		return x >= px && x <= px + pw && y >= py && y <= py + ph;
	}

	bool TouchControls::HitTestHandle(const ButtonLayout& layout, float x, float y) const
	{
		float px, py, pw, ph;
		ToPixels(layout, px, py, pw, ph);

		float handle = Math::Min(pw, ph) * 0.35f;
		if (handle < 28.0f) handle = 28.0f;

		return x >= px + pw - handle && x <= px + pw + 12.0f &&
			y >= py + ph - handle && y <= py + ph + 12.0f;
	}

	bool TouchControls::HitTestStick(float x, float y) const
	{
		float cx = _stick.x * _screenWidth;
		float cy = _stick.y * _screenHeight;
		float r = _stick.radius * _screenHeight * 1.35f;
		float dx = x - cx;
		float dy = y - cy;

		return (dx * dx + dy * dy) <= r * r;
	}

	bool TouchControls::HitTestStickHandle(float x, float y) const
	{
		float cx = _stick.x * _screenWidth;
		float cy = _stick.y * _screenHeight;
		float r = _stick.radius * _screenHeight;

		return x >= cx + r * 0.6f && y >= cy + r * 0.6f &&
			x <= cx + r * 1.5f && y <= cy + r * 1.5f;
	}

	void TouchControls::HandleEvent(const SDL_Event& e)
	{
		// The EDIT button must stay usable even when the layer is disabled,
		// otherwise there is no way back without the pause menu.
		bool layerActive = _enabled || _editMode;

		if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP)
		{
			float x = e.tfinger.x * _screenWidth;
			float y = e.tfinger.y * _screenHeight;

			if (e.type == SDL_FINGERDOWN)
			{
				FingerDown(x, y, e.tfinger.fingerId);
			}
			else if (e.type == SDL_FINGERUP)
			{
				FingerUp(e.tfinger.fingerId);
			}
			else
			{
				float dx = e.tfinger.dx * _screenWidth;
				float dy = e.tfinger.dy * _screenHeight;
				FingerMove(x, y, dx, dy, e.tfinger.fingerId);
			}

			(void)layerActive;

			return;
		}

		// Mouse drives the editor on desktop (real mouse only; emulated
		// touch-mouse is already handled as fingers above).
		if (_editMode && e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT &&
			e.button.which != SDL_TOUCH_MOUSEID)
		{
			MouseDown(static_cast<float>(e.button.x), static_cast<float>(e.button.y));
		}
		else if (_editMode && e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT &&
			e.button.which != SDL_TOUCH_MOUSEID)
		{
			MouseUp();
		}
		else if (_editMode && e.type == SDL_MOUSEMOTION && _mouseTouchActive &&
			e.motion.which != SDL_TOUCH_MOUSEID)
		{
			MouseMove(static_cast<float>(e.motion.x), static_cast<float>(e.motion.y));
		}
	}

	void TouchControls::FingerDown(float x, float y, SDL_FingerID finger)
	{
		if (_touches.find(finger) != _touches.end())
		{
			return;
		}

		// EDIT is always hittable so the layer can be re-enabled.
		{
			const auto& edit = _buttons[static_cast<size_t>(ButtonId::Edit)];
			if (HitTest(edit.layout, x, y))
			{
				if (!_enabled)
				{
					// First tap re-enables the layer so it can never trap
					// the player in a disabled state; tap again to edit.
					SetEnabled(true);
				}
				else
				{
					SetEditMode(!_editMode);
				}

				ActiveTouch touch;
				touch.kind = TouchKind::Button;
				touch.button = ButtonId::Edit;
				_touches.emplace(finger, touch);

				return;
			}
		}

		if (!_enabled)
		{
			return;
		}

		if (_editMode)
		{
			// Resize handles first.
			if (HitTestStickHandle(x, y))
			{
				_editStickSelected = true;
				ActiveTouch touch;
				touch.kind = TouchKind::StickResize;
				_touches.emplace(finger, touch);

				return;
			}

			for (auto& button : _buttons)
			{
				if (HitTestHandle(button.layout, x, y))
				{
					_editSelection = button.id;
					_editStickSelected = false;
					ActiveTouch touch;
					touch.kind = TouchKind::EditResize;
					touch.button = button.id;
					_touches.emplace(finger, touch);

					return;
				}
			}

			if (HitTestStick(x, y))
			{
				_editStickSelected = true;
				ActiveTouch touch;
				touch.kind = TouchKind::StickMove;
				touch.grabDX = x - _stick.x * _screenWidth;
				touch.grabDY = y - _stick.y * _screenHeight;
				_touches.emplace(finger, touch);

				return;
			}

			for (auto& button : _buttons)
			{
				if (HitTest(button.layout, x, y))
				{
					_editSelection = button.id;
					_editStickSelected = false;
					ActiveTouch touch;
					touch.kind = TouchKind::EditMove;
					touch.button = button.id;
					touch.grabDX = x - button.layout.x * _screenWidth;
					touch.grabDY = y - button.layout.y * _screenHeight;
					_touches.emplace(finger, touch);

					return;
				}
			}

			_editStickSelected = false;

			return;
		}

		// Gameplay: joystick first.
		if (_stick.visible && HitTestStick(x, y) && !_stickActive)
		{
			_stickActive = true;
			_stickFinger = finger;
			_stickKnob = Vector2f(0, 0);
			_moveVector = Vector2f(0, 0);

			ActiveTouch touch;
			touch.kind = TouchKind::Look; // reused as "claimed", stick tracked separately
			_touches.emplace(finger, touch);

			FingerMove(x, y, 0, 0, finger);

			return;
		}

		for (auto& button : _buttons)
		{
			if (button.id == ButtonId::Edit)
			{
				continue;
			}

			if (!button.layout.visible)
			{
				continue;
			}

			if (HitTest(button.layout, x, y))
			{
				ActiveTouch touch;
				touch.kind = TouchKind::Button;
				touch.button = button.id;
				_touches.emplace(finger, touch);
				SetButtonDown(button.id, true);

				return;
			}
		}

		// Anything else is free look.
		ActiveTouch touch;
		touch.kind = TouchKind::Look;
		_touches.emplace(finger, touch);
	}

	void TouchControls::FingerUp(SDL_FingerID finger)
	{
		auto it = _touches.find(finger);
		if (it == _touches.end())
		{
			return;
		}

		if (it->second.kind == TouchKind::Button)
		{
			SetButtonDown(it->second.button, false);
		}

		if (_stickActive && finger == _stickFinger)
		{
			_stickActive = false;
			_moveVector = Vector2f(0, 0);
			_stickKnob = Vector2f(0, 0);
		}

		_touches.erase(it);
	}

	void TouchControls::FingerMove(float x, float y, float dx, float dy, SDL_FingerID finger)
	{
		auto it = _touches.find(finger);
		if (it == _touches.end())
		{
			return;
		}

		ActiveTouch& touch = it->second;

		if (_editMode)
		{
			if (touch.kind == TouchKind::EditMove)
			{
				auto& layout = _buttons[static_cast<size_t>(touch.button)].layout;
				layout.x = (x - touch.grabDX) / _screenWidth;
				layout.y = (y - touch.grabDY) / _screenHeight;
				layout.x = Math::Clamp(layout.x, -0.2f, 1.0f);
				layout.y = Math::Clamp(layout.y, -0.2f, 1.0f);
			}
			else if (touch.kind == TouchKind::EditResize)
			{
				auto& layout = _buttons[static_cast<size_t>(touch.button)].layout;
				layout.w = (x - layout.x * _screenWidth) / _screenWidth;
				layout.h = (y - layout.y * _screenHeight) / _screenHeight;
				layout.w = Math::Clamp(layout.w, 0.03f, 0.6f);
				layout.h = Math::Clamp(layout.h, 0.03f, 0.6f);
			}
			else if (touch.kind == TouchKind::StickMove)
			{
				_stick.x = (x - touch.grabDX) / _screenWidth;
				_stick.y = (y - touch.grabDY) / _screenHeight;
				_stick.x = Math::Clamp(_stick.x, 0.0f, 1.0f);
				_stick.y = Math::Clamp(_stick.y, 0.0f, 1.0f);
			}
			else if (touch.kind == TouchKind::StickResize)
			{
				float cx = _stick.x * _screenWidth;
				float cy = _stick.y * _screenHeight;
				float dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
				_stick.radius = Math::Clamp(dist / _screenHeight, 0.05f, 0.35f);
			}

			return;
		}

		if (!_enabled)
		{
			return;
		}

		if (_stickActive && finger == _stickFinger)
		{
			float cx = _stick.x * _screenWidth;
			float cy = _stick.y * _screenHeight;
			float radius = _stick.radius * _screenHeight;

			float ox = (x - cx) / radius;
			float oy = (y - cy) / radius;
			float length = std::sqrt(ox * ox + oy * oy);
			if (length > 1.0f)
			{
				ox /= length;
				oy /= length;
			}

			// Dead zone.
			if (std::fabs(ox) < 0.12f) ox = 0.0f;
			if (std::fabs(oy) < 0.12f) oy = 0.0f;

			_stickKnob = Vector2f(ox, oy);
			_moveVector = Vector2f(ox, -oy);

			return;
		}

		if (touch.kind == TouchKind::Look)
		{
			// Sensitivity is applied here; Y inversion is applied once by
			// the consumer together with the mouse delta.
			float sensitivity = Config::Sensitivity();
			_lookDelta.x += dx * sensitivity;
			_lookDelta.y += dy * sensitivity;
		}
	}

	void TouchControls::MouseDown(float x, float y)
	{
		if (_mouseTouchActive)
		{
			return;
		}

		_mouseTouchActive = true;
		FingerDown(x, y, MouseFingerId);

		if (_touches.find(MouseFingerId) == _touches.end())
		{
			_mouseTouchActive = false;
		}
	}

	void TouchControls::MouseUp()
	{
		if (!_mouseTouchActive)
		{
			return;
		}

		FingerUp(MouseFingerId);
		_mouseTouchActive = false;
	}

	void TouchControls::MouseMove(float x, float y)
	{
		if (!_mouseTouchActive)
		{
			return;
		}

		auto it = _touches.find(MouseFingerId);
		if (it == _touches.end())
		{
			_mouseTouchActive = false;

			return;
		}

		FingerMove(x, y, 0, 0, MouseFingerId);
	}

	void TouchControls::EnsureTextures()
	{
		if (_circleTexture && _squareTexture)
		{
			return;
		}

		{
			const int size = 128;
			std::vector<uint8_t> pixels(size * size * 4);
			for (int y = 0; y < size; ++y)
			{
				for (int xx = 0; xx < size; ++xx)
				{
					float nx = (xx / static_cast<float>(size - 1)) * 2.0f - 1.0f;
					float ny = (y / static_cast<float>(size - 1)) * 2.0f - 1.0f;
					float dist = std::sqrt(nx * nx + ny * ny);

					uint8_t alpha = 0;
					if (dist <= 1.0f)
					{
						float edge = Math::Clamp((1.0f - dist) * 8.0f, 0.0f, 1.0f);
						float ring = (dist > 0.82f) ? 1.0f : 0.45f;
						alpha = static_cast<uint8_t>(edge * ring * 255.0f);
					}

					uint8_t* p = &pixels[(y * size + xx) * 4];
					p[0] = 255;
					p[1] = 255;
					p[2] = 255;
					p[3] = alpha;
				}
			}

			_circleTexture = std::make_shared<Texture2D>(size, size, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		}

		{
			uint8_t pixel[4] = { 255, 255, 255, 255 };
			_squareTexture = std::make_shared<Texture2D>(1, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		}
	}

	void TouchControls::Render(SpriteBatch* batch, const Font* font)
	{
		if (batch == nullptr)
		{
			return;
		}

		EnsureTextures();

		bool showGameplay = _enabled && !_editMode;
		bool showEdit = _editMode;

		if (!showGameplay && !showEdit)
		{
			// Layer disabled: only EDIT stays on screen (as a faint button)
			// so the user can open the editor and switch it back on.
			const auto& edit = _buttons[static_cast<size_t>(ButtonId::Edit)];
			float px, py, pw, ph;
			ToPixels(edit.layout, px, py, pw, ph);
			float diameter = Math::Min(pw, ph);
			batch->Draw(_circleTexture.get(), Vector2f(px + (pw - diameter) * 0.5f, py + (ph - diameter) * 0.5f),
				Vector2f(diameter, diameter), LinearColor(1, 1, 1, 0.25f));

			return;
		}

		// Joystick.
		if (_stick.visible || showEdit)
		{
			float cx = _stick.x * _screenWidth;
			float cy = _stick.y * _screenHeight;
			float radius = _stick.radius * _screenHeight;
			float alpha = _stick.visible ? (_stickActive ? 0.85f : 0.55f) : 0.18f;

			if (showEdit && _editStickSelected)
			{
				alpha = 0.9f;
			}

			batch->Draw(_circleTexture.get(), Vector2f(cx - radius, cy - radius),
				Vector2f(radius * 2.0f, radius * 2.0f), LinearColor(1, 1, 1, alpha));

			float knobR = radius * 0.45f;
			float kx = cx + _stickKnob.x * (radius - knobR);
			float ky = cy + _stickKnob.y * (radius - knobR);
			batch->Draw(_circleTexture.get(), Vector2f(kx - knobR, ky - knobR),
				Vector2f(knobR * 2.0f, knobR * 2.0f), LinearColor(1, 1, 1, Math::Min(1.0f, alpha + 0.25f)));

			if (showEdit)
			{
				float handle = 26.0f;
				batch->Draw(_squareTexture.get(), Vector2f(cx + radius * 0.7f - handle * 0.5f, cy + radius * 0.7f - handle * 0.5f),
					Vector2f(handle, handle), LinearColor(0.2f, 0.9f, 1.0f, 0.9f));
			}
		}

		for (const auto& button : _buttons)
		{
			if (!button.layout.visible && !showEdit)
			{
				continue;
			}

			float px, py, pw, ph;
			ToPixels(button.layout, px, py, pw, ph);
			float diameter = Math::Min(pw, ph);
			float dx = px + (pw - diameter) * 0.5f;
			float dy = py + (ph - diameter) * 0.5f;

			bool down = IsButtonDown(button.id);
			bool selected = showEdit && !_editStickSelected && _editSelection == button.id;
			float alpha;
			if (!button.layout.visible)
			{
				alpha = 0.18f;
			}
			else if (down)
			{
				alpha = 0.95f;
			}
			else if (selected)
			{
				alpha = 0.9f;
			}
			else
			{
				alpha = 0.55f;
			}

			LinearColor tint(1, 1, 1, alpha);
			if (selected)
			{
				tint = LinearColor(0.4f, 1.0f, 1.0f, alpha);
			}
			else if (down)
			{
				tint = LinearColor(1.0f, 0.9f, 0.4f, alpha);
			}

			batch->Draw(_circleTexture.get(), Vector2f(dx, dy), Vector2f(diameter, diameter), tint);

			// Label.
			float fontScale = Math::Clamp(diameter / 260.0f, 0.35f, 1.0f);
			std::string label = button.label;
			float labelWidth = static_cast<float>(label.size()) * 11.0f * fontScale;
			Vector2f labelPos(px + (pw - labelWidth) * 0.5f, py + ph * 0.5f - 9.0f * fontScale);
			batch->DrawText(font, label, labelPos + Vector2f(2, 2), LinearColor(0, 0, 0, alpha), fontScale);
			batch->DrawText(font, label, labelPos, LinearColor(1, 1, 1, alpha), fontScale);

			if (showEdit)
			{
				float handle = 26.0f;
				batch->Draw(_squareTexture.get(), Vector2f(px + pw - handle, py + ph - handle),
					Vector2f(handle, handle), LinearColor(0.2f, 0.9f, 1.0f, 0.9f));
			}
		}
	}

	void TouchControls::RenderEditPanel()
	{
		if (!_editMode)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(Lang::T("edit_title")))
		{
			ImGui::TextWrapped("%s", Lang::T("edit_hint"));
			ImGui::Separator();

			bool enabled = _enabled;
			if (ImGui::Checkbox(Lang::T("edit_touch_enabled"), &enabled))
			{
				SetEnabled(enabled);
			}

			ImGui::Separator();

			if (ImGui::TreeNode(Lang::T("edit_stick")))
			{
				ImGui::Checkbox(Lang::T("edit_visible"), &_stick.visible);
				ImGui::SliderFloat(Lang::T("edit_size"), &_stick.radius, 0.05f, 0.35f);

				if (ImGui::Button("..."))
				{
					_editStickSelected = true;
				}

				ImGui::TreePop();
			}

			for (auto& button : _buttons)
			{
				if (button.id == ButtonId::Edit)
				{
					continue;
				}

				if (ImGui::TreeNode(button.label))
				{
					ImGui::Checkbox(Lang::T("edit_visible"), &button.layout.visible);

					float sizeW = button.layout.w;
					float sizeH = button.layout.h;
					if (ImGui::SliderFloat("W", &sizeW, 0.03f, 0.6f))
					{
						button.layout.w = sizeW;
					}
					if (ImGui::SliderFloat("H", &sizeH, 0.03f, 0.6f))
					{
						button.layout.h = sizeH;
					}

					ImGui::TreePop();
				}

				if (!_editStickSelected && _editSelection == button.id)
				{
					ImGui::SameLine();
					ImGui::TextUnformatted(" <");
				}
			}

			ImGui::Separator();

			if (ImGui::Button(Lang::T("edit_reset")))
			{
				ResetLayout();
				SaveLayout();
			}

			ImGui::SameLine();

			if (ImGui::Button(Lang::T("edit_done")))
			{
				SetEditMode(false);
			}
		}

		ImGui::End();
	}

	void TouchControls::LoadLayout()
	{
		std::error_code ec;
		auto path = Paths::UserDir() / "touch_layout.json";
		if (!std::filesystem::exists(path, ec))
		{
			return;
		}

		try
		{
			std::ifstream stream(path);
			json j;
			stream >> j;

			if (j.contains("stick") && j["stick"].is_object())
			{
				const auto& s = j["stick"];
				if (s.contains("x") && s["x"].is_number()) _stick.x = s["x"].get<float>();
				if (s.contains("y") && s["y"].is_number()) _stick.y = s["y"].get<float>();
				if (s.contains("radius") && s["radius"].is_number())
				{
					_stick.radius = Math::Clamp(s["radius"].get<float>(), 0.05f, 0.35f);
				}
				if (s.contains("visible") && s["visible"].is_boolean()) _stick.visible = s["visible"].get<bool>();
			}

			if (j.contains("buttons") && j["buttons"].is_object())
			{
				for (auto& button : _buttons)
				{
					if (!j["buttons"].contains(button.name))
					{
						continue;
					}

					const auto& b = j["buttons"][button.name];
					if (!b.is_object())
					{
						continue;
					}

					if (b.contains("x") && b["x"].is_number()) button.layout.x = b["x"].get<float>();
					if (b.contains("y") && b["y"].is_number()) button.layout.y = b["y"].get<float>();
					if (b.contains("w") && b["w"].is_number())
					{
						button.layout.w = Math::Clamp(b["w"].get<float>(), 0.03f, 0.6f);
					}
					if (b.contains("h") && b["h"].is_number())
					{
						button.layout.h = Math::Clamp(b["h"].get<float>(), 0.03f, 0.6f);
					}
					if (b.contains("visible") && b["visible"].is_boolean())
					{
						button.layout.visible = b["visible"].get<bool>();
					}
				}
			}
		}
		catch (...)
		{
		}
	}

	void TouchControls::SaveLayout()
	{
		try
		{
			json j;
			j["stick"] = { { "x", _stick.x }, { "y", _stick.y }, { "radius", _stick.radius }, { "visible", _stick.visible } };

			for (const auto& button : _buttons)
			{
				j["buttons"][button.name] =
				{
					{ "x", button.layout.x },
					{ "y", button.layout.y },
					{ "w", button.layout.w },
					{ "h", button.layout.h },
					{ "visible", button.layout.visible }
				};
			}

			std::error_code ec;
			auto path = Paths::UserDir() / "touch_layout.json";
			std::filesystem::create_directories(path.parent_path(), ec);

			std::ofstream stream(path, std::ios::trunc);
			stream << j.dump(2);
		}
		catch (...)
		{
		}
	}
}
