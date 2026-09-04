// SSAO/SSR等、シーンのカメラ行列(cbPerCamera, b0)をPS側で使うフルスクリーンパス専用のVS。
// PostProcessShader_VS.hlslは同じb0にcbPostProcessを置いているため使い回せない(競合する)。
struct PSInput
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
};

PSInput main(uint vI : SV_VertexID)
{
    PSInput Out;
    Out.UV = float2((vI << 1) & 2, vI & 2);
    Out.Pos = float4(Out.UV * float2(2, -2) + float2(-1, 1), 0, 1);
    return Out;
}
