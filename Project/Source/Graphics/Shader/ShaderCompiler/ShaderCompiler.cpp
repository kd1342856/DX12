#include "../../../Pch.h"
#include "ShaderCompiler.h"
#include "../../../Framework/System/AppPaths/AppPaths.h"
#include <regex>

Microsoft::WRL::ComPtr<IDxcUtils>          ShaderCompiler::s_pUtils;
Microsoft::WRL::ComPtr<IDxcCompiler3>      ShaderCompiler::s_pCompiler;
Microsoft::WRL::ComPtr<IDxcIncludeHandler> ShaderCompiler::s_pIncludeHandler;
Microsoft::WRL::ComPtr<IDxcValidator>      ShaderCompiler::s_pValidator;
uint64_t ShaderCompiler::s_compilerVersionHash = 0;
int ShaderCompiler::s_cacheHitCount = 0;
int ShaderCompiler::s_cacheMissCount = 0;

namespace
{
    // キャッシュファイルの形式を変えたらkDxilCacheVersionを上げる(古いキャッシュは読まれなくなる)
    constexpr uint32_t kDxilCacheMagic = 0x4C495844; // "DXIL"
    constexpr uint32_t kDxilCacheVersion = 1;

    struct IncludeRecord
    {
        std::wstring path;
        uint64_t contentHash = 0;
    };

