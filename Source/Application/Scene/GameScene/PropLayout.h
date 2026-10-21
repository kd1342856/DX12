#pragma once
#include <memory>
#include <string>
#include <vector>

class Scene;
class GameObject;

// 家の小物(鍋・やかん・ちゃぶ台・仏壇など)の配置データ Asset/Model/Map/Props/layout.json を読み込み、
// シーンにランタイム生成する。layout.json が配置の正なので、シーンJSONには書き込まない
// (生成したGameObjectはDynamic扱いにしてあり、エディタでシーンを保存しても含まれない)。
//
// layout.json は glTF 空間(右手系・+Y上)。エンジンはモデル読み込み時に z を反転した左手系
// (aiProcess_ConvertToLeftHanded)なので、家のモデル House1.gltf と同じく z を反転して置く。
//
// 1インスタンス = ルート(TransformData + ModelRenderData) + 子"Collider"(静的Box)。
// 物理エンジンがまだ無いので physics.kind(throwable/dynamic/static)に関わらず全て静的な当たり判定にする。
// コライダーを子に分けているのは、プレイヤーが扉を開け閉めした時にモデル側エンティティの
// コライダーを動的に切り替える処理(Player::TryInteractDoor)の対象にさせないため。
namespace PropLayout
{
    constexpr const char* kLayoutPath = "Asset/Model/Map/Props/layout.json";

    struct SpawnedProp
    {
        std::shared_ptr<GameObject> obj;
        std::vector<std::string> initialOpenNodes; // 最初から開いておく扉/引き出しのノード名
    };

    // layout.json の全インスタンスを生成する。Scene::Init()(Awake/Start)より前に呼ぶ想定。
    std::vector<SpawnedProp> Spawn(Scene& scene, const std::string& layoutPath = kLayoutPath);

    // initial_open を反映する。モデルの読み込み完了後(アニメーション一覧が取れる状態)に呼ぶこと。
    void ApplyInitialOpen(const std::vector<SpawnedProp>& props);
}
