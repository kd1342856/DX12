#pragma once

#include <chrono>
#include <string>
#include <windows.h>

class GameTimer
{
public:
	static GameTimer& Instance();

	void Update();

	float DeltaTime() const { return m_deltaTime; }
	float GetFPS() const { return m_fps; }

	void UpdateWindowTitle(HWND hWnd, const std::wstring& baseName);

private:
	GameTimer();
	~GameTimer() = default;
	GameTimer(const GameTimer&) = delete;
	GameTimer& operator=(const GameTimer&) = delete;

	std::chrono::high_resolution_clock::time_point m_prevTime;

	float m_deltaTime = 0.0f;

	float m_fps = 0.0f;
	int m_frameCount = 0;
	float m_fpsElapsed = 0.0f;

	static constexpr float MAX_DELTA_TIME = 0.1f;
};
