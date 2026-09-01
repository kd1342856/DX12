#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <vector>

class ShaderCompiler
{
public:
	static void Initialize();
	static void Terminate();

	static Microsoft::WRL::ComPtr<IDxcBlob> CompileVS(const std::wstring& filePath, const std::wstring& entryPoint = L"main", const std::wstring& profile = L"vs_6_0");
	static Microsoft::WRL::ComPtr<IDxcBlob> CompilePS(const std::wstring& filePath, const std::wstring& entryPoint = L"main", const std::wstring& profile = L"ps_6_0");
	static Microsoft::WRL::ComPtr<IDxcBlob> CompileCS(const std::wstring& filePath, const std::wstring& entryPoint = L"main", const std::wstring& profile = L"cs_6_0");

	static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const std::wstring& entryPoint, const std::wstring& profile);

private:
	static Microsoft::WRL::ComPtr<IDxcUtils> s_pUtils;
	static Microsoft::WRL::ComPtr<IDxcCompiler3> s_pCompiler;
	static Microsoft::WRL::ComPtr<IDxcIncludeHandler> s_pIncludeHandler;
	static Microsoft::WRL::ComPtr<IDxcValidator> s_pValidator;
};