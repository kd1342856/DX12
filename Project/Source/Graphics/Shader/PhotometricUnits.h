#pragma once
#include <algorithm>
#include <cmath>

//=============================
//
// 物理単位のライトと露出の計算
//
//   スポットライト : 光度 cd(カンデラ)
//   点光源         : 光束 lm(ルーメン)。光度に直すと I = Φ / 4π
//   平行光(月)     : 照度 lx(ルクス)
//   発光・環境光   : 輝度 cd/m2(nit)
//
// ライトの色は「色温度(K) × 色味」を輝度1に正規化したもので、強さはすべて上の単位に乗せる。
// 露出はEV100で持ち、CPU側でライトの値に掛けてからシェーダーへ送る(前露出)。
//
//=============================
namespace PhotometricUnits
{
    constexpr float kPi = 3.14159265358979f;

    // 点光源の光束(lm)を光度(cd)に直す。全方向へ均等に出る光源として扱う
    inline float LumensToCandela(float lumens)
    {
        return lumens / (4.0f * kPi);
    }

    // 線形sRGB(Rec.709)の輝度
    inline float Luminance(const Math::Vector3& color)
    {
        return color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
    }

    // 色温度(K)の黒体放射の色を、輝度1の線形sRGBで返す。
    // プランク軌跡の近似式(Kim et al. 2002)でxy色度を求め、XYZから線形sRGBへ変換する。
    // 1667K～25000Kの範囲で有効(範囲外は端の値に丸める)
    inline Math::Vector3 ColorTemperatureToLinearRgb(float kelvin)
    {
        const float temperature = std::clamp(kelvin, 1667.0f, 25000.0f);
        const float t1 = 1000.0f / temperature;
        const float t2 = t1 * t1;
        const float t3 = t2 * t1;

        float x = 0.0f;
        if (temperature <= 4000.0f)
        {
            x = -0.2661239f * t3 - 0.2343589f * t2 + 0.8776956f * t1 + 0.179910f;
        }
        else
        {
            x = -3.0258469f * t3 + 2.1070379f * t2 + 0.2226347f * t1 + 0.240390f;
        }

        const float x2 = x * x;
        const float x3 = x2 * x;
        float y = 0.0f;
        if (temperature <= 2222.0f)
        {
            y = -1.1063814f * x3 - 1.34811020f * x2 + 2.18555832f * x - 0.20219683f;
        }
        else if (temperature <= 4000.0f)
        {
            y = -0.9549476f * x3 - 1.37418593f * x2 + 2.09137015f * x - 0.16748867f;
        }
        else
        {
            y = 3.0817580f * x3 - 5.87338670f * x2 + 3.75112997f * x - 0.37001483f;
        }

        // Y=1のXYZ
        const float bigX = x / y;
        const float bigZ = (1.0f - x - y) / y;
        Math::Vector3 rgb(
            3.2404542f * bigX - 1.5371385f - 0.4985314f * bigZ,
            -0.9692660f * bigX + 1.8760108f + 0.0415560f * bigZ,
            0.0556434f * bigX - 0.2040259f + 1.0572252f * bigZ);

        // 低い色温度では青がsRGBの色域の外(負)になるので0に丸め、丸めた後の輝度で1に揃え直す
        rgb.x = (std::max)(rgb.x, 0.0f);
        rgb.y = (std::max)(rgb.y, 0.0f);
        rgb.z = (std::max)(rgb.z, 0.0f);
        const float luminance = Luminance(rgb);
        return luminance > 1e-6f ? rgb / luminance : Math::Vector3(1.0f, 1.0f, 1.0f);
    }

    // ライトの色。色温度(0以下なら使わない)に色味を掛け、輝度1に正規化する。
    // 正規化するので、ライトの明るさは色によらず強さ(cd・lm・lx)だけで決まる
    inline Math::Vector3 MakeLightColor(const Math::Vector3& tint, float kelvin)
    {
        Math::Vector3 color = tint;
        if (kelvin > 0.0f)
        {
            color *= ColorTemperatureToLinearRgb(kelvin);
        }
        const float luminance = Luminance(color);
        return luminance > 1e-6f ? color / luminance : Math::Vector3::Zero;
    }

    // 絞り(F値)・シャッター速度(秒)・ISO感度からEV100を求める
    inline float ComputeEV100(float aperture, float shutterSeconds, float iso)
    {
        return std::log2((aperture * aperture) / (std::max)(shutterSeconds, 1e-6f) * 100.0f / (std::max)(iso, 1.0f));
    }

    // EV100から、輝度(nit)に掛ける露出を求める(飽和に基づく式。Lagarde 2014)。
    // EV100=0で輝度1.2 nitが白飛びの手前、EVが1上がるごとに倍の明るさまで写せる
    inline float ExposureFromEV100(float ev100)
    {
        return 1.0f / (1.2f * std::exp2(ev100));
    }
}
