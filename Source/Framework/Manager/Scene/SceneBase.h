#pragma once

// =============================================
// SceneBase
// シーンの基底クラス
// Load → Init → Update/Draw → Unload の順で呼ばれる
// =============================================
class SceneBase
{
public:
    virtual ~SceneBase() = default;

    virtual void Load()   {}                       // リソース読み込み
    virtual void Unload() {}                       // リソース解放（シーン切替時）
    virtual void Init()   = 0;                     // GameObject 初期化
    virtual void Update(float deltaTime) = 0;      // 毎フレーム更新
    virtual void Draw()   {}                       // Scene 固有の描画（UI等）
};
