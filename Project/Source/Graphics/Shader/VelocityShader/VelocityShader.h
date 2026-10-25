#pragma once
#include "../GraphicsShader/GraphicsShader.h"

//=============================
//
// 動く物の速度を描くシェーダー
//
//   カメラの動きによる速度(MotionVectors::RenderCameraVelocity)の上に、前のフレームから動いた物
//   (スキニングのモデル、開閉する扉など)だけを描き直して、物の動きを含んだ速度で上書きする。
//   深度はメインの描画のものをLESS_EQUALでテストする(書き込まない)。
//
//=============================
class VelocityShader : public GraphicsShader
{
public:
    // VelocityShader/inc_Velocity.hlsliのcbVelocityFrame(b4)と同じ並び
    struct FrameConstants
    {
        Math::Matrix currentViewProjection;
        Math::Matrix previousViewProjection;
    };

    virtual void Create(GraphicsDevice* pGraphicsDevice) override;

    // スタティックメッシュ(扉など)を描く準備
    void BeginStatic(RenderContext& context, const FrameConstants& frame);
    // スキニングのモデルを描く準備
    void BeginSkinned(RenderContext& context, const FrameConstants& frame);
    // スキニングのモデルの骨(今と前のフレーム)
    void SetBones(const std::vector<Math::Matrix>& bones, const std::vector<Math::Matrix>& previousBones);
    // ノードのワールド行列(今と前のフレーム)
    void SetWorld(const Math::Matrix& world, const Math::Matrix& previousWorld);
    void BeforeDrawMesh(const Mesh& mesh, const Material& material) override;

private:
    void BeginProgram(RenderContext& context, ShaderProgram* pProgram, ID3D12PipelineState* pPipelineState, const FrameConstants& frame);

    ShaderProgram* m_pStaticProgram = nullptr;
    ShaderProgram* m_pSkinnedProgram = nullptr;
    ID3D12PipelineState* m_pStaticPipelineState = nullptr;
    ID3D12PipelineState* m_pSkinnedPipelineState = nullptr;
};
