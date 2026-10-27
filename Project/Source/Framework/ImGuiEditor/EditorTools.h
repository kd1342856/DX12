#pragma once
#include <functional>
#include <string>

//=============================
//
// 保存していない変更の警告(仕様書「7. 便利な機能」)
//
//   シーンの切り替え・終了・プリセットの読み込みの前に、保存していないシェーダーの設定があれば
//   「保存する／保存しない／やめる」を聞く。Undoの履歴はシーンを切り替えると消えるので、その前に止める。
//
//=============================
namespace UnsavedChanges
{
    // 起動時に1回呼ぶ。ウィンドウを閉じる前に聞くようにする
    void Initialize(HWND hWnd);
    // 保存していない変更が無ければすぐにactionを呼び、あれば聞いてから呼ぶ。whatは確認の文言に出す操作の名前
    void Confirm(const std::string& what, std::function<void()> action);
    // 毎フレーム、ImGuiのフレームの中で呼ぶ(確認の画面を出す)
    void Update();
}

//=============================
//
// ゲームと同じ見え方への切り替え
//
//   F4を押している間か、シェーダーエディターのボタンで切り替えている間だけ、エディターのカメラでも
//   表示ガンマを外し、ゲームと同じガンマとプレイヤーの明るさで表示する(エディターでは見えたのにゲームでは真っ暗、を防ぐ)。
//
//=============================
namespace GameLookPreview
{
    bool IsToggled();
    void SetToggled(bool isToggled);
    bool IsActive();
    // 毎フレーム、ImGuiのフレームの中で、シーンを描く前に呼ぶ
    void Update();
}
