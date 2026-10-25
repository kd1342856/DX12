#pragma once

class SpriteRenderSystem : public SystemBase
{
public:
    void Init()
    {
    }
    void Update(float deltaTime) override;
    void Render();

    // マウスカーソルがスプライトの描画矩形内にあるか(Render()と同じ1280x720の仮想画面座標で判定)。
    // ImGuiのウィンドウ(ヒエラルキー等)の上にカーソルがある場合はfalse。
    bool IsMouseOver(Entity entity) const;
    // 左クリックした瞬間にカーソルがスプライト上にあるか
    bool IsClicked(Entity entity) const;
};
