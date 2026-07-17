#include "LitShader.hlsli"

//-------------------------------
// ピクセルシェーダ
//-------------------------------

// ピクセルシェーダ
PSOutput main(VSOutput In) : SV_Target0
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
    if (dot(wN, wN) < 0.0001) wN = float3(0, 1, 0);
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
            // 簡易シャドウ（カスケードなし）
            uint cascadeIndex = 0;
            float4 liPos = mul(float4(In.wPos, 1), g_DL_mLightVP[0]);
            liPos.xyz /= liPos.w;

            // 投影座標の範囲内か判定
            if (abs(liPos.x) <= 1 && abs(liPos.y) <= 1 && liPos.z <= 1)
            {
                // 投影座標-> UV座標へ変換
                float2 uv = liPos.xy * float2(1, -1) * 0.5 + 0.5;

                // ライトカメラからの距離を、バイアス引いた値と比較しシャドウアクネ対策
                float bias = g_DL_DirLightShadowBias;
                float z = liPos.z - bias; // シャドウアクネ対策

                // 画面のサイズ
                float2 pxSize;
                float levels;
                g_dirLightShadowMap.GetDimensions(0, pxSize.x, pxSize.y, levels);
                pxSize.x = max(pxSize.x, 1.0);
                pxSize.y = max(pxSize.y, 1.0);
                
                // ランダム回転
                float noise = InterleavedGradientNoise(In.Pos.xy);
                float s = sin(noise * 6.28318530718);
                float c = cos(noise * 6.28318530718);
                float2x2 rot = float2x2(c, -s, s, c);

                // PCF計算 (16回は重すぎるため4回に軽量化)
                shadow = 0;
                for (int i = 0; i < 4; i++)
                {
                    float2 offset = mul(g_poissonDisk16[i], rot) / pxSize;
                    shadow += g_dirLightShadowMap.SampleCmpLevelZero(g_ss_comparison, uv + offset, z);
                }
                shadow /= 4.0;
                
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
    
    //------------------
    // Spot Lights
    //------------------
    for (int i = 0; i < g_SL_Count; i++)
    {
        float3 L = g_SL[i].Pos - In.wPos;
        float d = length(L);
        L = L / d;
        
        float attenuation = saturate(1.0 - (d / g_SL[i].Range));
        attenuation *= attenuation;
        
        // Spotlight cone
        float cosAngle = dot(-L, normalize(g_SL[i].Dir));
        float coneAtt = saturate((cosAngle - g_SL[i].OuterCorn) / (g_SL[i].InnerCorn - g_SL[i].OuterCorn));
        // SpotShadow未使用のため常に1（影なし）
        float spotShadow = 1;
        
        float NdotL_SL = saturate(dot(wN, L));
        float3 H_SL = normalize(L + vCam);
        float LdotH_SL = saturate(dot(L, H_SL));
        float NdotH_SL = saturate(dot(wN, H_SL));
        
        float3 diff_SL = Diffuse_Burley(baseDiffuse, NdotL_SL, NdotV, LdotH_SL, roughness);
        float3 spec_SL = Specular_BRDF(roughness2, baseSpecular, NdotV, NdotL_SL, LdotH_SL, NdotH_SL);
        
        color += NdotL_SL * g_SL[i].Color * (diff_SL + spec_SL) * attenuation * coneAtt * spotShadow;
    }

    color += g_AmbientLight * baseColor.rgb * baseColor.a;
    
    //------------------
    // エミッシブ
    //------------------
    color += g_emissiveMap_white.Sample(g_ss_aniso_wrap, In.UV).rgb * g_EmissiveColor.rgb;
    
    //------------------
    // IBL (Dummy Env)
    //------------------
    float3 skyColor = float3(0.5, 0.6, 0.8);
    float3 groundColor = float3(0.3, 0.25, 0.2);
    
    // 拡散
    float envWeight = wN.y * 0.5 + 0.5;
    float3 envDiff = lerp(groundColor, skyColor, envWeight) * 1.5;
    color += envDiff * baseDiffuse.rgb / 3.141592;

    // 鏡面
    float3 vRef = reflect(-vCam, wN);
    float specWeight = vRef.y * 0.5 + 0.5;
    float3 envSpec = lerp(groundColor, skyColor, specWeight) * 2.0;
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