    bool ReadFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& outBytes)
    {
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs.is_open())
        {
            return false;
        }
        const std::streamsize size = ifs.tellg();
        if (size < 0)
        {
            return false;
        }
        outBytes.resize(static_cast<size_t>(size));
        ifs.seekg(0);
        if (size > 0 && !ifs.read(reinterpret_cast<char*>(outBytes.data()), size))
        {
            return false;
        }
        return true;
    }

    // 一時ファイルに書いてから置き換える。書き込みの途中で終了しても、壊れたファイルを残さない
    bool WriteFileAtomic(const std::filesystem::path& path, const void* pData, size_t size)
    {
        std::filesystem::path tempPath = path;
        tempPath += L".tmp";
        {
            std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open())
            {
                return false;
            }
            ofs.write(static_cast<const char*>(pData), static_cast<std::streamsize>(size));
            if (!ofs)
            {
                return false;
            }
        }
        std::error_code errorCode;
        std::filesystem::rename(tempPath, path, errorCode);
        if (errorCode)
        {
            std::filesystem::remove(tempPath, errorCode);
            return false;
        }
        return true;
    }

    bool HashFile(const std::filesystem::path& path, uint64_t& outHash)
    {
        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(path, bytes))
        {
            return false;
        }
        outHash = ShaderCompiler::HashBytes(bytes.data(), bytes.size());
        return true;
    }

    std::filesystem::path GetCacheDir(const wchar_t* pSubDirName)
    {
        std::filesystem::path dir = AppPaths::GetSavedDir() / L"ShaderCache" / pSubDirName;
        std::error_code errorCode;
        std::filesystem::create_directories(dir, errorCode);
        return dir;
    }

    std::wstring ToHex(uint64_t value)
    {
        wchar_t buffer[17] = {};
        swprintf_s(buffer, L"%016llx", static_cast<unsigned long long>(value));
        return buffer;
    }

    // キャッシュを読み込む。形式が違う、またはインクルードしたファイルのどれかが変わっていればfalse。
    // 成功したら、記録してあったインクルードのパスをoutIncludesに入れる
    bool LoadDxilCache(const std::filesystem::path& path, std::vector<uint8_t>& outDxil, std::vector<std::wstring>& outIncludes)
    {
        outIncludes.clear();
        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(path, bytes))
        {
            return false;
        }

        size_t offset = 0;
        auto read = [&](void* pDst, size_t size)
        {
            if (offset + size > bytes.size())
            {
                return false;
            }
            memcpy(pDst, bytes.data() + offset, size);
            offset += size;
            return true;
        };

        uint32_t magic = 0;
        uint32_t version = 0;
        uint32_t includeCount = 0;
        if (!read(&magic, sizeof(magic)) || !read(&version, sizeof(version)) || !read(&includeCount, sizeof(includeCount)))
        {
            return false;
        }
        if (magic != kDxilCacheMagic || version != kDxilCacheVersion)
        {
            return false;
        }

        for (uint32_t includeIndex = 0; includeIndex < includeCount; ++includeIndex)
        {
            uint32_t pathLength = 0;
            if (!read(&pathLength, sizeof(pathLength)))
            {
                return false;
            }
            std::wstring includePath(pathLength, L'\0');
            uint64_t savedHash = 0;
            if (!read(includePath.data(), pathLength * sizeof(wchar_t)) || !read(&savedHash, sizeof(savedHash)))
            {
                return false;
            }
            uint64_t currentHash = 0;
            if (!HashFile(includePath, currentHash) || currentHash != savedHash)
            {
                return false;
            }
            outIncludes.push_back(std::move(includePath));
        }

        uint64_t dxilSize = 0;
        if (!read(&dxilSize, sizeof(dxilSize)) || dxilSize == 0 || offset + dxilSize != bytes.size())
        {
            return false;
        }
        outDxil.assign(bytes.begin() + offset, bytes.end());
        return true;
    }

    void SaveDxilCache(const std::filesystem::path& path, const std::vector<IncludeRecord>& includes, IDxcBlob* pDxil)
    {
        std::vector<uint8_t> bytes;
        auto write = [&](const void* pSrc, size_t size)
        {
            const uint8_t* pBytes = static_cast<const uint8_t*>(pSrc);
            bytes.insert(bytes.end(), pBytes, pBytes + size);
        };

        const uint32_t includeCount = static_cast<uint32_t>(includes.size());
        write(&kDxilCacheMagic, sizeof(kDxilCacheMagic));
        write(&kDxilCacheVersion, sizeof(kDxilCacheVersion));
        write(&includeCount, sizeof(includeCount));
        for (const IncludeRecord& include : includes)
        {
            const uint32_t pathLength = static_cast<uint32_t>(include.path.size());
            write(&pathLength, sizeof(pathLength));
            write(include.path.data(), pathLength * sizeof(wchar_t));
            write(&include.contentHash, sizeof(include.contentHash));
        }
        const uint64_t dxilSize = pDxil->GetBufferSize();
        write(&dxilSize, sizeof(dxilSize));
        write(pDxil->GetBufferPointer(), pDxil->GetBufferSize());

        WriteFileAtomic(path, bytes.data(), bytes.size());
    }

    // インクルードされたファイルを記録する。読み込みはDXC標準のハンドラーに任せ、成功したパスだけを控える
    class RecordingIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit RecordingIncludeHandler(IDxcIncludeHandler* pInner)
            : m_pInner(pInner)
        {
        }

        HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
        {
            HRESULT hr = m_pInner->LoadSource(pFilename, ppIncludeSource);
            if (SUCCEEDED(hr) && pFilename)
            {
                m_includePaths.insert(pFilename);
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (!ppvObject)
            {
                return E_POINTER;
            }
            if (riid == __uuidof(IDxcIncludeHandler) || riid == __uuidof(IUnknown))
            {
                *ppvObject = static_cast<IDxcIncludeHandler*>(this);
                return S_OK;
            }
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        // スタックに置き、コンパイルの間だけ使う。寿命は呼び出し側が持つので参照カウントは数えない
        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return 1;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            return 1;
        }

        const std::set<std::wstring>& GetIncludePaths() const
        {
            return m_includePaths;
        }

    private:
        IDxcIncludeHandler* m_pInner = nullptr;
        std::set<std::wstring> m_includePaths;
    };
}

uint64_t ShaderCompiler::HashBytes(const void* pData, size_t size, uint64_t seed)
{
    constexpr uint64_t kFnvPrime = 1099511628211ull;
    const uint8_t* pBytes = static_cast<const uint8_t*>(pData);
    uint64_t hash = seed;
    for (size_t byteIndex = 0; byteIndex < size; ++byteIndex)
    {
        hash ^= pBytes[byteIndex];
        hash *= kFnvPrime;
    }
    return hash;
}

