#include <stdio.h>
#include "../../../Pch.h"
#include "ShaderCompiler.h"

Microsoft::WRL::ComPtr<ID3DBlob> ShaderCompiler::CompileShader(const std::wstring& filePath, const std::string& entryPoint, const std::string& profile)
{
	ID3DInclude* include = D3D_COMPILE_STANDARD_FILE_INCLUDE;
#ifdef _DEBUG
	UINT flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT flag = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3DCompileFromFile(filePath.c_str(), nullptr, include, entryPoint.c_str(), profile.c_str(), flag, 0, &shaderBlob, &errorBlob);
	
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
            MessageBoxA(nullptr, static_cast<char*>(errorBlob->GetBufferPointer()), "Shader Compile Error", MB_OK);
		}
        else
        {
            MessageBoxA(nullptr, "Shader file not found or empty", "Shader Compile Error", MB_OK);
        }
		assert(0 && "Shader Compilation Failed");
		return nullptr;
	}
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3DBlob> ShaderCompiler::CompileVS(const std::wstring& filePath, const std::string& entryPoint, const std::string& profile)
{
	return CompileShader(filePath, entryPoint, profile);
}

Microsoft::WRL::ComPtr<ID3DBlob> ShaderCompiler::CompilePS(const std::wstring& filePath, const std::string& entryPoint, const std::string& profile)
{
	return CompileShader(filePath, entryPoint, profile);
}

Microsoft::WRL::ComPtr<ID3DBlob> ShaderCompiler::CompileCS(const std::wstring& filePath, const std::string& entryPoint, const std::string& profile)
{
	return CompileShader(filePath, entryPoint, profile);
}



