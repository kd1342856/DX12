#pragma once
#include <map>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>
#include "../Shader/ComputeShader/ComputeShader.h"

class Mesh;
class Material;

//=============================
//
// レイトレースの加速構造(DXR 1.1のインラインのレイトレースで使う)
//
//   動かないモデルのメッシュごとにBLASを1回だけ作り、毎フレームその位置(ワールド行列)を並べてTLASを作り直す。
//   スキニングのモデル(幽霊など)は、毎フレーム骨の行列で変形した頂点をコンピュートで作り、BLASを更新する。
//   半透明のメッシュは入れない。
//   当たった三角形の色を求めるために、インスタンスごとの頂点・インデックス・マテリアルの番号を
//   構造化バッファ(GeometryInfo)にして渡す。インスタンスのInstanceIDがその番号になる。
//   TLASとGeometryInfoは、GPUが前のフレームで使っている間に書き換えないよう、フレームの数だけ持つ。
//
//=============================
class RaytracingScene
{
public:
    static RaytracingScene& Instance()
    {
        static RaytracingScene instance;
        return instance;
    }

    // シェーダーのGeometryInfoと同じ並び(inc_Raytracing.hlsli)
    struct GeometryInfo
    {
        uint32_t vertexBufferIndex;     // 頂点(ByteAddressBuffer)のSRVの番号
        uint32_t indexBufferIndex;      // インデックス(ByteAddressBuffer、32ビット)のSRVの番号
        uint32_t baseColorTextureIndex; // ベースカラーのSRVの番号。無ければ0xFFFFFFFF
        uint32_t emissiveTextureIndex;  // 発光のSRVの番号。無ければ0xFFFFFFFF
        Math::Vector4 baseColorFactor;
        Math::Vector3 emissive;         // 発光の輝度(nit) = 色 × 強さ
        float pad;
    };

    // DXR 1.1(インラインのレイトレース)に対応しているかを調べる。デバイスを作った後に一度だけ呼ぶ
    void Initialize();
    bool IsSupported() const { return m_isSupported; }

    // フレームの最初に呼び、この後AddInstanceで今のフレームのインスタンスを並べる
    void BeginFrame();
    // 動かないメッシュを1つ置く。BLASがまだ無ければ作る(1フレームに作る数には上限がある)
    void AddInstance(const Mesh& mesh, const Material& material, const Math::Matrix& world);
    // スキニングのメッシュを1つ置く。ownerIdは持ち主(エンティティ)の番号。bonesはこのフレームの骨の行列(中で写しを取る)
    void AddSkinnedInstance(uint32_t ownerId, const Mesh& mesh, const Material& material, const Math::Matrix& world, const std::vector<Math::Matrix>& bones);
    // BLASとTLASを作るコマンドを積む。描画(レイトレース)より前に呼ぶ
    void Build();

    // 今のフレームのTLASとGeometryInfoのSRVの番号。インスタンスが無ければ-1
    int GetTLASIndex() const { return m_hasTLAS ? m_frames[m_frameSlot].tlasSrvIndex : -1; }
    int GetGeometryInfoIndex() const { return m_hasTLAS ? m_frames[m_frameSlot].geometrySrvIndex : -1; }
    UINT GetInstanceCount() const { return static_cast<UINT>(m_instanceDescs.size()); }

    // シーンを読み直すときに、作ったBLASを捨てる
    void Clear();

private:
    RaytracingScene() = default;

    struct BottomLevel
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> cpBuffer;
        D3D12_GPU_VIRTUAL_ADDRESS vertexAddress = 0; // 作ったときの頂点バッファ(メッシュが作り直されたかの確認)
        int vertexSrvIndex = -1;
        int indexSrvIndex = -1;
        bool isBuilt = false;
        // 毎フレーム更新するBLAS(スキニング)か。更新できるように作り、2回目からは更新する
        bool allowUpdate = false;
    };

    // このフレームで作る(または更新する)BLAS
    struct PendingBuild
    {
        BottomLevel* pBottomLevel = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS vertexAddress = 0;
        UINT vertexCount = 0;
        D3D12_GPU_VIRTUAL_ADDRESS indexAddress = 0;
        UINT indexCount = 0;
    };

    // スキニングのメッシュの、変形した頂点とBLAS
    struct SkinnedGeometry
    {
        BottomLevel bottomLevel;
        Microsoft::WRL::ComPtr<ID3D12Resource> cpVertexBuffer; // 変形した頂点(UAVとSRV)
        int sourceSrvIndex = -1; // 元の頂点
        int outputUavIndex = -1;
        UINT vertexCount = 0;
        bool hasBeenWritten = false; // 一度でも書いたか(最初は暗黙の昇格でUAVになる)
        uint64_t lastUsedFrame = 0;
    };

    // このフレームで頂点を変形するスキニングのメッシュ
    struct SkinJob
    {
        SkinnedGeometry* pGeometry = nullptr;
        std::vector<Math::Matrix> bones; // 呼び出し元の行列は一時オブジェクトのことがあるので写しを持つ
    };

    struct FrameResources
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> cpTLAS;
        Microsoft::WRL::ComPtr<ID3D12Resource> cpInstanceBuffer; // UPLOAD
        Microsoft::WRL::ComPtr<ID3D12Resource> cpGeometryBuffer; // UPLOAD
        UINT64 tlasSize = 0;
        UINT instanceCapacity = 0;
        int tlasSrvIndex = -1;
        int geometrySrvIndex = -1;
    };

    BottomLevel* FindOrCreateBottomLevel(const Mesh& mesh);
    SkinnedGeometry* FindOrCreateSkinnedGeometry(uint32_t ownerId, const Mesh& mesh);
    void AddInstanceDesc(BottomLevel* pBottomLevel, const Material& material, const Math::Matrix& world);
    void ReleaseSkinnedGeometry(SkinnedGeometry& geometry);
    void SkinVertices();
    bool EnsureBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& cpBuffer, UINT64& capacity, UINT64 size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state, const wchar_t* pName);

    static constexpr int kFrameCount = 3;
    // 1フレームにBLASを作る数の上限(読み込み直後にフレームが止まらないように)
    static constexpr int kMaxBottomLevelBuildsPerFrame = 64;

    bool m_isSupported = false;
    std::unordered_map<const Mesh*, BottomLevel> m_bottomLevels;
    std::vector<PendingBuild> m_pendingBuilds;
    int m_pendingStaticBuildCount = 0;

    std::map<std::pair<uint32_t, const Mesh*>, SkinnedGeometry> m_skinnedGeometries;
    std::vector<SkinJob> m_skinJobs;
    ComputeShader m_skinShader;
    uint64_t m_frameCounter = 0;

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_instanceDescs;
    std::vector<GeometryInfo> m_geometryInfos;

    FrameResources m_frames[kFrameCount];
    int m_frameSlot = 0;
    bool m_hasTLAS = false;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_cpScratch;
    UINT64 m_scratchSize = 0;
};
