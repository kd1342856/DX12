struct VSInput
{
    float3 pos : POSITION;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
};

VSOutput main(VSInput v)
{
    VSOutput o;
    o.pos = float4(v.pos, 1.0f); // šw‚ğ1‚ÉŒÅ’è
    return o;
}
