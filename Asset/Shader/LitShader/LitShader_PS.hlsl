#include "LitShader.hlsli"

//-------------------------------
// ピクセルシェーダ
//-------------------------------

// ピクセルシェーダ
PSOutput PS(VSOutput In) : SV_Target0
{
    PSOutput Out = (PSOutput) 0; //
    
    // カメラへの方向
    float3 vCam = g_CamPos - In.wPos;
    float camDist = length(vCam); // カメラ - ピクセル距離
    vCam = normalize(vCam);
    
    // 法線
    float3x3 mTBN =
    {
        normalize(In.wT),
        normalize(In.wB),
        normalize(In.wN)
    };
    
    float3 wN = g_normalMap_normal.Sample(g_ss_aniso_wrap, In.UV).rgb;
    wN = wN * 2.0 - 1.0;
    wN = mul(wN, mTBN);
    wN = normalize(wN);

    float NdotV = saturate(dot(wN, vCam));
    
    //------------------------------------------
    // 材質色
    //------------------------------------------

    // Specular coefficiant - 非金属固定反射
    static const float3 kSpecularCoefficient = float3(0.04, 0.04, 0.04);

    float4 mr = g_metallicSmoothnesMap_white.Sample(g_ss_aniso_wrap, In.UV);
    // 金属性
    float metallic = mr.b * g_Metallic;
    // 粗さ
    float roughness = 1.0 - mr.g * g_Smoothness;
    // ラフネスの2乗したほうが、感覚的にリニアに見えるらしい
    const float roughness2 = roughness * roughness;

    // 材質の色
    float4 baseColor = g_baseMap_white.Sample(g_ss_aniso_wrap, In.UV) * g_BaseColor;

    // 材質の拡散色　非金属ほど材質の色になり、金属ほど拡散色は無くなる
    const float3 baseDiffuse = lerp(baseColor.rgb * (1.0 - kSpecularCoefficient), float3(0, 0, 0), metallic);
    // 材質の反射　非金属ほど光の色をそのまま反射、金属ほど材質の色が乗る
    const float3 baseSpecular = lerp(kSpecularCoefficient, baseColor.rgb, metallic);

    // 最終的な色
    float3 color = 0;
    
    //------------------
    // 平行光
    //------------------
    {
        // Half vector
        const float3 H = normalize(-g_DL_Dir + vCam);

        const float NdotL = saturate(dot(wN, -g_DL_Dir));
        const float LdotH = saturate(dot(-g_DL_Dir, H));
        const float NdotH = saturate(dot(wN, H));

        // 影
        float shadow = 1;
        if (g_DL_ShadowPower > 0)
        {
            // カスケード選択
            uint cascadeIndex = 0;
            if (camDist > g_DL_CascadeSplits[0]) cascadeIndex = 1;
            if (camDist > g_DL_CascadeSplits[1]) cascadeIndex = 2;

            float4 liPos = mul(float4(In.wPos, 1), g_DL_mLightVP[cascadeIndex]);
            liPos.xyz /= liPos.w;

            // 投影座標の範囲内か判定
            if (abs(liPos.x) <= 1 && abs(liPos.y) <= 1 && liPos.z <= 1)
            {
                // 投影座標-> UV座標へ変換
                float2 uv = liPos.xy * float2(1, -1) * 0.5 + 0.5;

                // ライトカメラからの距離を、バイアス引いた値と比較しシャドウアクネ対策
                float bias = g_DL_DirLightShadowBias * tan(acos(NdotL));
                bias = clamp(bias, 0, 0.0001); // シャドウアクネ対策用バイアス
                float z = liPos.z - bias; // シャドウアクネ対策

                // 画面のサイズ
                float2 pxSize;
                float elements;
                float levels;
                g_dirLightShadowMap.GetDimensions(0, pxSize.x, pxSize.y, elements, levels);
                
                // ランダム回転
                float noise = InterleavedGradientNoise(In.Pos.xy);
                float s = sin(noise * 6.28318530718);
                float c = cos(noise * 6.28318530718);
                float2x2 rot = float2x2(c, -s, s, c);

                // ポワソンサンプリングで、周辺のピクセルを影判定
                shadow = 0;
                for (int i = 0; i < 16; i++)
                {
                    // 回転させたオフセット
                    float2 offset = mul(g_poissonDisk16[i], rot) / pxSize;
                    shadow += g_dirLightShadowMap.SampleCmpLevelZero(g_ss_comparison, float3(uv + offset, cascadeIndex), z);
                }
                shadow /= 16.0;
                
                // 影の強度
                shadow = lerp(1.0, shadow, g_DL_ShadowPower);
            }
        }
        
        // 拡散光
        float3 diffuse = Diffuse_Burley(baseDiffuse, NdotL, NdotV, LdotH, roughness);
        // 反射光
        float3 specular = Specular_BRDF(roughness2, baseSpecular, NdotV, NdotL, LdotH, NdotH);

        // 光を加算
        color += NdotL * g_DL_Color * (diffuse + specular) * shadow;
    }
    
    //------------------
    // 環境光
    //------------------
    color += g_AmbientLight * baseColor.rgb * baseColor.a;
    
    //------------------
    // エミッシブ
    //------------------
    color += g_emissiveMap_white.Sample(g_ss_aniso_wrap, In.UV).rgb * g_EmissiveColor.rgb;
    
    //------------------
    // IBL
    //------------------
    // 拡散光
    float3 envDiff = g_IBLTex.SampleLevel(g_ss_aniso_wrap, wN, 8).rgb; // 拡散表?するため、低解像度の画像を使用する
    color += envDiff * baseDiffuse.rgb / 3.141592;

    // 反射光
    float3 vRef = reflect(-vCam, wN);
    float3 envSpec = g_IBLTex.SampleLevel(g_ss_aniso_wrap, vRef, roughness * 8).rgb; // 粗いほど低解像度の画像を使用する
    color += envSpec * baseSpecular;

    //------------------------------------------
    // 距離フォグ
    //------------------------------------------
    if (g_DistanceFogDensity > 0)
    {
        float d = camDist * g_DistanceFogDensity;
        
        // 持ちフォグ 1(近い)から0(遠い)
        float f = saturate(1.0 / exp(d * d));
        // 適用
        color = lerp(g_DistanceFogColor, color, f);
    }
    
    //------------------------------------------
    // 出力
    //------------------------------------------
    Out.color = float4(color, baseColor.a);
    Out.normal = float4(wN * 0.5 + 0.5, 1);
    Out.metallicRoughness = float4(metallic, roughness, 0, 1);
    return Out;
}

//-------------------------------
// シャドウマップ生成用 ピクセルシェーダ
//-------------------------------
float4 ShadowCasterPS(ShadowCasterVSOutput In) : SV_Target0
{
    return In.wvpPos.z / In.wvpPos.w;
}
