// Clear-screen background: a pale, slowly drifting mist.
// The counterpart of FogShader (the game-over screen's blood-red fog), in cold blue-white.
struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

cbuffer cbMist : register(b0)
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

// One layer of domain-warped mist drifting in the given direction.
float MistLayer(float2 uv, float t, float2 drift, float scale)
{
    float2 move = drift * t;
    float2 q = float2(fbm(uv * scale + move), fbm(uv * scale - move + 3.1));
    float2 r = float2(fbm(uv * scale * 1.6 + q + move * 1.4), fbm(uv * scale * 1.6 + q - move * 1.4 + 7.3));
    return fbm(uv * scale * 0.7 + r);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.UV;   // (0,0) top-left, (1,1) bottom-right
    float t = g_Time;

    // Two layers drifting in different directions at different speeds give the mist some depth.
    float back  = MistLayer(uv, t, float2(0.020, -0.012), 2.0);
    float front = MistLayer(uv + 5.0, t, float2(-0.035, 0.008), 3.0);

    // Cold, pale palette: near-black slate in the thin parts, blue-white where the mist is thick.
    float3 deep  = float3(0.02, 0.03, 0.05);
    float3 mid   = float3(0.18, 0.22, 0.28);
    float3 pale  = float3(0.62, 0.68, 0.74);
    float3 col = lerp(deep, mid, smoothstep(0.2, 0.7, back));
    col = lerp(col, pale, smoothstep(0.45, 0.95, front) * 0.55);

    // Mist settles lower down: a little thicker toward the bottom of the screen.
    col += float3(0.10, 0.12, 0.14) * smoothstep(0.4, 1.0, uv.y) * front;

    // Vignette, and keep the middle (where the report text sits) from getting too bright.
    float v = distance(uv, float2(0.5, 0.5));
    col *= smoothstep(0.95, 0.15, v);
    col = min(col, 0.6);

    // Fade in from black at the start (matches the scene-change fade).
    col *= smoothstep(0.0, 1.0, t);

    return float4(col, 1.0);
}
