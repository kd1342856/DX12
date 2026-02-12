#pragma once
// プリコンパイル済みヘッダー


//	基本
#pragma comment(lib, "winmm.lib")

#define NOMINMAX
#include<Windows.h>
#include<iostream>
#include<cassert>

#include<wrl/client.h>

//	STL
#include<map>
#include<unordered_map>
#include<unordered_set>
#include<string>
#include<array>
#include<vector>
#include<stack>
#include<list>
#include<iterator>
#include<queue>
#include<algorithm>
#include<memory>
#include<random>
#include<fstream>
#include<sstream>
#include<functional>
#include<thread>
#include<atomic>
#include<mutex>
#include<future>
#include<filesystem>
#include<chrono>
#include<bitset>
#include<set>
#include<typeinfo>

#define _USE_MATH_DEFINES
#include<math.h>

// Direct3D12

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include<d3d12.h>
#include<d3dcompiler.h>
#include<dxgi1_6.h>

#pragma comment(lib, "DirectXTK12.lib")
#include<SimpleMath.h>

//	DirectXTex
#pragma comment(lib, "DirectXTex.lib")
#include<DirectXTex.h>

#include"System/System.h"
#include"Graphics/Graphics.h"

//	GDFファサード(GraphicsDeviceの後に配置)
#include"System/GDF/GDF.h"

//	ECS
#include"ECS/ECS.h"
