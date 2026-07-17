//----------------------------------------
// 共通テクスチャ
//----------------------------------------

Texture2D g_colorTex : register(t4);
Texture2D g_normalTex : register(t5);
Texture2D g_depthTex : register(t6);

// 平行光ShadowMap
Texture2D g_dirLightShadowMap : register(t7);

// IBL
TextureCube g_IBLTex : register(t8);

//----------------------------------------
// 共通サンプラ
//----------------------------------------
SamplerState g_ss_aniso_wrap : register(s0);
SamplerState g_ss_aniso_clamp : register(s1);
SamplerState g_ss_linear_wrap : register(s2);
SamplerState g_ss_linear_clamp : register(s3);
SamplerState g_ss_point_wrap : register(s4);
SamplerState g_ss_point_clamp : register(s5);

SamplerComparisonState g_ss_comparison : register(s15);

//----------------------------------------
// 定数バッファ[0]：マテリアル単位
//----------------------------------------
/*
cbuffer cbPerMaterial : register(b0)
{
};
*/

//----------------------------------------
// 定数バッファ[1]：描画単位
//----------------------------------------
cbuffer cbPerDraw : register(b1)
{
    row_major float4x4 g_mW;    // ワールド変換行列
};

//----------------------------------------
// 定数バッファ[7]：カメラ単位
//----------------------------------------
cbuffer cbPerCamera : register(b0)
{
    // カメラ情報
    row_major float4x4 g_mV;    // ビュー変換行列
    row_major float4x4 g_mInvV; // ビュー変換行列(逆行列)
    row_major float4x4 g_mP;    // 投影変換行列
    row_major float4x4 g_mInvP; // 投影変換行列(逆行列)

    row_major float4x4 g_mVP;       // ビュー*投影変換行列
    row_major float4x4 g_mInvVP; // ビュー*投影変換行列(逆行列)

    float3 g_CamPos;            // カメラ座標(World)
};


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

    float EnableShadow;
    float ShadowBias;
    row_major float4x4 mLightVP;
};

// ポイントライトのデータ
struct PointLight
{
    float3 Color; // 色
    float Radius; // 半径
    float3 Pos; // 座標
    float tmp;
};

cbuffer cbLight : register(b3)
{
    // スポット光の使用数
    int g_SL_Count;
    float3 g_dummy;
    
    //-------------------------
    // 環境光
    //-------------------------
    float3 g_AmbientLight;
    float  g_tmp2;

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
};

//----------------------------------------
// 定数バッファ[10]：UI
//----------------------------------------
cbuffer cbPerUI : register(b10)
{
    row_major float4x4 g_mUIP; // 正投影行列
    float4 g_Color; // 全体の色・不透明度
    float2 g_ScreenRes; // 画面解像度
    float2 g_tmpUI;
};

//----------------------------------------
// 定数バッファ[13]：システム
//----------------------------------------
cbuffer cbSystem : register(b4)
{
    float g_Time;
};

