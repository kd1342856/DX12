#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <DirectXTex.h>

//=============================
//
// テクスチャのDDSのキャッシュ(BC7に圧縮し、ミップを含めて保存する)
//
//   マテリアルのテクスチャ(ミップを作るもの)を、次からはデコードもミップの作成もせずに読めるよう、
//   <root>/Saved/TextureCache/<キー>.dds に保存する。BC7はRGBA8の1/4の大きさなので、VRAMも減る。
//   BC7への圧縮はCPUでは重いので、初めて読んだときは今までどおりの形で表示し、圧縮と保存は
//   裏のスレッド1本で1枚ずつ行う(次の起動から使われる)。終了したときに残っていた分は、次の起動で続きを作る。
//   キーは、ファイルならパス・大きさ・更新日時、埋め込まれた画像なら中身のハッシュに、色空間とキャッシュの版を加えたもの。
//
//=============================
class TextureCache
{
public:
    static TextureCache& Instance();

    // キャッシュの版。圧縮の仕方やミップの作り方を変えたら上げる(古いキャッシュは使われなくなる)
    static constexpr uint32_t kVersion = 1;

    // キャッシュのキーを作る
    static uint64_t MakeFileKey(const std::string& filePath, bool isSrgb);
    static uint64_t MakeMemoryKey(const void* pData, size_t size, bool isSrgb);

    // キャッシュにあれば読み込む(ワーカースレッドから呼んでよい)
    bool TryLoad(uint64_t key, DirectX::ScratchImage& outImage) const;
    // ミップまで作り終えた画像を、裏のスレッドでBC7に圧縮して保存する。圧縮できない大きさのものは何もしない
    void RequestStore(uint64_t key, DirectX::ScratchImage&& image);

    // 裏のスレッドを止める(終了するときに呼ぶ。まだ圧縮していないものは捨てる)
    void Shutdown();

private:
    TextureCache() = default;
    ~TextureCache();

    struct Job
    {
        uint64_t key = 0;
        DirectX::ScratchImage image;
    };

    static std::filesystem::path GetCacheDirectory();
    static std::filesystem::path GetCachePath(uint64_t key);
    void WorkerLoop();
    static void Compress(Job& job);

    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<Job> m_jobs;
    std::thread m_worker;
    bool m_isStopping = false;
    bool m_isStarted = false;
};
