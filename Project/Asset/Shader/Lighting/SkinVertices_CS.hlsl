//================================
//
// スキニングのモデルの頂点を、骨の行列で変形して書き出す(レイトレースの加速構造に入れるため)
//
//   頂点の並びはMeshVertexのまま(位置12、UV8、法線12、色4、接線12、骨の番号2x4、骨の重み4x4 = 72バイト)。
//   位置・法線・接線を、SkinningShader_VSと同じ式で骨の行列を混ぜて変形する。ワールド行列は掛けない
//   (加速構造のインスタンスの変換で掛ける)。
//
//================================

struct Constants
{
    uint SourceIndex; // 元の頂点(ByteAddressBuffer)
    uint OutputIndex; // 変形した頂点(RWByteAddressBuffer)
    uint VertexCount;
};
ConstantBuffer<Constants> g_constants : register(b0);

cbuffer cbBones : register(b1)
{
    row_major float4x4 g_mBones[256];
};

static const uint kVertexStride = 72;

[numthreads(64, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    const uint vertexIndex = dispatchId.x;
    if (vertexIndex >= g_constants.VertexCount)
    {
        return;
    }
    ByteAddressBuffer source = ResourceDescriptorHeap[g_constants.SourceIndex];
    RWByteAddressBuffer output = ResourceDescriptorHeap[g_constants.OutputIndex];
    const uint offset = vertexIndex * kVertexStride;

    const float3 position = asfloat(source.Load3(offset));
    const float2 uv = asfloat(source.Load2(offset + 12));
    const float3 normal = asfloat(source.Load3(offset + 20));
    const uint color = source.Load(offset + 32);
    const float3 tangent = asfloat(source.Load3(offset + 36));
    const uint2 packedIndices = source.Load2(offset + 48);
    const float4 weights = asfloat(source.Load4(offset + 56));
    const uint4 indices = uint4(packedIndices.x & 0xFFFF, packedIndices.x >> 16, packedIndices.y & 0xFFFF, packedIndices.y >> 16);

    row_major float4x4 bones = 0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        bones += g_mBones[min(indices[i], 255u)] * weights[i];
    }
    const float3 skinnedPosition = mul(float4(position, 1.0), bones).xyz;
    const float3 skinnedNormal = mul(normal, (float3x3)bones);
    const float3 skinnedTangent = mul(tangent, (float3x3)bones);

    output.Store3(offset, asuint(skinnedPosition));
    output.Store2(offset + 12, asuint(uv));
    output.Store3(offset + 20, asuint(skinnedNormal));
    output.Store(offset + 32, color);
    output.Store3(offset + 36, asuint(skinnedTangent));
    output.Store2(offset + 48, packedIndices);
    output.Store4(offset + 56, asuint(weights));
}
