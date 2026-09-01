#pragma once

class Random
{
public:
    static Random& Instance()
    {
        static Random instance;
        return instance;
    }

    // floatŒ^‚Ì—”
    float Range(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(m_mt);
    }

    // intŒ^‚Ì—”
    int Range(int min, int max)
    {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(m_mt);
    }

private:
    Random() : m_mt(std::random_device{}()) {}
    ~Random() = default;
    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;

    std::mt19937 m_mt;
};
