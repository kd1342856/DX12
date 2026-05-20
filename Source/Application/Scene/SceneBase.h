#pragma once

// =============================================
// SceneBase
// シーンの基底クラス
// =============================================
class SceneBase
{
public:
virtual ~SceneBase() = default;

virtual void Init() = 0;
virtual void Update() = 0;
};