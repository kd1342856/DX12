#pragma once

//=============================
//
// ベンチマークのカメラの経路
//
//   エディターの自由カメラで打った点(位置と向き)を、一定の速さで順に回る。
//   点の間は Catmull-Rom で滑らかにつなぐ。点ごとに、そこで止まる時間と、
//   その点から次の点までをハント中の状態にするかを持つ。
//   保存先は Asset/Data/Benchmark/ の JSON(UTF-8)。
//
//=============================
struct BenchmarkPoint
{
    std::string label;         // 区間の名前(CSVの区間ごとの結果に出る)
    Math::Vector3 position;
    float pitch = 0.0f;        // ラジアン。正で下を向く(TransformData::m_rotation.x と同じ)
    float yaw = 0.0f;          // ラジアン(TransformData::m_rotation.y と同じ)
    float waitSeconds = 0.0f;  // この点に着いてから止まっている時間
    bool hunt = false;         // この点から次の点に着くまで、ハント中の状態にする
};

// 経路のある時刻での、カメラの位置と向き
struct BenchmarkPose
{
    Math::Vector3 position;
    float pitch = 0.0f;
    float yaw = 0.0f;
    int segmentIndex = 0;  // 今いる区間(出発した点の番号)
};

class BenchmarkPath
{
public:
    static constexpr const char* kDefaultPath = "Asset/Data/Benchmark/HousePath.json";

    bool Load(const std::string& path);
    bool Save(const std::string& path) const;

    std::vector<BenchmarkPoint>& GetPoints() { return m_points; }
    const std::vector<BenchmarkPoint>& GetPoints() const { return m_points; }

    float GetSpeed() const { return m_speed; }
    void SetSpeed(float speed) { m_speed = (std::max)(speed, 0.1f); }

    // 回り始める前に、最初の点で待つ時間(計測しない。TAAや自動露出を落ち着かせる)
    float GetWarmupSeconds() const { return m_warmupSeconds; }
    void SetWarmupSeconds(float seconds) { m_warmupSeconds = (std::max)(seconds, 0.0f); }

    // 点が2つ以上あれば回れる
    bool IsValid() const { return m_points.size() >= 2; }

    // 最初の点から最後の点で止まり終わるまでの時間(ウォームアップは含まない)
    float GetDuration() const;

    // 時刻(秒、0 から GetDuration())での位置と向き
    BenchmarkPose Evaluate(float time) const;

private:
    // 点iから点i+1までの移動にかかる時間
    float GetTravelSeconds(size_t pointIndex) const;

    std::vector<BenchmarkPoint> m_points;
    float m_speed = 1.5f;          // m/s。歩くくらいの速さ
    float m_warmupSeconds = 3.0f;
};
