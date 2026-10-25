// ライトとシステムの定数バッファ、距離減衰。ShaderCore.hlsliと、ShaderCoreを使わない
// シェーダー(スキニング)の両方から読む
#ifndef INC_LIGHT_DATA_HLSLI
#define INC_LIGHT_DATA_HLSLI

//----------------------------------------
// 定数バッファ[8]：ライト
//----------------------------------------

// スポットライトのデータ
struct SpotLight
{
    float3 Dir; // 光の方向
    float Range; // 距離
    float3 Color; // 色
    float InnerCorn; // 内側の角度
    float3 Pos; // 座標
    float OuterCorn; // 外側の角度

    // 影(C++のLocalShadowManagerが割り当てる)。番号は-1なら影なし
    float ShadowIndex;     // 2D配列(g_spotShadowMaps)の枚の番号。狭いスポット
    float ShadowBias;      // 影のバイアスの倍率(1で標準)
    float ShadowCubeIndex; // キューブ配列(g_cubeShadowMaps)の個の番号。広いスポット
    float SourceRadius;    // 光源の半径(m)。PCSSの半影の広さに使う
    row_major float4x4 mLightVP; // 2D配列の影の行列
};

// ポイントライトのデータ (ロウソク/裸電球等。フリッカーはC++側(LightSystem)で
// 明滅させた結果のColorを毎フレーム書き込むので、シェーダー側はただの点光源として扱う)
struct PointLight
{
    float3 Pos; // 座標
    float Range; // 減衰距離
    float3 Color; // 色
    float ShadowCubeIndex; // キューブ配列(g_cubeShadowMaps)の個の番号。-1なら影なし
    float SourceRadius;    // 光源の半径(m)。PCSSの半影の広さに使う
    float ShadowBias;      // 影のバイアスの倍率(1で標準)
    float2 pad;
};

cbuffer cbLight : register(b3)
{
    // スポット光の使用数
    int g_SL_Count;
    // ダミーIBLと空の明るさ(nitの値に露出を掛けたもの)
    float g_IBLLuminance;
    float g_SkyLuminance;
    float g_dummy;
    
    //-------------------------
    // 環境光
    //-------------------------
    float3 g_AmbientLight;
    float  g_IndirectShadowFloor; // 平行光の影が落ちる場所に残す間接光(Ambient/IBL)の下限 0..1

    //-------------------------
    // 平行光
    //-------------------------
    float3 g_DL_Dir;    // 平行光の方向
    float  g_DL_DirLightShadowBias; // 平行光のシャドウマップ用バイアス
    
    float3 g_DL_Color;  // 平行光の色
    float  g_DL_ShadowPower;
    
    row_major float4x4 g_DL_mLightVP[3]; // 平行光のビュー*投影行列 (カスケード数分)
    float4 g_DL_CascadeSplits;       // カスケード分割深度

    //-------------------------
    // スポット光
    //-------------------------
    SpotLight g_SL[10];
     
    //-------------------------
    // フォグ
    //-------------------------
    float3  g_DistanceFogColor;
    float   g_DistanceFogDensity;

    //-------------------------
    // ポイント光 (ロウソク/裸電球等、フリッカー対応)
    //-------------------------
    int g_PL_Count;
    float3 g_dummyPL;
    PointLight g_PL[8];

    //-------------------------
    // 室内判定 (RoomAreaのAABB、xyzのみ使用)。範囲内のピクセルは外の光(月明かり・環境光・
    // 空の反射光)を受けず、月の影の中と同じ扱いになる
    //-------------------------
    int    g_IndoorCount;
    float3 g_PadIndoor;
    float4 g_IndoorMin[16];
    float4 g_IndoorMax[16];

    //-------------------------
    // 反射プローブ(部屋ごとのキューブマップ。箱はxyzだけを使う)
    //-------------------------
    int    g_ProbeCount;
    float  g_ProbeMipCount;
    float2 g_PadProbe;
    float4 g_ProbeBoxMin[16];
    float4 g_ProbeBoxMax[16];
    float4 g_ProbeCapturePosition[16]; // xyz: 撮った位置、w: 露出の比。0なら未撮影

    //-------------------------
    // DDGI(拡散の環境光のプローブ。inc_DDGI.hlsli)
    //-------------------------
    float4 g_DDGIOrigin; // xyz: 最初のプローブの位置、w: プローブの間隔(m)
    int4   g_DDGICounts; // xyz: 各軸のプローブの数、w: テクスチャの1行に並べるプローブの数
    float4 g_DDGIParams; // x: 有効(1)か、y: 法線の向きへずらす量、z: カメラの向きへずらす量
};

// ワールド座標がいずれかの室内範囲(RoomArea)に入っているか
bool IsIndoor(float3 wPos)
{
    for (int r = 0; r < g_IndoorCount; r++)
    {
        if (all(wPos >= g_IndoorMin[r].xyz) && all(wPos <= g_IndoorMax[r].xyz)) return true;
    }
    return false;
}

//----------------------------------------
// 定数バッファ[13]：システム
//----------------------------------------
cbuffer cbSystem : register(b4)
{
    int g_DebugView;
    int g_EnableHDR;
    float g_Exposure;
    float g_Gamma;
    float g_ScreenWidth;
    float g_ScreenHeight;

    // SSR (スクリーンスペース反射) - 平面反射が無いガラス等のフォールバックに使う
    int g_EnableSSR;
    float g_SSRStepSize;

    // 平面反射(窓ガラス等)用: 各反射スロット(最大3枚=3窓まで同時)の反射カメラView*Proj行列。
    // ワールド座標をこれで再投影してg_planarReflectionMapN(N=スロット番号)の正しいUVを求める。
    row_major float4x4 g_mReflectionVP[3];
    int4 g_HasReflectionPacked; // x,y,z = スロット0/1/2が今フレーム有効か(0/1)。wは未使用
};

//----------------------------------------
// ライトの距離減衰
//----------------------------------------
// 逆二乗の法則(1/d^2)。光度(cd)に掛けると、その距離での照度(lx)になる。
// rangeは打ち切りにだけ使い、端で途切れないよう窓関数(1-(d/r)^4)^2で滑らかに0にする(Karis 2013)
float DistanceAttenuation(float distanceToLight, float range)
{
    float distanceSq = distanceToLight * distanceToLight;
    float ratio = distanceSq / max(range * range, 0.0001);
    float window = saturate(1.0 - ratio * ratio);
    // 光源に1cmより近づいても明るさが発散しないようにする
    return window * window / max(distanceSq, 0.0001);
}

#endif
