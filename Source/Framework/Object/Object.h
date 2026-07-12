#pragma once
#include <cstdint>
#include <string>
#include <random>

// =============================================
// Object
// GameObject のベースクラス
// UUID / Tag / Layer / Active を保持
// Texture・Mesh など Asset 類は継承しない
// =============================================
class Object {
public:
    Object() : m_uuid(GenerateUUID()) {}
    virtual ~Object() {}

    // UUID (生成時に自動付与、変更不可)
    uint64_t GetUUID() const { return m_uuid; }

    // 名前
    void               SetName(const std::string& name) { m_name = name; }
    const std::string& GetName() const { return m_name; }

    // タグ
    void               SetTag(const std::string& tag) { m_tag = tag; }
    const std::string& GetTag() const { return m_tag; }

    // レイヤー (ビットフラグ)
    void     SetLayer(uint32_t layer) { m_layer = layer; }
    uint32_t GetLayer() const { return m_layer; }

    // アクティブ状態
    void SetActive(bool active) { m_isActive = active; }
    bool IsActive() const { return m_isActive; }

    // 破棄 (派生クラスで必ず実装)
    virtual void Destroy() = 0;

protected:
    static uint64_t GenerateUUID() {
        static std::mt19937_64 rng(std::random_device{}());
        return rng();
    }

    uint64_t    m_uuid     = 0;
    std::string m_name     = "Object";
    std::string m_tag      = "Untagged";
    uint32_t    m_layer    = 0;
    bool        m_isActive = true;
};