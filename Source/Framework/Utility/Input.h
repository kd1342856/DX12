#pragma once

class Input
{
public:
	void Init(HWND hWnd);
    void ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam);
	void Update();

	bool IsKeyHold(int key) const { return (m_keyState[key] & 0x80) != 0; }
	bool IsKeyTrigger(int key) const { return (m_keyState[key] & 0x80) != 0 && (m_prevKeyState[key] & 0x80) == 0; }
	bool IsKeyRelease(int key) const { return (m_keyState[key] & 0x80) == 0 && (m_prevKeyState[key] & 0x80) != 0; }

	bool IsMouseLeftHold() const { return IsKeyHold(VK_LBUTTON); }
	bool IsMouseLeftTrigger() const { return IsKeyTrigger(VK_LBUTTON); }
	bool IsMouseLeftRelease() const { return IsKeyRelease(VK_LBUTTON); }

	bool IsMouseRightHold() const { return IsKeyHold(VK_RBUTTON); }
	bool IsMouseRightTrigger() const { return IsKeyTrigger(VK_RBUTTON); }
	bool IsMouseRightRelease() const { return IsKeyRelease(VK_RBUTTON); }

	int GetMouseX() const { return m_mouseX; }
	int GetMouseY() const { return m_mouseY; }
	int GetMouseDeltaX() const { return m_mouseDeltaX; }
	int GetMouseDeltaY() const { return m_mouseDeltaY; }

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
	HWND m_hWnd = nullptr;
};