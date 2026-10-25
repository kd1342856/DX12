#include "../../../Pch.h"
#include "ShaderCompiler.h"

#pragma comment(lib, "dxcompiler.lib")

Microsoft::WRL::ComPtr<IDxcUtils>          ShaderCompiler::s_pUtils;
Microsoft::WRL::ComPtr<IDxcCompiler3>      ShaderCompiler::s_pCompiler;
Microsoft::WRL::ComPtr<IDxcIncludeHandler> ShaderCompiler::s_pIncludeHandler;
Microsoft::WRL::ComPtr<IDxcValidator>      ShaderCompiler::s_pValidator;

void ShaderCompiler::Initialize()
{
    if (s_pCompiler) return;
    HMODULE hDxc = LoadLibraryA("dxcompiler.dll");
    if (hDxc)
    {
        typedef HRESULT(__stdcall* DxcCreateInstanceProc)(REFCLSID, REFIID, LPVOID*);
        DxcCreateInstanceProc pfnDxcCreateInstance =
            (DxcCreateInstanceProc)GetProcAddress(hDxc, "DxcCreateInstance");
        if (pfnDxcCreateInstance)
        {
            pfnDxcCreateInstance(CLSID_DxcUtils,     IID_PPV_ARGS(&s_pUtils));
            pfnDxcCreateInstance(CLSID_DxcCompiler,  IID_PPV_ARGS(&s_pCompiler));
            pfnDxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&s_pValidator));
        }
    }
    else
    {
        DxcCreateInstance(CLSID_DxcUtils,     IID_PPV_ARGS(&s_pUtils));
        DxcCreateInstance(CLSID_DxcCompiler,  IID_PPV_ARGS(&s_pCompiler));
        DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&s_pValidator));
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
}

void ShaderCompiler::Terminate()
{
    s_pIncludeHandler.Reset();
    s_pCompiler.Reset();
    s_pUtils.Reset();
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileShader(
    const std::wstring& filePath,
    const std::wstring& entryPoint,
    const std::wstring& profile)
{
    Initialize();


    wchar_t absPath[MAX_PATH] = {};
    GetFullPathNameW(filePath.c_str(), MAX_PATH, absPath, nullptr);
    std::wstring absoluteFilePath = absPath;

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
        throw std::runtime_error("Shader file not found");
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

    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Qembed_debug");
    arguments.push_back(L"-Od");
    arguments.push_back(L"-HV");
    arguments.push_back(L"2021");

    OutputDebugStringW(shaderDir.c_str());
    OutputDebugStringW(L"\n");

    OutputDebugStringW(shaderCommon.c_str());
    OutputDebugStringW(L"\n");

    for (auto arg : arguments)
    {
        OutputDebugStringW(arg);
        OutputDebugStringW(L"\n");
    }


    Microsoft::WRL::ComPtr<IDxcResult> pResults;
    HRESULT hrCompile = s_pCompiler->Compile(
        &sourceBuffer,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        s_pIncludeHandler.Get(),
        IID_PPV_ARGS(&pResults));

    if (pResults)
    {
        HRESULT status = S_OK;
        pResults->GetStatus(&status);

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors;
        pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

        if (FAILED(status))
        {
            if (pErrors)
            {
                OutputDebugStringA(pErrors->GetStringPointer());
            }

            throw std::runtime_error("Shader compile failed");
        }

        // ¬Œ÷‚È‚çŒx‚¾‚¯•\Ž¦
        if (pErrors && pErrors->GetStringLength() > 0)
        {
            OutputDebugStringA(pErrors->GetStringPointer());
        }
    }

    if (FAILED(hrCompile))
        throw std::runtime_error("IDxcCompiler3::Compile HRESULT failed");

    if (!pResults) return nullptr;

    HRESULT status = S_OK;
    pResults->GetStatus(&status);
    if (FAILED(status))
        throw std::runtime_error("Shader compile status failed");

    Microsoft::WRL::ComPtr<IDxcBlob> pShader;
    pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr);

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
                pValResult->GetResult(&pShader);
        }
    }

    return pShader;
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