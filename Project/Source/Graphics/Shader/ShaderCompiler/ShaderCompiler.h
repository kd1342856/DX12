#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <vector>

// DXCが出したエラー・警告の1件。ファイル名と行番号はDXCのメッセージ「ファイル:行:列: error: 本文」から取り出す
struct ShaderDiagnostic
{
	std::string filePath; // UTF-8
	int line = 0;         // 1から。位置の無いメッセージは0
	int column = 0;
	bool isError = true;  // falseなら警告
	std::string message;  // UTF-8
};

// 1つのシェーダーのコンパイル結果
struct ShaderCompileResult
{
	Microsoft::WRL::ComPtr<IDxcBlob> cpBlob; // 失敗したらnullptr
	// ソースとインクルードしたファイルの絶対パス。ホットリロードがこれらの更新を監視する
	std::vector<std::wstring> dependencies;
	std::vector<ShaderDiagnostic> diagnostics;
	std::string log; // DXCの出力そのもの(UTF-8)
	bool isFromCache = false;
};

class ShaderCompiler
{
public:
	static void Initialize();
	static void Terminate();

	// 失敗しても例外を投げず、エラーをoutResultに入れてfalseを返す。キャッシュの照合とコンパイルはCompileShaderと同じ
	static bool TryCompileShader(const std::wstring& filePath, const std::wstring& entryPoint, const std::wstring& profile, ShaderCompileResult& outResult);

	// DXCの出力をエラー・警告ごとに分ける
	static std::vector<ShaderDiagnostic> ParseDiagnostics(const std::string& log);

	// ファイルのパスを、比較に使える形(絶対パス・正規化・小文字)にする
	static std::wstring NormalizePath(const std::wstring& path);

	static Microsoft::WRL::ComPtr<IDxcBlob> CompileVS(const std::wstring& filePath, const std::wstring& entryPoint = L"main", const std::wstring& profile = L"vs_6_0");
	static Microsoft::WRL::ComPtr<IDxcBlob> CompilePS(const std::wstring& filePath, const std::wstring& entryPoint = L"main", const std::wstring& profile = L"ps_6_0");
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileCS(const std::wstring& filePath, const std::wstring& entryPoint = L"main", const std::wstring& profile = L"cs_6_0");

	// コンパイル結果(DXIL)は <root>/Saved/ShaderCache/DXIL にキャッシュする。
	// キーはコンパイラのバージョン・引数(-Dのマクロ定義も引数に含まれる)・ソースのハッシュで、
	// 読み込み時にインクルードしたファイルの中身も照合し、どれかが変わっていればコンパイルし直す。
	// 失敗したら例外を投げる(起動時の読み込み用)
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const std::wstring& entryPoint, const std::wstring& profile);

	// Initialize() loads dxcompiler.dll from Library/Bin at runtime, so other code must not call
	// DxcCreateInstance directly (that would need an import-time link to dxcompiler.dll).
	static IDxcUtils* GetUtils() { return s_pUtils.Get(); }

	// 64ビットのFNV-1aハッシュ。シェーダーとPSOのディスクキャッシュのキーに使う
	static constexpr uint64_t kHashSeed = 14695981039346656037ull;
	static uint64_t HashBytes(const void* pData, size_t size, uint64_t seed = kHashSeed);

	// 起動してからのキャッシュの当たり・外れの回数(ログ表示用)
	static int GetCacheHitCount() { return s_cacheHitCount; }
	static int GetCacheMissCount() { return s_cacheMissCount; }

private:
	static Microsoft::WRL::ComPtr<IDxcUtils> s_pUtils;
	static Microsoft::WRL::ComPtr<IDxcCompiler3> s_pCompiler;
	static Microsoft::WRL::ComPtr<IDxcIncludeHandler> s_pIncludeHandler;
	static Microsoft::WRL::ComPtr<IDxcValidator> s_pValidator;

	// コンパイラ(dxcompiler.dll)と検証器(dxil.dll)のバージョンのハッシュ。DXCを更新したらキャッシュを捨てる
	static uint64_t s_compilerVersionHash;
	static int s_cacheHitCount;
	static int s_cacheMissCount;
};
