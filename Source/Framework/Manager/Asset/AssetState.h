#pragma once

// ============================================================
// AssetState
// 非同期ロードのアセット状態管理
// 全アセット（Texture, Mesh 等）が共通で使用する
//
// レンダラー側は描画前に IsReady() を確認すること：
//   if (texture->IsReady()) texture->Set(...);
//   else defaultTexture->Set(...);
// ============================================================
enum class AssetState
{
    LoadingCPU,
    WaitingGPU,
    Ready,
    Failed,
};
