#pragma once
#include "../../GPUResource/GPUResource.h"
// AssetState のみ必要（GPUUploadQueue.hを直接インクルードすると循環参照になる）
#include "../../../Framework/Manager/Asset/AssetState.h"

// 前方宣言
struct TextureUploadData;

// ============================================================
// Texture
// CPU ロードと GPU アップロードを完全分離した設計
//
//  LoadCPU()   : ワーカースレッドから呼び可能（ファイルI/O・デコードのみ）
//  CreateGPU() : メインスレッド専用（GPUリソース生成・アップロードコマンド記録）
//
// 将来的には CreateGPU() をさらに
//   CreateResource()        : GPU バッファ生成
//   RecordUploadCommands()  : コマンド記録
// に分割することも可能
// ============================================================
class Texture : public GPUResource
{
public:
    // ----------------------------------------------------------
    // ワーカースレッド対応：ファイルI/O + WIC/DirectXTex デコードのみ
    // GPU には一切触らない
    // ----------------------------------------------------------
    bool LoadCPU(const std::string& filePath, TextureUploadData& outData);
    bool LoadCPUFromMemory(const void* pData, size_t size, TextureUploadData& outData);

    // ----------------------------------------------------------
    // メインスレッド専用：CPUデータ → GPU リソース生成＋コマンド記録
    // Begin()/End() は GPUUploadQueue::Process() が外側で管理するため
    // この関数内では呼ばない
    // ----------------------------------------------------------
    bool CreateGPU(GraphicsDevice* pDevice, const TextureUploadData& data);

    // ----------------------------------------------------------
    // メモリから直接作成（メインスレッド専用・同期）
    // ホワイト/ブラック/ノーマルテクスチャ等のデフォルト生成に使用
    // ----------------------------------------------------------
    bool CreateFromMemory(const void* data, int width, int height, DXGI_FORMAT format);

    // シェーダーリソーステーブルにセット
    void Set(int index);

    int GetSRVNumber() const { return m_srvNumber; }

    // ----------------------------------------------------------
    // AssetState：描画前に IsReady() を確認すること
    // GPU アップロード前は m_resource == nullptr の可能性がある
    // ----------------------------------------------------------
    AssetState GetState() const { return m_state; }
    void SetState(AssetState state) { m_state = state; }
    bool IsReady() const { return m_state == AssetState::Ready; }

public:
    int m_srvNumber = 0;

private:
    AssetState m_state = AssetState::LoadingCPU;
};
