#pragma once
#include "BenchmarkPath.h"
#include "../../Framework/DirectX/Utility/Profiler.h"
#include "../../Graphics/Renderer/QualitySettings.h"
#include "../../Graphics/Device/GraphicsDevice.h"

struct TransformData;

//=============================
//
// ベンチマークモード
//
//   家の中を決まったカメラの経路(BenchmarkPath)で回り、フレーム時間を記録して CSV に出す。
//   目標は、画質 High で平均100fps以上、1% low 60fps以上。
//   回っている間は、ゲームの進行(スクリプト・アニメーション・パーティクル)を止め、
//   入力もゲームに渡さない。経路の点で指定した区間だけハント中の状態(フォグと暗さ)にする。
//   始め方は、エディターの「Benchmark」ウィンドウのボタンか、起動引数 -benchmark
//   (読み込みを待ってから始め、終わったらアプリを終了する)。
//   結果は Saved/Benchmark/<日時>/ に、summary・segments・passes・frames の4つの CSV で出す。
//
//=============================
class BenchmarkRunner
{
public:
    static BenchmarkRunner& Instance()
    {
        static BenchmarkRunner instance;
        return instance;
    }

    // 起動引数に -benchmark があるか
    static bool IsRequestedByCommandLine();

    // 経路を読み込んでベンチマークを始める。quitWhenDone なら終わったらアプリを終了する
    void Start(bool quitWhenDone);
    // 途中でやめる(結果は出さない)。pCamera は始める前の位置に戻す
    void Cancel(TransformData* pCamera);

    // カメラを動かしていて、ゲームを止めている間(読み込み待ち・ウォームアップ・計測)
    bool IsActive() const { return m_phase != Phase::Idle; }

    // 毎フレーム、ゲームの更新の代わりに呼ぶ。カメラの位置と向きを pCamera に書く
    void Update(float deltaTime, TransformData* pCamera);

    // 回っている間の画面の隅の表示(進み具合)
    void DrawOverlay() const;
    // エディターの「Benchmark」ウィンドウ(経路の編集と開始)。pEditorCamera は自由カメラ
    void DrawEditorWindow(TransformData* pEditorCamera);

private:
    enum class Phase
    {
        Idle,
        WaitForLoading,  // ジョブとNavMeshの構築が終わるのを待つ
        Warmup,          // 最初の点で止まって待つ(計測しない)
        Running,         // 経路を回りながら計測する
    };

    // 区間名ごとのGPU・CPU時間の合計
    struct PassAccumulator
    {
        const char* pName = nullptr;
        double gpuSumMs = 0.0;
        float gpuMaxMs = 0.0f;
        int gpuFrames = 0;
        double cpuSumMs = 0.0;
        float cpuMaxMs = 0.0f;
        int cpuFrames = 0;
    };

    // 1フレームぶんの記録
    struct FrameRecord
    {
        float time;        // 計測を始めてからの秒
        float frameMs;     // フレーム全体の時間
        float gpuFrameMs;  // GPUの時間(数フレーム遅れて読み戻した値)
        int segmentIndex;
        bool hunt;
    };

    // 計測の結果(ウィンドウに出す)
    struct Result
    {
        bool isValid = false;
        Profiler::FrameStats stats;
        float gpuFrameAvgMs = 0.0f;
        std::string outputDir;
    };

    BenchmarkRunner() = default;

    void BeginRun();
    void FinishRun(TransformData* pCamera);
    void RestoreSettings(TransformData* pCamera);
    void SetHunt(bool hunt);
    void RecordFrame(int segmentIndex);
    PassAccumulator& FindOrAddPass(const char* pName);
    bool WriteResults(const std::string& outputDir);

    BenchmarkPath m_path;
    std::string m_pathFile = BenchmarkPath::kDefaultPath;
    Phase m_phase = Phase::Idle;
    bool m_quitWhenDone = false;
    float m_phaseTime = 0.0f;   // 今の段階に入ってからの秒
    int m_idleFrameCount = 0;   // 読み込み待ちで、ジョブが空のフレームが続いた数
    bool m_isHuntActive = false;

    // 始める前の設定(終わったら戻す)
    QualityLevel m_savedQuality = QualityLevel::High;
    float m_savedRenderScaleOverride = 0.0f;
    GraphicsDevice::FrameRateMode m_savedFrameRateMode = GraphicsDevice::FrameRateMode::Unlimited;
    Math::Vector3 m_savedCameraPosition;
    Math::Vector3 m_savedCameraRotation;
    bool m_hasSavedCamera = false;

    std::vector<FrameRecord> m_frames;
    std::vector<PassAccumulator> m_passes;
    Result m_lastResult;

    // エディターのウィンドウで選んでいる点
    int m_selectedPointIndex = -1;
};
