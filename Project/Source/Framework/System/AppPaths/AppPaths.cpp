#include "../../../Pch.h"
#include "AppPaths.h"

namespace
{
	std::filesystem::path GetExeDir()
	{
		wchar_t exePath[MAX_PATH];
		DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		if (length == 0 || length >= MAX_PATH)
		{
			std::error_code errorCode;
			return std::filesystem::current_path(errorCode);
		}
		return std::filesystem::path(exePath).parent_path();
	}
}

const AppPaths::Dirs& AppPaths::Resolve()
{
	static const Dirs s_dirs = []()
	{
		Dirs dirs;
		std::error_code errorCode;

		// exeの場所(<root>/x64/<Config>や配布用フォルダ)から親へたどり、
		// Project/Assetを持つフォルダをルートとみなす
		std::filesystem::path searchDir = GetExeDir();
		for (int depth = 0; depth < 8 && !searchDir.empty(); depth++)
		{
			if (std::filesystem::is_directory(searchDir / L"Project" / L"Asset", errorCode))
			{
				dirs.root = searchDir;
				dirs.project = searchDir / L"Project";
				return dirs;
			}

			std::filesystem::path parentDir = searchDir.parent_path();
			if (parentDir == searchDir)
			{
				break;
			}
			searchDir = parentDir;
		}

		// 見つからなければ、今のカレントディレクトリをルート兼プロジェクトとみなす(従来の動作)
		dirs.project = std::filesystem::current_path(errorCode);
		dirs.root = dirs.project;
		return dirs;
	}();
	return s_dirs;
}

void AppPaths::Initialize()
{
	std::error_code errorCode;
	std::filesystem::current_path(GetProjectDir(), errorCode);

	// dxcompiler.dllは内部でdxil.dllを名前だけで読み込むので、検索先にBinを加えておく
	SetDllDirectoryW(GetBinDir().c_str());
}

const std::filesystem::path& AppPaths::GetRootDir()
{
	return Resolve().root;
}

const std::filesystem::path& AppPaths::GetProjectDir()
{
	return Resolve().project;
}

std::filesystem::path AppPaths::GetBinDir()
{
	return GetProjectDir() / L"Bin";
}

std::filesystem::path AppPaths::GetLibraryDir()
{
	return GetRootDir() / L"Library";
}

std::filesystem::path AppPaths::GetSavedDir()
{
	return GetRootDir() / L"Saved";
}

std::filesystem::path AppPaths::GetSavedFilePath(const wchar_t* pFileName)
{
	std::filesystem::path savedDir = GetSavedDir();
	std::error_code errorCode;
	std::filesystem::create_directories(savedDir, errorCode);
	return savedDir / pFileName;
}

std::string AppPaths::ToUtf8(const std::filesystem::path& path)
{
	const std::wstring& widePath = path.native();
	int utf8Length = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (utf8Length <= 0)
	{
		return std::string();
	}
	std::string utf8Path(utf8Length - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, utf8Path.data(), utf8Length, nullptr, nullptr);
	return utf8Path;
}