void ShaderCompiler::Initialize()
{
    if (s_pCompiler) return;

    // dxcompiler.dllはリンク時に結び付けず(exeの隣に置く必要が無いように)、Library/Binから実行時に読み込む。
    // dxil.dllはdxcompiler.dllが名前で読み込むので、AppPaths::InitializeでLibrary/BinをDLLの検索先に加えてある。
    std::filesystem::path dxcompilerPath = AppPaths::GetBinDir() / L"dxcompiler.dll";
    HMODULE hDxc = LoadLibraryW(dxcompilerPath.c_str());
    if (!hDxc)
    {
        // Library/Binに無い場合は通常のDLL検索順(exeの隣など)で探す
        hDxc = LoadLibraryW(L"dxcompiler.dll");
    }
    if (!hDxc)
    {
        // 環境のコードページに左右されないよう、メッセージは英語にしておく
        std::wstring message = L"dxcompiler.dll was not found.\nPlace dxcompiler.dll and dxil.dll in:\n" + dxcompilerPath.parent_path().wstring();
        MessageBoxW(nullptr, message.c_str(), L"Shader Compiler", MB_OK | MB_ICONERROR);
        return;
    }

    typedef HRESULT(__stdcall* DxcCreateInstanceProc)(REFCLSID, REFIID, LPVOID*);
    DxcCreateInstanceProc pfnDxcCreateInstance =
        (DxcCreateInstanceProc)GetProcAddress(hDxc, "DxcCreateInstance");
    if (pfnDxcCreateInstance)
    {
        pfnDxcCreateInstance(CLSID_DxcUtils,     IID_PPV_ARGS(&s_pUtils));
        pfnDxcCreateInstance(CLSID_DxcCompiler,  IID_PPV_ARGS(&s_pCompiler));
        pfnDxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&s_pValidator));
    }

    if (s_pUtils)
    {
        HRESULT hr = s_pUtils->CreateDefaultIncludeHandler(&s_pIncludeHandler);
        if (FAILED(hr))
        {
            OutputDebugStringA("CreateDefaultIncludeHandler FAILED\n");
        }

        if (!s_pIncludeHandler)
        {
            OutputDebugStringA("IncludeHandler is nullptr\n");
        }

    }

    // DXCを入れ替えたら古いキャッシュを使わないよう、コンパイラと検証器のバージョンをキーに含める
    s_compilerVersionHash = kHashSeed;
    IUnknown* pVersionSources[] = { s_pCompiler.Get(), s_pValidator.Get() };
    for (IUnknown* pVersionSource : pVersionSources)
    {
        if (!pVersionSource)
        {
            continue;
        }
        Microsoft::WRL::ComPtr<IDxcVersionInfo> cpVersionInfo;
        if (SUCCEEDED(pVersionSource->QueryInterface(IID_PPV_ARGS(&cpVersionInfo))))
        {
            UINT32 major = 0;
            UINT32 minor = 0;
            cpVersionInfo->GetVersion(&major, &minor);
            s_compilerVersionHash = HashBytes(&major, sizeof(major), s_compilerVersionHash);
            s_compilerVersionHash = HashBytes(&minor, sizeof(minor), s_compilerVersionHash);
        }
        Microsoft::WRL::ComPtr<IDxcVersionInfo2> cpVersionInfo2;
        if (SUCCEEDED(pVersionSource->QueryInterface(IID_PPV_ARGS(&cpVersionInfo2))))
        {
            UINT32 commitCount = 0;
            char* pCommitHash = nullptr;
            if (SUCCEEDED(cpVersionInfo2->GetCommitInfo(&commitCount, &pCommitHash)))
            {
                s_compilerVersionHash = HashBytes(&commitCount, sizeof(commitCount), s_compilerVersionHash);
                if (pCommitHash)
                {
                    s_compilerVersionHash = HashBytes(pCommitHash, strlen(pCommitHash), s_compilerVersionHash);
                    CoTaskMemFree(pCommitHash);
                }
            }
        }
    }
}

void ShaderCompiler::Terminate()
{
    s_pIncludeHandler.Reset();
    s_pCompiler.Reset();
    s_pUtils.Reset();
}

