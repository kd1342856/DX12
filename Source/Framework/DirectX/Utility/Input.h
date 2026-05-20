#pragma once
#include <Keyboard.h>
#include <Mouse.h>


// =============================================
// Input
// キーボード・マウス入力のユーティリティクラス
// DirectXTK12のKeyboard/Mouseをラップして
// 1行で入力状態を取得できるようにする
// =============================================
class Input
{
public:
	// 初期化（ウィンドウハンドルを渡す）
	void Init(HWND hWnd);

	// 毎フレーム呼ぶ（GDF::BeginFrame内で呼ばれる）
	void Update();

	// Windowメッセージをフックする（Window::WindowProcedureで呼ぶ）
	void ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam);

	// ===========================
	// キーボード
	// ===========================

	// 押しっぱなし
	bool IsKeyHold(DirectX::Keyboard::Keys key) const
	{
		return m_keyboardState.IsKeyDown(key);
	}

	// 押した瞬間（1フレームだけtrue）
	bool IsKeyTrigger(DirectX::Keyboard::Keys key) const
	{
		return m_keyboardTracker.IsKeyPressed(key);
	}

	// 離した瞬間（1フレームだけtrue）
	bool IsKeyRelease(DirectX::Keyboard::Keys key) const
	{
		return m_keyboardTracker.IsKeyReleased(key);
	}

	// ===========================
	// マウス
	// ===========================

	// 左クリック押しっぱなし
	bool IsMouseLeftHold()   const { return m_mouseState.leftButton; }
	// 右クリック押しっぱなし
	bool IsMouseRightHold()  const { return m_mouseState.rightButton; }
	// 中クリック押しっぱなし
	bool IsMouseMiddleHold() const { return m_mouseState.middleButton; }

	// 左クリック押した瞬間
	bool IsMouseLeftTrigger()   const { return m_mouseTracker.leftButton   == DirectX::Mouse::ButtonStateTracker::PRESSED; }
	// 右クリック押した瞬間
	bool IsMouseRightTrigger()  const { return m_mouseTracker.rightButton  == DirectX::Mouse::ButtonStateTracker::PRESSED; }
	// 中クリック押した瞬間
	bool IsMouseMiddleTrigger() const { return m_mouseTracker.middleButton == DirectX::Mouse::ButtonStateTracker::PRESSED; }

	// 左クリック離した瞬間
	bool IsMouseLeftRelease()  const { return m_mouseTracker.leftButton  == DirectX::Mouse::ButtonStateTracker::RELEASED; }
	// 右クリック離した瞬間
	bool IsMouseRightRelease() const { return m_mouseTracker.rightButton == DirectX::Mouse::ButtonStateTracker::RELEASED; }

	// マウス座標取得
	int GetMouseX() const { return m_mouseState.x; }
	int GetMouseY() const { return m_mouseState.y; }

	// マウスホイール
	int GetMouseWheel() const { return m_mouseState.scrollWheelValue; }

	// マウスの移動量（前フレームからの差分）
	int GetMouseDeltaX() const { return m_mouseDeltaX; }
	int GetMouseDeltaY() const { return m_mouseDeltaY; }

	// マウスモード設定
	void SetMouseModeRelative() { m_mouse.SetMode(DirectX::Mouse::MODE_RELATIVE); }
	void SetMouseModeAbsolute() { m_mouse.SetMode(DirectX::Mouse::MODE_ABSOLUTE); }

private:
	// キーボード
	DirectX::Keyboard                            m_keyboard;
	DirectX::Keyboard::State                     m_keyboardState = {};
	DirectX::Keyboard::KeyboardStateTracker      m_keyboardTracker;

	// マウス
	DirectX::Mouse                               m_mouse;
	DirectX::Mouse::State                        m_mouseState = {};
	DirectX::Mouse::ButtonStateTracker           m_mouseTracker;

	// マウス移動量（前フレームとの差分）
	int m_prevMouseX  = 0;
	int m_prevMouseY  = 0;
	int m_mouseDeltaX = 0;
	int m_mouseDeltaY = 0;

	// シングルトン
	Input() {}
	~Input() {}

public:
	static Input& Instance()
	{
		static Input instance;
		return instance;
	}
};