# DX12Framework

C++ / DirectX 12 で作るホラーゲーム用エンジン。Visual Studio（`DX12Framework.sln`）でビルドする。

- `Source/` … エンジンとゲームのC++コード
- `Asset/Shader/` … HLSLシェーダー（`.hlsl` / `.hlsli`）
- `Library/` … 外部ライブラリ（assimp、ImGui、recastnavigation など）。**編集・整形しない**

## コーディング規則

新しく書くコード・変更するコードは、必ず以下の規則に従うこと。

既存コードの書式は `.clang-format` で一括整形済み。命名は一括で変更しない。**触ったファイルから順に**規則に合わせて直す（Visual Studioの名前の変更機能を使い、宣言と全参照を一度に変える）。命名の変更と機能の変更は、可能ならコミットを分ける。

### 書式（`.clang-format` で自動整形）

- `if`・`for`・`while`・`do`・`else`・`switch`・関数・クラス・構造体・enum・namespace の `{` と `}` は、必ず単独の行に置く（Allman）
- 中身が1行でも括弧を省略しない。1行にまとめない

```cpp
// NG
if (pTex && pTex->IsReady()) {
    pTex->Set(t0);
}
if (!pMesh) return;

// OK
if (pTex && pTex->IsReady())
{
    pTex->Set(t0);
}
if (!pMesh)
{
    return;
}
```

- インデントはスペース4つ（タブは使わない）。`public:` などのアクセス指定子と `case` ラベルは1段浅く置く
- ポインタ・参照の記号は型の側に付ける（`Mesh* pMesh`、`const Matrix& world`）
- 書いたら `clang-format -i <ファイル>` で整形する（Visual Studioなら「ドキュメントのフォーマット」）
- HLSL（`Asset/Shader/`）は clang-format が正しく解釈できない（`: SV_Target` を次の行に送る、`[loop]` を `for` と同じ行にする）ため、`Asset/Shader/.clang-format` で自動整形を止めている。書式の規則はC++と同じで、手で合わせる。セマンティクスは関数宣言と同じ行（`float4 main(PSInput In) : SV_Target0`）、`[loop]`・`[unroll]` などの属性は単独の行に置く

### 命名（`.clang-tidy` の `readability-identifier-naming` で警告）

| 対象 | 規則 | 例 |
|---|---|---|
| ローカル変数・引数 | ローワーキャメルケース。1文字の名前は避ける | `meshCount`、`lightDir` |
| 1文字の例外 | 数行の単純なインデックスのループの `i`・`j` だけ許可。入れ子が深い、または本体が長いループは意味のある名前にする | `faceIndex`、`lightIndex` |
| メンバー変数 | `m_` ＋ ローワーキャメルケース | `m_frameCount` |
| ポインタの接頭辞 | 生ポインタ `p`、`std::unique_ptr` は `up`、`std::shared_ptr` は `sp`、`std::weak_ptr` は `wp`、`ComPtr` は `cp`。ローカル変数・引数にも付ける。メンバーなら `m_` の後ろに付ける | `pMesh`、`spModel`、`m_upDevice`、`m_cpSwapChain` |
| 関数・クラス・構造体・enum | パスカルケース（DirectX・ImGuiのAPIと読み味をそろえる） | `BeginFrame()`、`RenderTarget` |

```cpp
// NG
std::unique_ptr<Texture> m_spWhiteTex;
LightData m_LightData;
for (int k = 0; k < 4; k++) { /* 長い本体 */ }

// OK
std::unique_ptr<Texture> m_upWhiteTex;
LightData m_lightData;
for (int sampleIndex = 0; sampleIndex < 4; sampleIndex++)
{
    // 長い本体
}
```

### 命名（提案・ユーザー未確認）

以下は上の規則から延長して決めた**提案**で、まだユーザーが確認していない。新しく書くコードではこれに従ってよいが、既存コードをこの項目のためだけに書き換えないこと。変更になったらこの節を更新する。

| 対象 | 規則（提案） | 例 |
|---|---|---|
| 静的メンバー | `s_` ＋ ローワーキャメルケース | `s_isShuttingDown` |
| グローバル変数 | `g_` ＋ ローワーキャメルケース | `g_mainThreadId` |
| 定数（`constexpr`・`const` の名前付き定数） | `k` ＋ パスカルケース | `kMaxGPUScopesPerFrame` |
| データだけの構造体の公開フィールド（Settings、CBufferData など） | `m_` を付けずローワーキャメルケース | `enableSSR`、`bloomThreshold` |
| マクロ | 大文字のスネークケース | `PROFILE_GPU_SCOPE` |
| HLSL | cbufferの変数とリソースは `g_` ＋ ローワーキャメルケース。ローカル変数はC++と同じ規則 | `g_DL_Dir` → `g_dirLightDir` |

### コメントとメッセージの言語

- コメント、`assert` のメッセージ、ログ・例外・エラーのメッセージ（`OutputDebugString`、`crash.log`・`exception.log` への出力など）は**日本語**で書く。C++もHLSLも同じ
- 識別子（変数名・関数名など）は英語のまま。上の命名規則に従う
- 既存の英語のコメントやメッセージは一括で書き換えない。命名と同じく、触った箇所から順に日本語にする

```cpp
// NG
assert(Thread::IsMainThread() && "Texture::CreateGPU must be called on the main thread!");

// OK
// GPUリソースの生成はメインスレッドでしか行えない
assert(Thread::IsMainThread() && "Texture::CreateGPU はメインスレッドから呼ぶこと");
```

### ファイルの文字コード

- 既存のソースには ASCII、UTF-8、UTF-8（BOM付き）、Shift_JIS（CP932）が混ざっている。**既存ファイルの文字コードと改行コードを変えない**こと。Shift_JISのファイルに日本語を書くときはShift_JISのまま保存する
- 新しく作るファイルは UTF-8（BOM付き）にする（Visual StudioのMSVCがBOMなしUTF-8をShift_JISとして読むのを避けるため）
- 例外：ASCIIだけのファイル、またはBOMなしUTF-8のファイルに日本語を書くときは、UTF-8（BOM付き）で保存する。BOMがないとMSVCが日本語をShift_JISとして読み、文字列（`assert` のメッセージなど）が文字化けする
