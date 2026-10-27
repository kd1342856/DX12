#pragma once
#include <filesystem>
#include <string>

//=============================
//
// アプリのフォルダ構成からパスを解決する
//
//   <root>/Project/Asset  … 実行に必須なデータ
//   <root>/Library        … 外部ライブラリ(Library/Binのdxcompiler.dll・dxil.dllは実行に必須)
//   <root>/Saved          … ログ・ini・キャッシュなど、消しても再生成されるもの
//
// Visual Studioからの起動でも、<root>/x64/<Config>のexeを直接起動しても同じ場所を指す。
//
//=============================
class AppPaths
{
public:
	// 起動直後に1回呼ぶ。Projectフォルダをカレントディレクトリにし(既存の"Asset/..."の相対パスを
	// そのまま使えるようにするため)、DLLの検索先にLibrary/Binを加える。
	static void Initialize();

	static const std::filesystem::path& GetRootDir();
	static const std::filesystem::path& GetProjectDir();
	static std::filesystem::path GetBinDir();
	static std::filesystem::path GetLibraryDir();
	static std::filesystem::path GetSavedDir();

	// Saved直下のファイルのパス。Savedフォルダが無ければ作る。
	static std::filesystem::path GetSavedFilePath(const wchar_t* pFileName);

	// ImGui等、UTF-8のパス文字列を受け取るAPI向け
	static std::string ToUtf8(const std::filesystem::path& path);

private:
	struct Dirs
	{
		std::filesystem::path root;
		std::filesystem::path project;
	};

	static const Dirs& Resolve();
};
