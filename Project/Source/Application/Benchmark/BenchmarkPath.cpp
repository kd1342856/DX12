#include "../../Pch.h"
#include "BenchmarkPath.h"
#include "nlohmann/json.hpp"

namespace
{
    // 一様な Catmull-Rom。t は 0 から 1 で、p1 から p2 までを通る
    float CatmullRom(float p0, float p1, float p2, float p3, float t)
    {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
            + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }

    Math::Vector3 CatmullRom(const Math::Vector3& p0, const Math::Vector3& p1, const Math::Vector3& p2, const Math::Vector3& p3, float t)
    {
        return Math::Vector3(
            CatmullRom(p0.x, p1.x, p2.x, p3.x, t),
            CatmullRom(p0.y, p1.y, p2.y, p3.y, t),
            CatmullRom(p0.z, p1.z, p2.z, p3.z, t));
    }

    // angle を reference から半周以内になるように 2π ずつずらす(遠回りに回らないように)
    float UnwrapAngle(float angle, float reference)
    {
        while (angle - reference > DirectX::XM_PI)
        {
            angle -= DirectX::XM_2PI;
        }
        while (angle - reference < -DirectX::XM_PI)
        {
            angle += DirectX::XM_2PI;
        }
        return angle;
    }
}

bool BenchmarkPath::Load(const std::string& path)
{
    std::ifstream in(path);
    if (!in)
    {
        return false;
    }
    nlohmann::json root = nlohmann::json::parse(in, nullptr, false);
    if (root.is_discarded() || !root.is_object())
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[Benchmark] 経路のJSONを読めなかった: %s", path.c_str());
        return false;
    }

    m_speed = (std::max)(root.value("speed", 1.5f), 0.1f);
    m_warmupSeconds = (std::max)(root.value("warmupSeconds", 3.0f), 0.0f);
    m_points.clear();
    if (root.contains("points") && root["points"].is_array())
    {
        for (const nlohmann::json& pointJson : root["points"])
        {
            BenchmarkPoint point;
            point.label = pointJson.value("label", std::string());
            if (pointJson.contains("position") && pointJson["position"].is_array() && pointJson["position"].size() == 3)
            {
                point.position = Math::Vector3(pointJson["position"][0].get<float>(), pointJson["position"][1].get<float>(), pointJson["position"][2].get<float>());
            }
            point.pitch = pointJson.value("pitch", 0.0f);
            point.yaw = pointJson.value("yaw", 0.0f);
            point.waitSeconds = (std::max)(pointJson.value("waitSeconds", 0.0f), 0.0f);
            point.hunt = pointJson.value("hunt", false);
            m_points.push_back(point);
        }
    }
    return true;
}

bool BenchmarkPath::Save(const std::string& path) const
{
    nlohmann::json root;
    root["speed"] = m_speed;
    root["warmupSeconds"] = m_warmupSeconds;
    root["points"] = nlohmann::json::array();
    for (const BenchmarkPoint& point : m_points)
    {
        nlohmann::json pointJson;
        pointJson["label"] = point.label;
        pointJson["position"] = { point.position.x, point.position.y, point.position.z };
        pointJson["pitch"] = point.pitch;
        pointJson["yaw"] = point.yaw;
        pointJson["waitSeconds"] = point.waitSeconds;
        pointJson["hunt"] = point.hunt;
        root["points"].push_back(pointJson);
    }

    std::error_code errorCode;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), errorCode);
    std::ofstream out(path);
    if (!out)
    {
        Logger::Instance().AddLog(Logger::LogLevel::Error, u8"[Benchmark] 経路を保存できなかった: %s", path.c_str());
        return false;
    }
    out << root.dump(2);
    return true;
}

float BenchmarkPath::GetTravelSeconds(size_t pointIndex) const
{
    if (pointIndex + 1 >= m_points.size())
    {
        return 0.0f;
    }
    // 弧の長さではなく点の間の直線の長さで決める(曲がり方が強い所は少し速く動く)
    const float distance = Math::Vector3::Distance(m_points[pointIndex].position, m_points[pointIndex + 1].position);
    return (std::max)(distance / m_speed, 0.1f);
}

float BenchmarkPath::GetDuration() const
{
    float duration = 0.0f;
    for (size_t pointIndex = 0; pointIndex < m_points.size(); ++pointIndex)
    {
        duration += m_points[pointIndex].waitSeconds + GetTravelSeconds(pointIndex);
    }
    return duration;
}

BenchmarkPose BenchmarkPath::Evaluate(float time) const
{
    BenchmarkPose pose;
    if (m_points.empty())
    {
        return pose;
    }

    // 時刻がどの点で止まっている間か、どの点の間を動いている間かを探す
    float remaining = (std::max)(time, 0.0f);
    const size_t lastIndex = m_points.size() - 1;
    for (size_t pointIndex = 0; pointIndex <= lastIndex; ++pointIndex)
    {
        const BenchmarkPoint& point = m_points[pointIndex];
        pose.segmentIndex = static_cast<int>(pointIndex);

        if (remaining <= point.waitSeconds || pointIndex == lastIndex)
        {
            pose.position = point.position;
            pose.pitch = point.pitch;
            pose.yaw = point.yaw;
            return pose;
        }
        remaining -= point.waitSeconds;

        const float travelSeconds = GetTravelSeconds(pointIndex);
        if (remaining < travelSeconds)
        {
            const BenchmarkPoint& prev = m_points[pointIndex > 0 ? pointIndex - 1 : pointIndex];
            const BenchmarkPoint& next = m_points[pointIndex + 1];
            const BenchmarkPoint& afterNext = m_points[(std::min)(pointIndex + 2, lastIndex)];
            const float t = remaining / travelSeconds;

            pose.position = CatmullRom(prev.position, point.position, next.position, afterNext.position, t);

            // 向きも同じ曲線で補間する。ヨーは隣の点との差が半周以内になるようにそろえる
            const float yaw1 = point.yaw;
            const float yaw0 = UnwrapAngle(prev.yaw, yaw1);
            const float yaw2 = UnwrapAngle(next.yaw, yaw1);
            const float yaw3 = UnwrapAngle(afterNext.yaw, yaw2);
            pose.yaw = CatmullRom(yaw0, yaw1, yaw2, yaw3, t);
            pose.pitch = CatmullRom(prev.pitch, point.pitch, next.pitch, afterNext.pitch, t);
            return pose;
        }
        remaining -= travelSeconds;
    }
    return pose;
}
