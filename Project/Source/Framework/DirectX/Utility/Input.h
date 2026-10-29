#pragma once
#include <windows.h>

class Input
{
public:
	void Init(HWND hWnd);
    void ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam);
	void Update();

	// ゲームへの入力を止めている間(SetGameInputBlocked)は、何も押されていない扱いになる
	bool IsKeyHold(int key) const { return !m_isGameInputBlocked && (m_keyState[key] & 0x80) != 0; }
	bool IsKeyTrigger(int key) const { return !m_isGameInputBlocked && (m_keyState[key] & 0x80) != 0 && (m_prevKeyState[key] & 0x80) == 0; }
	bool IsKeyRelease(int key) const { return !m_isGameInputBlocked && (m_keyState[key] & 0x80) == 0 && (m_prevKeyState[key] & 0x80) != 0; }

	bool IsMouseLeftHold() const { return IsKeyHold(VK_LBUTTON); }
	bool IsMouseLeftTrigger() const { return IsKeyTrigger(VK_LBUTTON); }
	bool IsMouseLeftRelease() const { return IsKeyRelease(VK_LBUTTON); }

	bool IsMouseRightHold() const { return IsKeyHold(VK_RBUTTON); }
	bool IsMouseRightTrigger() const { return IsKeyTrigger(VK_RBUTTON); }
	bool IsMouseRightRelease() const { return IsKeyRelease(VK_RBUTTON); }

	int GetMouseX() const { return m_mouseX; }
	int GetMouseY() const { return m_mouseY; }
	HWND GetHWnd() const { return m_hWnd; }
	int GetMouseDeltaX() const { return m_isGameInputBlocked ? 0 : m_mouseDeltaX; }
	int GetMouseDeltaY() const { return m_isGameInputBlocked ? 0 : m_mouseDeltaY; }

	// ゲームのスクリプトへ入力を渡さない(エディターの自由カメラ中に、プレイヤーが一緒に動かないように)。
	// GameScene が GameManager::Update を回す間だけ立てる
	void SetGameInputBlocked(bool isBlocked) { m_isGameInputBlocked = isBlocked; }
	// ウィンドウが選択されているか(選択されていない間は、キーもマウスも受け取らない)
	bool IsWindowActive() const { return m_isWindowActive; }

	void SetMouseModeRelative() { 
        if (!m_isRelative) {
            ShowCursor(FALSE);
            m_isRelative = true; 
        }
    }
	void SetMouseModeAbsolute() { 
        if (m_isRelative) {
            ShowCursor(TRUE);
            m_isRelative = false; 
        }
    }

	static Input& Instance();

private:
	Input() {}
	~Input() {}

	BYTE m_keyState[256] = {};
	BYTE m_prevKeyState[256] = {};

	int m_mouseX = 0;
	int m_mouseY = 0;
	int m_prevMouseX = 0;
	int m_prevMouseY = 0;
	int m_mouseDeltaX = 0;
	int m_mouseDeltaY = 0;

	bool m_isRelative = false;
    bool m_wasRelative = false;
	bool m_isGameInputBlocked = false;
	bool m_isWindowActive = false;
	HWND m_hWnd = nullptr;
};