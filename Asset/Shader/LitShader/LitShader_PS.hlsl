#include "LitShader.hlsli"

//-------------------------------
// ピクセルシェーダ
//-------------------------------

// 簡易版 Value Noise (HLSL)
float Hash21(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float Noise2D(float2 p)
{
    float2 f = frac(p);
    p = floor(p);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0;
    return lerp(lerp(frac(sin(n) * 43758.5453123), frac(sin(n + 1.0) * 43758.5453123), f.x),
                lerp(frac(sin(n + 57.0) * 43758.5453123), frac(sin(n + 58.0) * 43758.5453123), f.x), f.y);
}

float Noise1D(float x) 
{ 
    // 周波数を下げて砂嵐にならないようにする
    return frac(sin(x * 12.71) * 437.585453); 
}

float ComputeCrackMask(float2 localUV, float randBase)
{
    float dist = length(localUV);
    float angle = atan2(localUV.y, localUV.x);
    
    // 角度を不規則に歪ませる（滑らかなNoise2Dを使用し、ピクセル間で値が飛ばないようにする）
    float angleWarp = (Noise2D(float2(angle * 2.0, randBase)) - 0.5) * 2.0;
    float warpedAngle = angle + angleWarp;
    
    // 距離を不規則に歪ませる（滑らかなNoise2Dを使用）
    float distWarp = (Noise2D(float2(angle * 1.5, randBase + 10.0)) - 0.5) * 0.3;
    float warpedDist = dist + distWarp;

    // ① 放射状のひび(衝撃点から外へ)
    float spokes = lerp(40.0, 70.0, randBase);
    float spokeNoise = (Noise2D(float2(warpedDist * 2.0, randBase * 10.0)) - 0.5) * 0.15; 
    float spokeFrac = frac(warpedAngle / (2.0 * 3.14159265) * spokes + spokeNoise);
    float spokeDist = min(spokeFrac, 1.0 - spokeFrac);
    // 綺麗な円形でフェードアウトしないように warpedDist を使う
    float outerFade = smoothstep(0.45, 0.05, warpedDist);
    
    // 穴の形を激しく歪ませる（角度による大きな歪み＋UVによる細かいギザギザ）
    float holeDist = dist 
        + (Noise2D(float2(angle * 3.0, randBase * 5.0)) - 0.5) * 0.06 
        + (Noise2D(localUV * 30.0) - 0.5) * 0.03;
        
    // 穴の縁(holeRadius=0.05付近)から自然にヒビが発生するようにする
    float innerFade = smoothstep(0.04, 0.07, holeDist);
    float radialFade = outerFade * innerFade;
    // 線は少し太めに（0.015）してエイリアシングを防ぐ
    float radialCrack = smoothstep(0.015, 0.002, spokeDist) * radialFade;

    // ② 同心円状のひび(中心付近だけ細かく)
    float rings = lerp(20.0, 40.0, frac(randBase * 13.0));
    float ringNoise = (Noise2D(float2(warpedAngle * 3.0, randBase * 7.0)) - 0.5) * 0.8; 
    float ringFrac = frac(warpedDist * rings - ringNoise);
    float ringDist = min(ringFrac, 1.0 - ringFrac);
    // こちらも線を少し太めに (0.02)
    float ringCrack = smoothstep(0.02, 0.002, ringDist) * smoothstep(0.4, 0.0, warpedDist) * innerFade; 

    // ③ 全体のジャギーノイズ(線を汚す)
    float jitter = Noise2D(localUV * 5.0);
    float crack = saturate(radialCrack + ringCrack);
    crack *= lerp(0.8, 1.0, jitter); 

    return crack;
}

float LinearizeDepth(float depth)
{
    float4 clipSpace = float4(0.0, 0.0, depth, 1.0);
    float4 viewSpace = mul(clipSpace, g_mInvP);
    return viewSpace.z / viewSpace.w;
}

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
    wN = wN * float3(g_normalScale, g_normalScale, 1.0);
    wN = mul(wN, mTBN);
    if (dot(wN, wN) < 0.0001) wN = float3(0, 1, 0);
    wN = normalize(wN);

    float NdotV = saturate(dot(wN, vCam));
    
    //------------------------------------------
    // 材質色
    //------------------------------------------

    // Specular coefficiant - 非金属固定反射
    static const float3 kSpecularCoefficient = float3(0.04, 0.04, 0.04);

    float4 mr = g_metallicRoughnessMap_white.Sample(g_ss_aniso_wrap, In.UV);
    // 金属性 (B channel)
    float metallic = mr.b * g_metallicFactor;
    // 粗さ (G channel)
    float roughness = mr.g * g_roughnessFactor;
    // ラフネスの2乗したほうが、感覚的にリニアに見えるらしい
    const float roughness2 = roughness * roughness;

    // 材質の色
    float4 baseColor = g_baseMap_white.Sample(g_ss_aniso_wrap, In.UV) * g_baseColorFactor;

    // プロシージャルデカール
    if (g_proceduralType == 1) // Blood
    {
        float2 uv = In.UV * 2.0 - 1.0; // -1..1に正規化
        float n1 = Noise2D(uv * 3.0);   // 大きな染みの形
        float n2 = Noise2D(uv * 14.0);  // 細かいにじみ
        float mask = n1 * 0.55 + n2 * 0.45;
        
        float alpha = smoothstep(0.45, 0.62, mask);
        
        // Depth Fade
        float opaqueDepth = g_opaqueDepth.Load(int3(In.Pos.xy, 0)).r;
        float sceneZ = LinearizeDepth(opaqueDepth);
        float pixelZ = LinearizeDepth(In.Pos.z);
        float diffZ = abs(sceneZ - pixelZ);
        float depthFade = saturate(diffZ / 0.5); // 0.5m の範囲でフェード
        alpha *= depthFade;

        float3 bColor = lerp(float3(0.05, 0.0, 0.0), float3(0.28, 0.02, 0.02), mask);
        
        baseColor = float4(bColor, alpha);
        metallic = 0.2;
        roughness = 0.2; // 血痕は少しツヤがある
    }
    else if (g_proceduralType == 2) // Cobweb (とりあえず板ポリ用として処理)
    {
        float2 uv = In.UV * 2.0 - 1.0;
        float r = length(uv);
        float theta = atan2(uv.y, uv.x);
        
        // 放射状の糸
        float spokes = 8.0;
        float spokeFrac = frac(theta / (2.0 * 3.14159265) * spokes);
        float spokeDist = min(spokeFrac, 1.0 - spokeFrac); // 0に近いほど糸の上
        float spokeMask = smoothstep(0.03, 0.0, spokeDist);
        
        // 同心円の糸
        float rings = 4.0;
        float ringFrac = frac(r * rings);
        float ringDist = min(ringFrac, 1.0 - ringFrac);
        float ringMask = smoothstep(0.04, 0.0, ringDist);
        
        float webMask = saturate(spokeMask + ringMask);
        webMask *= smoothstep(1.0, 0.8, r); // 外側をフェードアウト
        webMask *= step(r, 1.0); // 円の外側は完全に切り捨て
        
        baseColor = float4(1.0, 1.0, 1.0, webMask * 0.8);
        metallic = 0.0;
        roughness = 0.8;
    }

    // アルファテスト（0.1未満なら破棄: MASKモードのみ）
    if (g_alphaMode == 1)
    {
        clip(baseColor.a - 0.1f);
    }

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

    // オクルージョン (R channel)
    float occTex = g_occlusionMap_white.Sample(g_ss_aniso_wrap, In.UV).r;
    float occlusion = lerp(1.0, occTex, g_occlusionStrength);
    
    color += g_AmbientLight * baseColor.rgb * baseColor.a * occlusion;
    
    //------------------
    // エミッシブ
    //------------------
    float3 emissiveColor = g_emissiveMap_white.Sample(g_ss_aniso_wrap, In.UV).rgb * g_emissiveFactor;
    color += emissiveColor * g_emissiveStrength;
    
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
    // デバッグビュー切り替え
    //------------------------------------------
    if (g_DebugView != 0) // Final以外の時
    {
        switch(g_DebugView)
        {
            case 1: color = baseColor.rgb; break; // Albedo
            case 2: color = wN * 0.5 + 0.5; break; // Normal (WorldNormalと同じ扱い)
            case 3: color = wN * 0.5 + 0.5; break; // WorldNormal
            case 4: color = roughness.xxx; break; // Roughness
            case 5: color = metallic.xxx; break; // Metallic
            case 6: color = occTex.xxx; break; // AO
            case 7: color = frac(camDist / 10.0).xxx; break; // Depth (簡易可視化)
            case 8: color = frac(In.wPos / 10.0); break; // Position
            case 9: color = emissiveColor * g_emissiveStrength; break; // Emissive
        }
    }

    //------------------------------------------
    // Glass (Refraction & Fresnel)
    //------------------------------------------
    if (g_proceduralType == 3)
    {
        // 窓の大きさに依存しないよう、ワールド座標をグリッド分割してひびを発生させる
        float gridSize = 3.0; // より大きくバリンと
        float3 gridId = floor(In.wPos / gridSize);
        float3 gridLocal = frac(In.wPos / gridSize) - 0.5;
        
        // ランダムな中心位置の生成
        // 境界からはみ出さないように、ズレの最大幅を 0.4 に抑える (±0.2)
        // gridLocal は -0.5 ~ 0.5 なので、端までの最短距離は 0.3 になる
        float3 p3_1 = frac((gridId + 1.0) * 0.1031); p3_1 += dot(p3_1, p3_1.yzx + 33.33);
        float3 p3_2 = frac((gridId + 2.0) * 0.1031); p3_2 += dot(p3_2, p3_2.yzx + 33.33);
        float3 p3_3 = frac((gridId + 3.0) * 0.1031); p3_3 += dot(p3_3, p3_3.yzx + 33.33);
        
        float3 center = float3(frac((p3_1.x + p3_1.y) * p3_1.z) - 0.5, 
                               frac((p3_2.x + p3_2.y) * p3_2.z) - 0.5, 
                               frac((p3_3.x + p3_3.y) * p3_3.z) - 0.5) * 0.4;
                               
        // 中心からの相対ベクトル
        float3 diff = gridLocal - center;
        
        // 法線方向の成分を除去して平面上のベクトルにする
        float3 diffPlane = diff - dot(diff, wN) * wN;
        
        // 接空間の2D座標に変換（In.wT, In.wBを使用）
        float2 crackUV = float2(dot(diffPlane, normalize(In.wT)), dot(diffPlane, normalize(In.wB)));
        
        // スケール調整：1.0に戻す (ComputeCrackMask内で dist=0.4 で完全にフェードアウトする)
        // これでグリッドの境界(0.5)に到達する前にヒビが消滅するため、絶対にスパっと切れない
        crackUV *= 1.0;  
        
        float3 p3_r = frac((gridId + 6.0) * 0.1031); p3_r += dot(p3_r, p3_r.yzx + 33.33);
        float randBase = frac((p3_r.x + p3_r.y) * p3_r.z);

        // --- 物理的な穴を開ける ---
        float holeRadius = 0.05;
        // ComputeCrackMask と全く同じ式で穴の形を激しく歪ませる
        float holeAngle = atan2(crackUV.y, crackUV.x);
        float holeDist = length(crackUV) 
            + (Noise2D(float2(holeAngle * 3.0, randBase * 5.0)) - 0.5) * 0.06 
            + (Noise2D(crackUV * 30.0) - 0.5) * 0.03;
            
        if (holeDist < holeRadius)
        {
            // 穴の中はガラスを描画しない（完全に透けさせる）
            clip(-1);
        }
        
        // ひびマスクの生成
        float crackMask = ComputeCrackMask(crackUV, randBase);
        
        // 法線をひびに沿って微妙にずらし、ハイライトが線を辿るようにする
        float2 crackGrad = float2(
            ComputeCrackMask(crackUV + float2(0.01, 0.0), randBase) - crackMask,
            ComputeCrackMask(crackUV + float2(0.0, 0.01), randBase) - crackMask
        );
        wN = normalize(wN + In.wT * crackGrad.x * 2.5 + In.wB * crackGrad.y * 2.5);
        
        // ひび部分はざらつく(すりガラス化) - 汚れ感のためにラフネスをさらに上げる
        float currentRoughness = lerp(roughness, 0.85, crackMask);

        float IOR = 1.45;
        float F0 = pow((1.0 - IOR) / (1.0 + IOR), 2.0); // approx 0.0337
        // 通常のガラスのF0(約0.03〜0.1)だと、正面から見た時(NdotV≈1)は
        // fresnel≒F0まで下がり、透過色(外の景色)がほぼ全面に出て鏡像がほぼ見えなくなる。
        // 窓を正面から見ても顔が映る「鏡のような窓」にするため、F0を大きく底上げする。
        F0 = max(F0, 0.5);

        float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
        fresnel *= (1.0 - currentRoughness * 0.5);

        float2 screenUV = In.Pos.xy / float2(g_ScreenWidth, g_ScreenHeight);
        
        // ノーマルとひび割れによる屈折オフセット
        float2 refractOffset = wN.xy * 0.05 * (1.0 - currentRoughness * 0.5);
        
        float2 sampleUV = saturate(screenUV + refractOffset);
        float3 refractColor = g_refractionMap.Sample(g_ss_linear_clamp, sampleUV).rgb;
        
        // ひび割れ部分は白濁する（ガラスの断面）
        float glassOpacity = currentRoughness * 0.8 + 0.15;
        
        // 光の散乱で白っぽく見せる
        float3 glassColor = lerp(baseColor.rgb, float3(0.9, 0.92, 0.95), crackMask * 0.6);
        
        // 透過色: 背景にガラス色を掛けたものと、ガラス自体のDiffuse色をブレンド
        float3 transColor = lerp(refractColor * glassColor, glassColor, glassOpacity);
        
        // 平面反射色の取得
        // メインカメラのスクリーンUV(sampleUV)は別カメラ(反射カメラ)で描いた
        // g_planarReflectionMapとは対応しないため使えない。
        // このピクセルのワールド座標(In.wPos)を、反射カメラのView*Proj行列で
        // 直接再投影して、正しいUVを求める。
        float3 reflectionColor = float3(0.0, 0.0, 0.0);
        if (g_HasReflection != 0)
        {
            float4 reflClip = mul(float4(In.wPos, 1.0), g_mReflectionVP);
            if (reflClip.w > 0.0001)
            {
                float2 reflNdc = reflClip.xy / reflClip.w;
                float2 reflectionUV = reflNdc * float2(0.5, -0.5) + 0.5;
                reflectionColor = g_planarReflectionMap.Sample(g_ss_linear_clamp, saturate(reflectionUV)).rgb;
            }
        }
        
        // 反射色と環境マップベースカラーをブレンド（環境マップはぼんやり全体に、平面反射はくっきり）
        // 完全な黒（反射対象がない部分）を避けるため
        float3 finalReflection = lerp(color, reflectionColor, 0.8);
        
        // 透過色と反射色をフレネルでブレンド
        color = lerp(transColor, finalReflection, fresnel);
        baseColor.a = 1.0;
    }

    // HDR ToneMapping and Gamma Correction are now handled in PostProcessShader.
    // We output linear HDR colors directly.
    
    //------------------------------------------
    // 出力
    //------------------------------------------
    Out.color = float4(color, baseColor.a);
    return Out;
}

//-------------------------------
// シャドウマップ生成用 ピクセルシェーダ
//-------------------------------
float4 ShadowCasterPS(ShadowCasterVSOutput In) : SV_Target0
{
    return In.wvpPos.z / In.wvpPos.w;
}
