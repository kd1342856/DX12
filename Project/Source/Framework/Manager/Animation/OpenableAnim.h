#pragma once
#include <string>

// 家のモデル(と小物)の開け閉めアニメーションを名前で見分ける。
// - 部屋の出入口: "Door_..."(開き戸) / "Fusuma_..."(和室の襖) / "Entrance_..."(玄関の引き戸)。ナビメッシュの開口部・部屋同士の見え方・
//   ゴーストが勝手に開けるドアはこれだけを対象にする。
// - 家具の扉: "..._Open"(台所の戸棚・冷蔵庫・押入れの襖・仏壇など)。プレイヤーは開け閉めできるが、
//   部屋の出入口ではないのでナビメッシュやゴーストからは無視する。
// (以前は名前に"Door"を含むかだけで見ていたため、"Kitchen_BaseCabinet_Door_1_Open"等の戸棚まで出入口扱いになり、
//  逆に襖は開けられなかった)
namespace OpenableAnim
{
    inline bool StartsWith(const std::string& s, const char* prefix)
    {
        return s.rfind(prefix, 0) == 0;
    }

    inline bool EndsWith(const std::string& s, const char* suffix)
    {
        const size_t n = std::char_traits<char>::length(suffix);
        return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
    }

    // 部屋の出入口(ドア・襖)
    inline bool IsPassage(const std::string& animName)
    {
        return StartsWith(animName, "Door") || StartsWith(animName, "Fusuma") || StartsWith(animName, "Entrance");
    }

    // プレイヤーが開け閉めできるもの(出入口 + 家具の扉)
    inline bool IsOpenable(const std::string& animName)
    {
        return IsPassage(animName) || EndsWith(animName, "_Open");
    }

    // 全開になる再生位置。"..._Open"は開くだけのクリップなので最後が全開。
    // 以前からのドアのクリップ("..._OpenClose"など)は開いて閉じるまでが1本なので半分が全開
    // (AnimationSystemは開ける時にここで止め、閉める時は逆再生して0へ戻す)
    inline float FullOpenTime(const std::string& animName, float duration)
    {
        return EndsWith(animName, "_Open") ? duration : duration * 0.5f;
    }

    // 引き戸(襖・押入れ・玄関の引き戸)。開閉音を開き戸と変える
    inline bool IsSliding(const std::string& animName)
    {
        return animName.find("Fusuma") != std::string::npos || animName.find("Oshiire") != std::string::npos
            || animName.find("SlidingDoor") != std::string::npos;
    }

    // 開け閉めの効果音
    inline const char* OpenSound(const std::string& animName)
    {
        if (StartsWith(animName, "Entrance")) return "Asset/Sound/SE/hikido.wav"; // 玄関の引き戸
        return IsSliding(animName) ? "Asset/Sound/SE/fusuma_op.wav" : "Asset/Sound/SE/door_open.wav";
    }
    inline const char* CloseSound(const std::string& animName)
    {
        if (StartsWith(animName, "Entrance")) return "Asset/Sound/SE/hikido.wav";
        return IsSliding(animName) ? "Asset/Sound/SE/fusuma_close.wav" : "Asset/Sound/SE/door_open.wav";
    }
}