std::wstring ShaderCompiler::NormalizePath(const std::wstring& path)
{
    std::error_code errorCode;
    std::filesystem::path absolutePath = std::filesystem::absolute(path, errorCode);
    if (errorCode)
    {
        absolutePath = path;
    }
    std::wstring normalized = absolutePath.lexically_normal().make_preferred().wstring();
    // Windowsのパスは大文字と小文字を区別しない
    for (wchar_t& ch : normalized)
    {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return normalized;
}

std::vector<ShaderDiagnostic> ShaderCompiler::ParseDiagnostics(const std::string& log)
{
    // DXCのメッセージは「C:\path\file.hlsl:12:5: error: 本文」の形。続く行(ソースの引用や^の印)は直前のメッセージに含める
    static const std::regex kHeaderPattern(R"(^(.+?):(\d+):(\d+): (fatal error|error|warning|note): (.*)$)");
    std::vector<ShaderDiagnostic> diagnostics;
    // 「In file included from a.hlsl:3:」はエラーの前に出る。どこからインクルードしたかとして、次のエラーに付ける
    std::string includeContext;
    std::istringstream stream(log);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.rfind("In file included from ", 0) == 0)
        {
            includeContext += "\n" + line;
            continue;
        }
        std::smatch match;
        if (std::regex_match(line, match, kHeaderPattern))
        {
            if (match[4] == "note" && !diagnostics.empty())
            {
                // noteは直前のエラーの補足(マクロの展開元など)
                diagnostics.back().message += "\n" + line;
                continue;
            }
            ShaderDiagnostic diagnostic;
            diagnostic.filePath = match[1];
            diagnostic.line = std::stoi(match[2]);
            diagnostic.column = std::stoi(match[3]);
            diagnostic.isError = (match[4] != "warning");
            diagnostic.message = std::string(match[5]) + includeContext;
            includeContext.clear();
            diagnostics.push_back(std::move(diagnostic));
        }
        else if (!diagnostics.empty())
        {
            if (!line.empty())
            {
                diagnostics.back().message += "\n" + line;
            }
        }
        else if (!line.empty())
        {
            // 位置の無いメッセージ(引数の誤りなど)
            ShaderDiagnostic diagnostic;
            diagnostic.message = line;
            diagnostics.push_back(std::move(diagnostic));
        }
    }
    return diagnostics;
}

