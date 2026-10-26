#pragma once
#include <functional>
#include <string>
#include <vector>
#include <d3d12.h>

//=============================
//
// レンダーグラフ(1フレームの描画の組み立て)
//
//   1フレームの描画を、名前の付いたパスの並びとして先に組み立ててから、まとめて実行する。
//   パスごとに、使う資源とその状態を宣言しておくと、実行の直前に状態遷移を入れる
//   (パスの中で遷移させている資源は書かなくてよい)。パスの名前でCPUとGPUの時間を計る。
//   ゲームとエディターの描画の違い(デバッグの線・エディターの画面)を、同じ組み立ての中の条件として書けるようにし、
//   2つの関数に同じ手順を写して片方だけ直す、ということが起きないようにする。
//
//=============================
class RenderGraph
{
public:
    struct ResourceUse
    {
        ID3D12Resource* pResource = nullptr;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    };

    // 時間の計り方。中で自分で計っている処理(深度プリパスなど)は、名前が重ならないよう計らない
    enum class Profiling
    {
        CpuAndGpu,
        None,
    };

    // パスを足す。usesは、実行の直前に遷移させておく資源と状態
    void AddPass(const std::string& name, std::function<void()> execute, std::vector<ResourceUse> uses = {}, Profiling profiling = Profiling::CpuAndGpu);

    // 足した順に実行する。実行したら空になる(毎フレーム組み立て直す)
    void Execute();

    // 直前に実行したパスの名前(確認用)
    const std::vector<std::string>& GetExecutedPassNames() const { return m_executedPassNames; }

private:
    struct Pass
    {
        std::string name;
        std::function<void()> execute;
        std::vector<ResourceUse> uses;
        Profiling profiling = Profiling::CpuAndGpu;
    };

    std::vector<Pass> m_passes;
    std::vector<std::string> m_executedPassNames;
};
