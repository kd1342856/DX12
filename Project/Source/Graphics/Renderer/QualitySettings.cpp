#include "../../Pch.h"
#include "QualitySettings.h"
#include "Renderer.h"
#include "../GPUResource/RenderTarget/RenderTarget.h"
#include "../Shader/ShaderManager/ShaderManager.h"
#include "../Shadow/LocalShadowManager.h"

const QualityPreset& QualitySettings::GetPreset(QualityLevel level)
{
    // 平面反射は、反射プローブを入れたので「高」では1枚・512に絞り、3枚・1024は「最高」だけにする
    static const QualityPreset kPresets[] = {
        // renderScale, 平行光の影, スポットの影, キューブの影, SSAO, SSR, 平面反射の数, 平面反射の解像度, ブルームの回数, レイトレースの反射
        { 0.75f, 1024, 512, 256, false, false, 0, 512, 2, false },   // 低
        { 1.0f, 2048, 1024, 512, true, false, 1, 512, 3, false },    // 中
        { 1.0f, 2048, 1024, 512, true, true, 1, 512, 3, false },     // 高
        { 1.0f, 4096, 2048, 1024, true, true, 3, 1024, 4, true },    // 最高
    };
    return kPresets[static_cast<int>(level)];
}

void QualitySettings::Request(QualityLevel level)
{
    s_requestedLevel = level;
    s_isRequested = true;
}

int QualitySettings::ToRenderSize(UINT windowSize)
{
    return (std::max)(1, static_cast<int>(static_cast<float>(windowSize) * GetRenderScale() + 0.5f));
}

void QualitySettings::RequestRenderScaleOverride(float scale)
{
    s_requestedRenderScaleOverride = (std::max)(scale, 0.0f);
    s_isRenderScaleRequested = true;
}

bool QualitySettings::ApplyPending()
{
    // 描画解像度の倍率だけが変わったときは、画面の大きさのRTを作り直すだけ(呼び出し側で行う)
    bool isRenderScaleChanged = false;
    if (s_isRenderScaleRequested)
    {
        s_isRenderScaleRequested = false;
        isRenderScaleChanged = (s_renderScaleOverride != s_requestedRenderScaleOverride);
        s_renderScaleOverride = s_requestedRenderScaleOverride;
        if (isRenderScaleChanged)
        {
            // GPUが古いRTを使い終わるまで待つ
            GraphicsDevice::Instance().GetQueueManager()->GetGraphicsQueue()->Flush();
        }
    }
    if (!s_isRequested)
    {
        return isRenderScaleChanged;
    }
    s_isRequested = false;
    s_level = s_requestedLevel;
    const QualityPreset& preset = GetCurrent();

    // GPUが古いテクスチャを使い終わるまで待ってから作り直す
    auto& device = GraphicsDevice::Instance();
    device.GetQueueManager()->GetGraphicsQueue()->Flush();

    const float directionalResolution = static_cast<float>(preset.directionalShadowResolution);
    device.GetShadowMap()->Resize(Math::Vector2(directionalResolution, directionalResolution));
    device.GetSpotShadowMaps()->Resize(preset.spotShadowResolution);
    device.GetCubeShadowMaps()->Resize(preset.cubeShadowResolution);
    LocalShadowManager::Instance().InvalidateAll();

    for (int slot = 0; slot < 3; ++slot)
    {
        Renderer::GetPlanarReflectionRenderTarget(slot)->Resize(static_cast<int>(preset.planarReflectionResolution), static_cast<int>(preset.planarReflectionResolution));
    }

    auto& ssao = ShaderManager::Instance().GetSSAOSettings();
    ssao.EnableSSAO = preset.enableSSAO;
    auto& renderer = ShaderManager::Instance().GetRendererSettings();
    renderer.EnableSSR = preset.enableSSR;
    renderer.IsDirty = true;
    auto& postProcess = ShaderManager::Instance().GetPostProcessSettings();
    postProcess.BloomIterations = preset.bloomIterations;
    postProcess.IsDirty = true;
    return true;
}
