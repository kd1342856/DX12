import os

def fix_stuff():
    # Fix NavMeshManager.h (Encoding corruption)
    path_h = r'c:\GitHub\DX12\Source\Framework\Manager\NavMeshManager.h'
    content_h = '''#pragma once

#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "SimpleMath.h"

namespace Math = DirectX::SimpleMath;

struct rcHeightfield;
struct rcCompactHeightfield;
struct rcContourSet;
struct rcPolyMesh;
struct rcPolyMeshDetail;
class dtNavMesh;
class dtNavMeshQuery;
class dtQueryFilter;

class Model;

class NavMeshManager
{
public:
    static NavMeshManager& Instance();

    void Init();
    void Release();

    bool BuildNavMesh(std::shared_ptr<Model> stageModel, const Math::Matrix& worldTransform);

    bool FindPath(const Math::Vector3& start, const Math::Vector3& end, std::vector<Math::Vector3>& outPath);

    void DrawDebug();

private:
    NavMeshManager();
    ~NavMeshManager();

    rcHeightfield* m_solid = nullptr;
    rcCompactHeightfield* m_chf = nullptr;
    rcContourSet* m_cset = nullptr;
    rcPolyMesh* m_pmesh = nullptr;
    rcPolyMeshDetail* m_dmesh = nullptr;

    dtNavMesh* m_navMesh = nullptr;
    dtNavMeshQuery* m_navQuery = nullptr;
    dtQueryFilter* m_filter = nullptr;

    void CleanupRecast();
};'''
    with open(path_h, 'w', encoding='cp932') as f:
        f.write(content_h)

    # Fix GhostAI.cpp includes
    path_cpp = r'c:\GitHub\DX12\Source\Application\Script\Character\Ghost\GhostAI.cpp'
    with open(path_cpp, 'r', encoding='cp932') as f:
        content_cpp = f.read()

    include_str = '''#include "GhostAI.h"
#include "../../../../Framework/Manager/NavMeshManager.h"
#include "../../../../Framework/Manager/SceneManager.h"
#include "../../../../Framework/Manager/Scene.h"
#include "../../../Scene/TitleScene/TitleScene.h"'''
    
    if '#include "../../../../Framework/Manager/NavMeshManager.h"' not in content_cpp:
        content_cpp = content_cpp.replace('#include "GhostAI.h"', include_str)
        with open(path_cpp, 'w', encoding='cp932') as f:
            f.write(content_cpp)

if __name__ == '__main__':
    fix_stuff()