bool ShaderCompiler::TryCompileShader(
    const std::wstring& filePath,
    const std::wstring& entryPoint,
    const std::wstring& profile,
    ShaderCompileResult& outResult)
{
    outResult = {};
    Initialize();
    if (!s_pCompiler || !s_pUtils)
    {
        ShaderDiagnostic diagnostic;
        diagnostic.message = u8"シェーダーコンパイラ(dxcompiler.dll)を読み込めていません";
        outResult.diagnostics.push_back(diagnostic);
        return false;
    }

    wchar_t absPath[MAX_PATH] = {};
    GetFullPathNameW(filePath.c_str(), MAX_PATH, absPath, nullptr);
    std::wstring absoluteFilePath = absPath;
    outResult.dependencies.push_back(absoluteFilePath);

    std::wstring shaderDir = absoluteFilePath.substr(
        0, absoluteFilePath.find_last_of(L"/\\"));

    std::wstring shaderCommon =
        shaderDir.substr(0, shaderDir.find_last_of(L"/\\"))
        + L"\\Common";

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSourceBlob;
    HRESULT hr = s_pUtils->LoadFile(absoluteFilePath.c_str(), nullptr, &pSourceBlob);
    if (FAILED(hr))
    {
        std::wstring msg = L"Shader file not found: ";
        msg += absoluteFilePath;
        msg += L"\n";
        OutputDebugStringW(msg.c_str());

        ShaderDiagnostic diagnostic;
        diagnostic.filePath = std::filesystem::path(absoluteFilePath).u8string();
        diagnostic.message = u8"シェーダーのファイルを開けません";
        outResult.diagnostics.push_back(diagnostic);
        return false;
    }

    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr      = pSourceBlob->GetBufferPointer();
    sourceBuffer.Size     = pSourceBlob->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    std::vector<LPCWSTR> arguments;



    arguments.push_back(absoluteFilePath.c_str());

    arguments.push_back(L"-E");
    arguments.push_back(entryPoint.c_str());
    arguments.push_back(L"-T");
    arguments.push_back(profile.c_str());

    arguments.push_back(L"-I");
    arguments.push_back(shaderCommon.c_str());

#ifdef _DEBUG
    // Debugはステップ実行できるよう最適化せず、デバッグ情報をDXILに埋め込む
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Qembed_debug");
    arguments.push_back(L"-Od");
#else
    // Releaseは最適化する。デバッグ情報(PIXのソース表示用)はDXILに埋め込まず、PDBとしてSaved/ShaderCache/PDBに書き出す。
    // -Fdに末尾が\のフォルダを渡すと、PDBの名前はDXCがハッシュから決め、DXILにはその名前だけが入る
    const std::filesystem::path pdbDir = GetCacheDir(L"PDB");
    const std::wstring pdbDirArg = pdbDir.wstring() + L"\\";
    arguments.push_back(L"-O3");
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Fd");
    arguments.push_back(pdbDirArg.c_str());
#endif
    arguments.push_back(L"-HV");
    arguments.push_back(L"2021");

    // キャッシュのキー。引数(マクロ定義を含む)とソースが同じなら、インクルードの照合の後にキャッシュを使う
    uint64_t cacheKey = HashBytes(&kDxilCacheVersion, sizeof(kDxilCacheVersion));
    cacheKey = HashBytes(&s_compilerVersionHash, sizeof(s_compilerVersionHash), cacheKey);
    for (LPCWSTR pArgument : arguments)
    {
        // 引数の区切りが分かるよう、終端の0も含める
        cacheKey = HashBytes(pArgument, (wcslen(pArgument) + 1) * sizeof(wchar_t), cacheKey);
    }
    cacheKey = HashBytes(sourceBuffer.Ptr, sourceBuffer.Size, cacheKey);

    const std::filesystem::path cachePath = GetCacheDir(L"DXIL") / (ToHex(cacheKey) + L".bin");
    std::vector<uint8_t> cachedDxil;
    std::vector<std::wstring> cachedIncludes;
    if (LoadDxilCache(cachePath, cachedDxil, cachedIncludes))
    {
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> cpCachedBlob;
        if (SUCCEEDED(s_pUtils->CreateBlob(cachedDxil.data(), static_cast<UINT32>(cachedDxil.size()), DXC_CP_ACP, &cpCachedBlob)))
        {
            ++s_cacheHitCount;
            outResult.cpBlob = cpCachedBlob;
            outResult.isFromCache = true;
            outResult.dependencies.insert(outResult.dependencies.end(), cachedIncludes.begin(), cachedIncludes.end());
            return true;
        }
    }
    ++s_cacheMissCount;

    OutputDebugStringW(shaderDir.c_str());
    OutputDebugStringW(L"\n");

    OutputDebugStringW(shaderCommon.c_str());
    OutputDebugStringW(L"\n");

    for (auto arg : arguments)
    {
        OutputDebugStringW(arg);
        OutputDebugStringW(L"\n");
    }


    RecordingIncludeHandler includeHandler(s_pIncludeHandler.Get());
    Microsoft::WRL::ComPtr<IDxcResult> pResults;
    HRESULT hrCompile = s_pCompiler->Compile(
        &sourceBuffer,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        &includeHandler,
        IID_PPV_ARGS(&pResults));

    // 失敗しても、それまでに読めたインクルードは監視の対象にする(インクルード先を直したら再コンパイルできるように)
    for (const std::wstring& includePath : includeHandler.GetIncludePaths())
    {
        outResult.dependencies.push_back(includePath);
    }

    if (FAILED(hrCompile) || !pResults)
    {
        ShaderDiagnostic diagnostic;
        diagnostic.filePath = std::filesystem::path(absoluteFilePath).u8string();
        diagnostic.message = u8"DXCの呼び出しに失敗しました";
        outResult.diagnostics.push_back(diagnostic);
        return false;
    }

    HRESULT status = S_OK;
    pResults->GetStatus(&status);

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    if (pErrors && pErrors->GetStringLength() > 0)
    {
        // 成功なら警告だけが入っている
        OutputDebugStringA(pErrors->GetStringPointer());
        outResult.log = pErrors->GetStringPointer();
        outResult.diagnostics = ParseDiagnostics(outResult.log);
    }

    if (FAILED(status))
    {
        if (outResult.diagnostics.empty())
        {
            ShaderDiagnostic diagnostic;
            diagnostic.filePath = std::filesystem::path(absoluteFilePath).u8string();
            diagnostic.message = u8"コンパイルに失敗しました(DXCがメッセージを返していません)";
            outResult.diagnostics.push_back(diagnostic);
        }
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> pShader;
    pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr);

    // 検証に失敗したDXILはキャッシュしない(PSOの作成で失敗するので、次回もコンパイルし直して原因を出す)
    bool isValidated = (s_pValidator == nullptr);
    if (pShader && s_pValidator)
    {
        Microsoft::WRL::ComPtr<IDxcOperationResult> pValResult;
        s_pValidator->Validate(
            pShader.Get(), DxcValidatorFlags_InPlaceEdit, &pValResult);
        if (pValResult)
        {
            HRESULT valStatus = S_OK;
            pValResult->GetStatus(&valStatus);
            if (SUCCEEDED(valStatus))
            {
                pValResult->GetResult(&pShader);
                isValidated = true;
            }
            else
            {
                ShaderDiagnostic diagnostic;
                diagnostic.filePath = std::filesystem::path(absoluteFilePath).u8string();
                diagnostic.message = u8"DXILの検証に失敗しました";
                Microsoft::WRL::ComPtr<IDxcBlobEncoding> cpValErrors;
                if (SUCCEEDED(pValResult->GetErrorBuffer(&cpValErrors)) && cpValErrors && cpValErrors->GetBufferSize() > 0)
                {
                    const char* pText = static_cast<const char*>(cpValErrors->GetBufferPointer());
                    diagnostic.message += "\n" + std::string(pText, strnlen(pText, cpValErrors->GetBufferSize()));
                }
                outResult.diagnostics.push_back(diagnostic);
            }
        }
    }

#ifndef _DEBUG
    Microsoft::WRL::ComPtr<IDxcBlob> cpPdb;
    Microsoft::WRL::ComPtr<IDxcBlobUtf16> cpPdbName;
    if (SUCCEEDED(pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&cpPdb), &cpPdbName)) && cpPdb && cpPdbName)
    {
        // 名前には-Fdのフォルダが付いて返ってくることがあるので、ファイル名だけを使う
        const std::filesystem::path pdbPath = pdbDir / std::filesystem::path(cpPdbName->GetStringPointer()).filename();
        WriteFileAtomic(pdbPath, cpPdb->GetBufferPointer(), cpPdb->GetBufferSize());
    }
