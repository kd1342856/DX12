#pragma once
#include <chrono>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

//=============================
//
// エディターのUndo/Redo(仕様書「6. Undo/Redo」)
//
//   フレームごとには記録せず、操作1回(ドラッグの開始から終了、入力欄のEnter、オンオフの切り替え)を1件にする。
//   各件は「どの対象(targetKey: PostProcessなど)の、どの値(fieldKey: VisitFieldsのキー)が、何から何に変わったか」を持つ。
//   まとめて変える操作(すべて初期値に戻す・プリセットの読み込み・A/Bの切り替え)は、変わった値をすべて1件にする。
//   戻すときは、対象ごとに登録した関数で値を書き戻す。シーンの物(ライトなど)も、対象を登録すれば同じ履歴に乗る。
//
//=============================
class EditHistory
{
public:
    struct FieldChange
    {
        std::string targetKey;
        std::string fieldKey;
        std::string label; // 知らせに出す表示名(UTF-8)
        nlohmann::json before;
        nlohmann::json after;
    };

    // 値を書き戻す関数。書けなかったら(戻す先が無ければ)false
    using ApplyFunction = std::function<bool(const std::string& fieldKey, const nlohmann::json& value)>;

    static EditHistory& Instance();

    void RegisterTarget(const std::string& targetKey, ApplyFunction apply);

    // 値1つの変更を1件として積む。前と後が同じなら積まない
    void PushChange(FieldChange change);
    // まとめて変える操作。before・afterはShaderSettingsIO::Captureの形({グループ: {キー: 値}})
    void PushSettingsDiff(const std::string& description, const nlohmann::json& before, const nlohmann::json& after);

    bool Undo();
    bool Redo();
    void Clear();
    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }

    // 毎フレーム、ImGuiのフレームの中で呼ぶ。ショートカット(Ctrl+Z・Ctrl+Y・Ctrl+Shift+Z)と、画面の下の知らせ
    void Update();

    // 値をJSONから知らせ用の文字列にする
    static std::string FormatJson(const nlohmann::json& value);

private:
    struct Command
    {
        std::string description; // 知らせに出す説明
        std::vector<FieldChange> changes;
    };

    EditHistory();
    void Push(Command command);
    // 書き戻す。書けなかった値の数を返す
    int ApplyCommand(const Command& command, bool isUndo);
    void ShowNotice(const std::string& text);

    static constexpr size_t kMaxCommands = 200;
    std::deque<Command> m_undoStack;
    std::vector<Command> m_redoStack;
    std::unordered_map<std::string, ApplyFunction> m_targets;

    std::string m_notice;
    std::chrono::steady_clock::time_point m_noticeTime;
};
