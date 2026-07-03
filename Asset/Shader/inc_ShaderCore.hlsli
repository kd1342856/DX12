//----------------------------------------
// 蜈ｱ騾壹ユ繧ｯ繧ｹ繝√Ε
//----------------------------------------

Texture2D g_colorTex : register(t4);
Texture2D g_normalTex : register(t5);
Texture2D g_depthTex : register(t6);

// 蟷ｳ陦悟・ShadowMap
Texture2D g_dirLightShadowMap : register(t7);

Texture2D g_spotLightShadowMap : register(t9);

// IBL
TextureCube g_IBLTex : register(t8);

//----------------------------------------
// 蜈ｱ騾壹し繝ｳ繝励Λ
//----------------------------------------
SamplerState g_ss_aniso_wrap : register(s0);
SamplerState g_ss_aniso_clamp : register(s1);
SamplerState g_ss_linear_wrap : register(s2);
SamplerState g_ss_linear_clamp : register(s3);
SamplerState g_ss_point_wrap : register(s4);
SamplerState g_ss_point_clamp : register(s5);

SamplerComparisonState g_ss_comparison : register(s15);

//----------------------------------------
// 螳壽焚繝舌ャ繝輔ぃ[0]・壹・繝・Μ繧｢繝ｫ蜊倅ｽ・
//----------------------------------------
/*
cbuffer cbPerMaterial : register(b0)
{
};
*/

//----------------------------------------
// 螳壽焚繝舌ャ繝輔ぃ[1]・壽緒逕ｻ蜊倅ｽ・
//----------------------------------------
cbuffer cbPerDraw : register(b1)
{
    row_major float4x4 g_mW;    // 繝ｯ繝ｼ繝ｫ繝牙､画鋤陦悟・
};

//----------------------------------------
// 螳壽焚繝舌ャ繝輔ぃ[7]・壹き繝｡繝ｩ蜊倅ｽ・
//----------------------------------------
cbuffer cbPerCamera : register(b0)
{
    // 繧ｫ繝｡繝ｩ諠・?ｱ
    row_major float4x4 g_mV;    // 繝薙Η繝ｼ螟画鋤陦悟・
    row_major float4x4 g_mInvV; // 繝薙Η繝ｼ螟画鋤陦悟・(騾・｡悟・)
    row_major float4x4 g_mP;    // 蟆・ｽｱ螟画鋤陦悟・
    row_major float4x4 g_mInvP; // 蟆・ｽｱ螟画鋤陦悟・(騾・｡悟・)

    row_major float4x4 g_mVP;       // 繝薙Η繝ｼ*蟆・ｽｱ螟画鋤陦悟・
    row_major float4x4 g_mInvVP; // 繝薙Η繝ｼ*蟆・ｽｱ螟画鋤陦悟・(騾・｡悟・)

    float3 g_CamPos;            // 繧ｫ繝｡繝ｩ蠎ｧ讓・World)
};


//----------------------------------------
// 螳壽焚繝舌ャ繝輔ぃ[8]・壹Λ繧､繝・
//----------------------------------------

// 繧ｹ繝昴ャ繝医Λ繧､繝医・繝・・繧ｿ
struct SpotLight
{
    float3 Dir; // 蜈峨・譁ｹ蜷・
    float Range; // 遽・峇
    float3 Color; // 濶ｲ
    float InnerCorn; // 蜀・・縺ｮ隗貞ｺｦ
    float3 Pos; // 蠎ｧ讓・
    float OuterCorn; // 螟門・縺ｮ隗貞ｺｦ

    float EnableShadow;
    float ShadowBias;
    row_major float4x4 mLightVP;
};

// 繝昴う繝ｳ繝医Λ繧､繝医・繝・・繧ｿ
struct PointLight
{
    float3 Color; // 濶ｲ
    float Radius; // 蜊雁ｾ・
    float3 Pos; // 蠎ｧ讓・
    float tmp;
};

cbuffer cbLight : register(b3)
{
    // 繧ｹ繝昴ャ繝亥・縺ｮ菴ｿ逕ｨ謨ｰ
    int g_SL_Count;
    float3 g_dummy;
    
    //-------------------------
    // 迺ｰ蠅・・
    //-------------------------
    float3 g_AmbientLight;
    float  g_tmp2;

    //-------------------------
    // 蟷ｳ陦悟・
    //-------------------------
    float3 g_DL_Dir;    // 蟷ｳ陦悟・縺ｮ譁ｹ蜷・
    float  g_DL_DirLightShadowBias; // 蟷ｳ陦悟・縺ｮ繧ｷ繝｣繝峨え繝槭ャ繝礼畑繝舌う繧｢繧ｹ
    
    float3 g_DL_Color;  // 蟷ｳ陦悟・縺ｮ濶ｲ
    float  g_DL_ShadowPower;
    
    row_major float4x4 g_DL_mLightVP[3]; // 蟷ｳ陦悟・縺ｮ繝薙Η繝ｼ*蟆・ｽｱ陦悟・ (繧ｫ繧ｹ繧ｱ繝ｼ繝・譫壼・)
    float4 g_DL_CascadeSplits;       // 繧ｫ繧ｹ繧ｱ繝ｼ繝牙・蜑ｲ豺ｱ蠎ｦ

    //-------------------------
    // 繧ｹ繝昴ャ繝亥・
    //-------------------------
    SpotLight g_SL[10];
     
    //-------------------------
    // 繝輔か繧ｰ
    //-------------------------
    float3  g_DistanceFogColor;
    float   g_DistanceFogDensity;
};

//----------------------------------------
// 螳壽焚繝舌ャ繝輔ぃ[10]・啅I
//----------------------------------------
cbuffer cbPerUI : register(b10)
{
    row_major float4x4 g_mUIP; // 豁｣謚募ｽｱ陦悟・
    float4 g_Color; // 蜈ｨ菴薙・濶ｲ繝ｻ荳埼乗・蠎ｦ
    float2 g_ScreenRes; // 逕ｻ髱｢隗｣蜒丞ｺｦ
    float2 g_tmpUI;
};

//----------------------------------------
// 螳壽焚繝舌ャ繝輔ぃ[13]・壹す繧ｹ繝・Β
//----------------------------------------
cbuffer cbSystem : register(b4)
{
    float g_Time;
};

