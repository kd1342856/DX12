#pragma once

//=============================
//
// 影用の深度テクスチャの配列
//
//   2D配列   : スポットライト用。1枚ごとに1灯(Texture2DArray)
//   キューブ配列: 点光源と広角のスポットライト用。6面で1灯(TextureCubeArray)
//
// 描き込みは1面ずつのDSVで行い、シェーダーからは配列全体を1つのSRVで読む。
// 中身はフレームをまたいで残るので、描き直しが要らない面はそのまま使い回せる(影のキャッシュ)。
//
//=============================
class ShadowMapArray : public GPUResource
{
public:
    // layerCount: 2Dなら枚数、キューブなら個数(面の数はその6倍)
    bool Create(GraphicsDevice* pGraphicsDevice, UINT resolution, UINT layerCount, bool isCube);
    // 解像度だけを変えて作り直す(中身は消える)。ディスクリプタの番号は変わらない。GPUの処理が終わってから呼ぶこと
    bool Resize(UINT resolution);

    UINT GetResolution() const { return m_resolution; }
    UINT GetLayerCount() const { return m_layerCount; }
    bool IsCube() const { return m_isCube; }
    int GetSRVNumber() const { return m_srvNumber; }

    // faceIndex: 2Dなら枚の番号、キューブなら「個の番号×6+面の番号(+X,-X,+Y,-Y,+Z,-Z)」
    UINT GetDSVNumber(UINT faceIndex) const { return m_dsvNumbers[faceIndex]; }
    void ClearFace(UINT faceIndex);

private:
    UINT GetFaceCount() const { return m_layerCount * (m_isCube ? 6 : 1); }
    bool CreateResource(UINT resolution);
    void WriteDescriptors();

    UINT m_resolution = 0;
    UINT m_layerCount = 0;
    bool m_isCube = false;
    int m_srvNumber = -1;
    std::vector<UINT> m_dsvNumbers;
};
