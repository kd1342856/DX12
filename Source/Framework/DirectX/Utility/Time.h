#pragma once

// =============================================
// GameTimer
// DeltaTimeとFPS計測を提供するシングルトン
// =============================================
class GameTimer
{
public:
	// シングルトンインスタンス取得
	static GameTimer& Instance()
	{
		static GameTimer instance;
		return instance;
	}

	// フレーム開始時に呼ぶ
	void Update();

	// 前フレームからの経過時間(秒)を取得
	float DeltaTime() const { return m_deltaTime; }

	// 現在のFPSを取得
	float GetFPS() const { return m_fps; }

	// ウィンドウタイトルにFPSを表示する
	void UpdateWindowTitle(HWND hWnd, const std::wstring& baseName);

private:
	GameTimer();
	~GameTimer() = default;
	GameTimer(const GameTimer&) = delete;
	GameTimer& operator=(const GameTimer&) = delete;

	// 高精度タイマー用
	std::chrono::high_resolution_clock::time_point m_prevTime;

	// デルタタイム(秒)
	float m_deltaTime = 0.0f;

	// FPS計測用
	float m_fps = 0.0f;
	int m_frameCount = 0;
	float m_fpsElapsed = 0.0f;

	// デルタタイムの上限(フレーム落ち時のカクつき防止)
	static constexpr float MAX_DELTA_TIME = 0.1f;
};