#pragma once

enum class DebugView : int
{
    Final = 0, Albedo, Normal, WorldNormal, Roughness, Metallic, AO, Depth, Position, Emissive
};

struct DebugViewItem
{
    DebugView Value;
    const char* Name;
};

constexpr DebugViewItem DebugViewNames[] = {
    { DebugView::Final, "Final" },
    { DebugView::Albedo, "Albedo" },
    { DebugView::Normal, "Normal" },
    { DebugView::WorldNormal, "World Normal" },
    { DebugView::Roughness, "Roughness" },
    { DebugView::Metallic, "Metallic" },
    { DebugView::AO, "AO" },
    { DebugView::Depth, "Depth" },
    { DebugView::Position, "Position" },
    { DebugView::Emissive, "Emissive" }
};

struct RendererSettings
{
    bool IsDirty = true;
    bool EnablePBR = true;
};

struct LightingSettings
{
    bool IsDirty = true;
    DirectX::XMFLOAT3 DirectionalLightDir = { 0.5f, -1.0f, 0.3f };
    DirectX::XMFLOAT3 DirectionalLightColor = { 1.0f, 1.0f, 1.0f };
    float DirectionalLightIntensity = 1.0f;
    DirectX::XMFLOAT3 AmbientLight = { 0.35f, 0.35f, 0.35f };
};

struct ShadowSettings
{
    bool IsDirty = true;
    bool EnableShadows = true;
    float ShadowPower = 2.5f;
    float ShadowBias = 0.001f;
    DirectX::XMFLOAT4 CascadeSplits = { 0.1f, 0.3f, 0.6f, 1.0f };
};

struct IBLSettings
{
    bool IsDirty = true;
    bool EnableIBL = true;
    float IBLIntensity = 1.0f;
};

struct PostProcessSettings
{
    bool IsDirty = true;
    bool EnableHDR = true;
    float Gamma = 1.00f;
    float Exposure = 0.30f;
    float BloomThreshold = 1.0f;
    float BloomIntensity = 1.0f;
};

struct DebugSettings
{
    bool IsDirty = true;
    DebugView CurrentDebugView = DebugView::Final;
};

struct FogSettings
{
    bool IsDirty = true;

    // Distance fog (always present at FogDensity from the start of a match; grows to
    // HuntFogDensity while a ghost is hunting)
    bool EnableFog = true;
    DirectX::XMFLOAT3 FogColor = { 0.5f, 0.5f, 0.55f };
    float FogDensity = 0.02f;      // Density under normal conditions
    float HuntFogDensity = 0.18f;  // Density while a ghost is hunting

    // Screen darkening while a ghost is hunting (scales ambient + directional light down)
    bool EnableHuntDarken = true;
    float HuntDarkenAmount = 0.6f; // 0 = no darkening, 1 = lights fully off at full hunt intensity

    // Shared normal<->hunt blend rate, in "fraction per second" (1.0 = full transition in ~1s).
    // Drives both the fog density interpolation and the darkening above, so they move together.
    float HuntTransitionSpeed = 0.8f;
};
