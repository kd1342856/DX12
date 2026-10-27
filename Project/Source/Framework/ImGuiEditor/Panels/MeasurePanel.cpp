#include "MeasurePanel.h"
#include "../EditorContext.h"
#include "ImGui/imgui.h"
#include "../Widgets/SettingsFieldUI.h"
#include "../../../Graphics/Renderer/AutoExposure.h"
#include "../../../Graphics/Renderer/PixelProbe.h"
#include "../../../Graphics/Renderer/ReferenceCompare.h"
#include <cmath>

namespace
{
    // 絶対の輝度(cd/m2)からEV100へ。EV100 = log2(L * 100 / 12.5)
    float LuminanceToEV100(float luminance)
    {
        return std::log2((std::max)(luminance, 1e-6f)) + 3.0f;
    }
}

void MeasurePanel::Draw(EditorContext& ctx)
{
    (void)ctx;
    // 検索中は値のパネルだけを見せる
    if (SettingsFieldUI::IsSearching())
    {
        return;
    }
    if (!ImGui::CollapsingHeader(u8"計測と比較###Measure"))
    {
        // 閉じている間は、読み戻しも比べるのも止める
        return;
    }
    ImGui::PushID("Measure");
    DrawHistogram();
    ImGui::Separator();
    DrawPixelInspector();
    ImGui::Separator();
    DrawReferenceCompare();
    ImGui::PopID();
}

void MeasurePanel::DrawHistogram()
{
    auto& autoExposure = AutoExposure::Instance();
    ImGui::TextUnformatted(u8"明るさのヒストグラム");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"画面の画素を明るさ(EV100)ごとに数えたもの。露出を掛ける前の絶対の輝度で数える。\n自動露出と同じデータで、自動露出が切れていても、このパネルを開いている間は数える。\n山が右の端に寄っていれば白飛び、左の端に寄っていれば黒つぶれしやすい。");
    }
    if (!autoExposure.IsAvailable())
    {
        ImGui::TextDisabled(u8"自動露出のシェーダーを作れなかったので使えません");
        return;
    }
    autoExposure.RequestHistogram();

    const uint32_t* pBins = autoExposure.GetLatestHistogram();
    if (!pBins)
    {
        ImGui::TextDisabled(u8"数えています…");
        return;
    }
    float values[AutoExposure::kHistogramBinCount] = {};
    float maxValue = 1.0f;
    uint64_t total = 0;
    for (UINT binIndex = 0; binIndex < AutoExposure::kHistogramBinCount; ++binIndex)
    {
        values[binIndex] = static_cast<float>(pBins[binIndex]);
        maxValue = (std::max)(maxValue, values[binIndex]);
        total += pBins[binIndex];
    }
    ImGui::PlotHistogram("##histogram", values, static_cast<int>(AutoExposure::kHistogramBinCount), 0, nullptr,
        0.0f, maxValue, ImVec2(-1.0f, 90.0f));

    // 横の軸の目盛り(左端・真ん中・右端のEV100)
    const float minEV = AutoExposure::kMinLog2Luminance + 3.0f;
    const float maxEV = AutoExposure::kMaxLog2Luminance + 3.0f;
    ImGui::Text("EV100 %.0f", minEV);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f - 20.0f);
    ImGui::Text("%.0f", (minEV + maxEV) * 0.5f);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
    ImGui::Text("%.0f", maxEV);

    // 端に溜まった割合(数えられる幅の外は端の箱に入る)
    if (total > 0)
    {
        const float darkPercent = 100.0f * pBins[0] / static_cast<float>(total);
        const float brightPercent = 100.0f * pBins[AutoExposure::kHistogramBinCount - 1] / static_cast<float>(total);
        ImGui::TextDisabled(u8"左端(暗すぎ) %.1f%%  右端(明るすぎ) %.1f%%", darkPercent, brightPercent);
    }
}

void MeasurePanel::DrawPixelInspector()
{
    ImGui::Checkbox(u8"ピクセルインスペクター", &m_isInspecting);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"マウスの下の画素の、トーンマッピングの前の値を読む(数フレーム遅れる)。\nImGuiのウィンドウの上では読まない。");
    }
    if (!m_isInspecting)
    {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const bool isOverScene = !io.WantCaptureMouse && io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f &&
        io.MousePos.x >= 0.0f && io.MousePos.y >= 0.0f && io.MousePos.x < io.DisplaySize.x && io.MousePos.y < io.DisplaySize.y;
    if (isOverScene)
    {
        PixelProbe::Instance().Request(io.MousePos.x / io.DisplaySize.x, io.MousePos.y / io.DisplaySize.y);
    }

    const PixelProbe::Result& result = PixelProbe::Instance().GetResult();
    if (!result.isValid)
    {
        ImGui::TextDisabled(u8"画面の上にマウスを置いてください");
        return;
    }
    // シーンのRTの値は露出を掛けた後。露出で割り戻すと絶対の値になる
    const float exposedLuminance = 0.2126f * result.red + 0.7152f * result.green + 0.0722f * result.blue;
    const float exposure = (std::max)(result.exposure, 1e-20f);
    const float absoluteLuminance = exposedLuminance / exposure;
    const float stopsFromMiddleGray = std::log2((std::max)(exposedLuminance, 1e-6f) / 0.18f);

    char text[512] = {};
    snprintf(text, sizeof(text),
        u8"画素 (%d, %d)\n"
        u8"露出の後の値(リニア)  R %.4f  G %.4f  B %.4f\n"
        u8"露出の後の輝度  %.4f  (18%%の灰色から %+.2f 段)\n"
        u8"絶対の輝度  %.4g cd/m2  (EV100 %.2f)",
        result.x, result.y, result.red, result.green, result.blue,
        exposedLuminance, stopsFromMiddleGray, absoluteLuminance, LuminanceToEV100(absoluteLuminance));
    ImGui::TextUnformatted(text);
    if (isOverScene)
    {
        ImGui::SetTooltip("%s", text);
    }
}

