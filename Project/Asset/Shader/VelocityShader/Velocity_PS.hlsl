#include "inc_Velocity.hlsli"

float2 main(VelocityVSOutput In) : SV_Target0
{
    // アルファテストのマテリアルは、メインの描画と同じところを切り抜く
    if (g_alphaMode == 1)
    {
        float alpha = g_baseMap.Sample(g_ss_linear_wrap, In.UV).a * g_baseColorFactor.a;
        clip(alpha - 0.1f);
    }
    return ClipToUV(In.CurrClip) - ClipToUV(In.PrevClip);
}
