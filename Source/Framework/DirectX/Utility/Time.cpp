#include "Time.h"

GameTimer::GameTimer()
{
	// 初回のタイムスタンプを記録
	m_prevTime = std::chrono::high_resolution_clock::now();
}

void GameTimer::Update()
{
	auto currentTime = std::chrono::high_resolution_clock::now();

	// 前フレームからの経過時間をマイクロ秒単位で計算して秒に変換
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_prevTime);
	m_deltaTime = elapsed.count() / 1000000.0f;

	// デルタタイムの上限を設定(デバッグ停止やフレーム落ち対策)
	if (m_deltaTime > MAX_DELTA_TIME)
	{
		m_deltaTime = MAX_DELTA_TIME;
	}

	m_prevTime = currentTime;

	// FPS計測(1秒ごとに更新)
	m_frameCount++;
	m_fpsElapsed += m_deltaTime;

	if (m_fpsElapsed >= 1.0f)
	{
		m_fps = static_cast<float>(m_frameCount) / m_fpsElapsed;
		m_frameCount = 0;
		m_fpsElapsed = 0.0f;
	}
}

void GameTimer::UpdateWindowTitle(HWND hWnd, const std::wstring& baseName)
{
	// ウィンドウタイトルにFPSを表示
	wchar_t titleBuffer[256];
	swprintf_s(titleBuffer, L"%s  [ FPS: %.1f ]", baseName.c_str(), m_fps);
	SetWindowTextW(hWnd, titleBuffer);
}