struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

cbuffer cbFog : register(b0)
{
    float g_Time;
    float3 g_Padding;
};

float hash(float2 p) 
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453123);
}

float noise(float2 p) 
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(
        lerp(hash(i + float2(0.0, 0.0)), hash(i + float2(1.0, 0.0)), u.x),
        lerp(hash(i + float2(0.0, 1.0)), hash(i + float2(1.0, 1.0)), u.x),
        u.y
    );
}

float fbm(float2 p) 
{
    float f = 0.0;
    float w = 0.5;
    for (int i = 0; i < 5; i++) {
        f += w * noise(p);
        p *= 2.0;
        w *= 0.5;
    }
    return f;
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.UV;
    
    // Scroll coordinates over time
    float2 movement = float2(g_Time * 0.05, g_Time * 0.08);
    
    // Domain warping for fluid-like motion
    float2 q = float2(fbm(uv * 3.0 + movement), fbm(uv * 3.0 - movement));
    float2 r = float2(fbm(uv * 5.0 + q + movement * 1.5), fbm(uv * 5.0 + q - movement * 1.5));
    
    float f = fbm(uv * 2.0 + r);

    // Color gradient (Dark red    // Colors
    float3 darkRed = float3(0.0, 0.0, 0.0);
    float3 brightRed = float3(0.25, 0.0, 0.0); // Deep blood red
    float3 col = lerp(darkRed, brightRed, f * f * 2.5); // Higher contrast to emphasize black areas
    
    // Vignette effect (darker edges)
    float dist = distance(uv, float2(0.5, 0.5));
    col *= smoothstep(0.9, 0.05, dist); // Harsher vignette

    // Add a slight fade-in at the very beginning
    float alpha = smoothstep(0.0, 1.0, g_Time);

    // Using Red Fog
    return float4(alpha * col.r, 0.0, 0.0, 1.0); // col is grayscale so col.r is fine
}
