#pragma once
#include <d3dcompiler.h>
#include <wrl.h>
#include <string>
#include <cassert>

class ShaderCompiler
{
public:
	static Microsoft::WRL::ComPtr<ID3DBlob> CompileVS(const std::wstring& filePath, const std::string& entryPoint = "main", const std::string& profile = "vs_5_0");
	static Microsoft::WRL::ComPtr<ID3DBlob> CompilePS(const std::wstring& filePath, const std::string& entryPoint = "main", const std::string& profile = "ps_5_0");
	static Microsoft::WRL::ComPtr<ID3DBlob> CompileCS(const std::wstring& filePath, const std::string& entryPoint = "main", const std::string& profile = "cs_5_0");

private:
	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring& filePath, const std::string& entryPoint, const std::string& profile);
};