void MeasurePanel::DrawReferenceCompare()
{
    auto& compare = ReferenceCompare::Instance();
    ImGui::TextUnformatted(u8"リファレンス比較");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(u8"最終の表示の色(トーンマッピングとsRGBの後)を見本と比べる。\n"
            u8"画像ファイルの見本は、同じトーンマッピングで書き出したsRGBの絵(PNGなど)を使う。\n"
            u8"見本は画面いっぱいに引き伸ばすので、画面と同じ縦横の比の絵にする。");
    }
    if (GraphicsDevice::Instance().IsHDROutput())
    {
        ImGui::TextDisabled(u8"HDR出力のときは使えません");
        return;
    }

    int mode = static_cast<int>(compare.GetMode());
    ImGui::RadioButton(u8"しない", &mode, static_cast<int>(ReferenceCompare::Mode::Off));
    ImGui::SameLine();
    ImGui::RadioButton(u8"ワイプ", &mode, static_cast<int>(ReferenceCompare::Mode::Wipe));
    ImGui::SameLine();
    ImGui::RadioButton(u8"差分", &mode, static_cast<int>(ReferenceCompare::Mode::Difference));
    compare.SetMode(static_cast<ReferenceCompare::Mode>(mode));

    int source = static_cast<int>(compare.GetSource());
    ImGui::TextUnformatted(u8"見本");
    ImGui::SameLine();
    ImGui::RadioButton(u8"スナップショット", &source, static_cast<int>(ReferenceCompare::Source::Snapshot));
    ImGui::SameLine();
    ImGui::RadioButton(u8"画像ファイル", &source, static_cast<int>(ReferenceCompare::Source::ImageFile));
    compare.SetSource(static_cast<ReferenceCompare::Source>(source));

    if (compare.GetSource() == ReferenceCompare::Source::Snapshot)
    {
        if (ImGui::Button(u8"今の画面を撮る"))
        {
            compare.RequestSnapshot();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(compare.HasSnapshot() ? u8"撮ってあります" : u8"まだ撮っていません");
    }
    else
    {
        ImGui::SetNextItemWidth(-60.0f);
        ImGui::InputTextWithHint("##imagePath", u8"Asset/Reference/xxx.png", m_imagePath, sizeof(m_imagePath));
        ImGui::SameLine();
        if (ImGui::Button(u8"読む"))
        {
            if (!compare.LoadImageFile(m_imagePath))
            {
                Logger::Instance().AddLog(Logger::LogLevel::Warning, std::string(u8"[ReferenceCompare] 画像を読めませんでした: ") + m_imagePath);
            }
        }
        if (!compare.GetImageFilePath().empty())
        {
            ImGui::TextDisabled(compare.IsImageReady() ? u8"読み終わりました: %s" : u8"読んでいます: %s", compare.GetImageFilePath().c_str());
        }
    }

    if (compare.GetMode() == ReferenceCompare::Mode::Wipe)
    {
        float wipePosition = compare.GetWipePosition();
        if (ImGui::SliderFloat(u8"境目", &wipePosition, 0.0f, 1.0f, "%.2f"))
        {
            compare.SetWipePosition(wipePosition);
        }
        ImGui::TextDisabled(u8"境目より左が見本、右が今の画面");
    }
    else if (compare.GetMode() == ReferenceCompare::Mode::Difference)
    {
        float gain = compare.GetDifferenceGain();
        if (ImGui::SliderFloat(u8"差の強調", &gain, 1.0f, 64.0f, u8"%.1f倍", ImGuiSliderFlags_Logarithmic))
        {
            compare.SetDifferenceGain(gain);
        }
        ImGui::TextDisabled(u8"黒いところは見本と同じ。明るいほど違う");
    }
    if (compare.GetMode() != ReferenceCompare::Mode::Off && compare.GetReferenceSRVIndex() == -1)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), u8"見本がまだ無いので、比べていません");
    }
}
