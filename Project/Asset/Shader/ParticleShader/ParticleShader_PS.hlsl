struct VSOutput
{
    float4 Pos   : SV_Position;
    float2 UV    : TEXCOORD0; // -1..1 (中心が0)
    float4 Color : COLOR0;
};

float4 main(VSOutput In) : SV_Target
{
    // 中心が明るく、縁に向かって柔らかく消える光の粒(加算合成なので重なるほど明るくなる)
    float d = length(In.UV);
    float falloff = saturate(1.0f - d);
    falloff *= falloff;
    return float4(In.Color.rgb, In.Color.a * falloff);
}
