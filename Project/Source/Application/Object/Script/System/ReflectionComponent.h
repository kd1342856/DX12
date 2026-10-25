#pragma once

#include "../../../../Framework/ECS/Components/Data/NativeScript.h"

class ReflectionComponent : public NativeScript {
public:
    void Serialize(nlohmann::json& out) const override;
    void Deserialize(const nlohmann::json& in) override;
    void ImGuiUpdate() override;
    void Update(float deltaTime) override;
    void PreDraw() override;

    bool IsActive() const { return m_isActive; }

    Math::Vector3 m_planeNormal = { 0.0f, 0.0f, 1.0f };
    Math::Vector3 m_planePoint = { 0.0f, 0.0f, 0.0f };

    // Calculated World Plane
    Math::Vector3 m_worldPlaneNormal = { 0.0f, 0.0f, 1.0f };
    Math::Vector3 m_worldPlanePoint = { 0.0f, 0.0f, 0.0f };

    // エディタ上で球の範囲を示すデバッグ描画用
    float m_debugSize = 2.0f;

    // 対応するRoomArea(GameObject名)。空なら常にアクティブ(従来通りの挙動)。
    // 設定するとプレイヤーがそのRoomArea内にいる時だけ反射を有効化する。
    std::string m_roomName; // unused (kept for old scene data load compatibility)

    // Reflection is active whenever the player is within this distance of the mirror plane,
    // instead of the old "player is inside this named RoomArea" check - a window sitting right at
    // a room's boundary line meant standing normally in front of it never counted as "inside",
    // so the reflection never activated at realistic viewing distance.
    float m_activationDistance = 4.0f;

    // trueなら、反射カメラを自動計算(プレイヤー/エディタカメラを鏡面で反射)する代わりに、
    // m_fixedCameraSourceUUIDで明示的に指定したカメラの映像をそのままこの鏡に映す
    // (監視カメラ/モニターのような使い方)。
    bool m_useFixedCameraSource = false;

    // 反射ソースとして使うカメラのGameObjectを、Hierarchyからドラッグ&ドロップで明示的に
    // 指定する(以前のような「CameraData.m_isDebugPreviewが立っているものを検索して使う」
    // 曖昧な方式だと、削除し損ねた孤児コンポーネントが誤って優先される事故が起きたため)。
    // UUIDだけがシリアライズ対象で、実体へのポインタは毎フレームResolveFixedCameraSource()で
    // 都度解決する(GameObjectの生存を跨いだ古いポインタを持ち続けないため)。
    uint64_t m_fixedCameraSourceUUID = 0;

    // m_fixedCameraSourceUUIDから実際のGameObjectを解決する。見つからなければnullptr。
    class GameObject* ResolveFixedCameraSource() const;

private:
    bool m_isActive = true;
};