#endif

    if (pShader && isValidated)
    {
        std::vector<IncludeRecord> includes;
        bool canCache = true;
        for (const std::wstring& includePath : includeHandler.GetIncludePaths())
        {
            IncludeRecord record;
            record.path = includePath;
            if (!HashFile(includePath, record.contentHash))
            {
                // 中身を照合できないインクルードがあると、変更を検出できないのでキャッシュしない
                canCache = false;
                break;
            }
            includes.push_back(std::move(record));
        }
        if (canCache)
        {
            SaveDxilCache(cachePath, includes, pShader.Get());
        }
    }

    // 検証に失敗したDXILも返す(起動時はこれまでどおり使い、PSOの作成で失敗させる)。ホットリロードはfalseを見て差し替えない
    outResult.cpBlob = pShader;
    return pShader != nullptr && isValidated;
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileShader(
    const std::wstring& filePath,
    const std::wstring& entryPoint,
    const std::wstring& profile)
{
    ShaderCompileResult result;
    if (TryCompileShader(filePath, entryPoint, profile, result))
    {
        return result.cpBlob;
    }
    if (result.cpBlob)
    {
        // 検証だけに失敗した。これまでどおりそのまま返す
        return result.cpBlob;
    }
    throw std::runtime_error("Shader compile failed");
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileVS(
    const std::wstring& filePath,
    const std::wstring& entryPoint,
    const std::wstring& profile)
{
    return CompileShader(filePath, entryPoint, profile);
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompilePS(
    const std::wstring& filePath,
    const std::wstring& entryPoint,
    const std::wstring& profile)
{
    return CompileShader(filePath, entryPoint, profile);
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileCS(
    const std::wstring& filePath,
    const std::wstring& entryPoint,
    const std::wstring& profile)
{
    return CompileShader(filePath, entryPoint, profile);
}
