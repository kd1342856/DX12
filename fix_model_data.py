import os

def fix_model_data():
    # Fix NavMeshManager.h
    path_h = r'c:\GitHub\DX12\Source\Framework\Manager\NavMeshManager.h'
    with open(path_h, 'r', encoding='cp932') as f:
        content_h = f.read()

    content_h = content_h.replace('class Model;', 'class ModelData;')
    content_h = content_h.replace('BuildNavMesh(std::shared_ptr<Model> stageModel', 'BuildNavMesh(std::shared_ptr<ModelData> stageModel')
    
    with open(path_h, 'w', encoding='cp932') as f:
        f.write(content_h)

    # Fix NavMeshManager.cpp
    path_cpp = r'c:\GitHub\DX12\Source\Framework\Manager\NavMeshManager.cpp'
    with open(path_cpp, 'r', encoding='cp932') as f:
        content_cpp = f.read()

    content_cpp = content_cpp.replace('BuildNavMesh(std::shared_ptr<Model> stageModel', 'BuildNavMesh(std::shared_ptr<ModelData> stageModel')

    target_loop = '''    for (auto& mesh : stageModel->GetMeshes()) {
        const auto& meshVerts = mesh->GetVertices();
        const auto& meshFaces = mesh->GetFaces();'''

    new_loop = '''    for (const auto& node : stageModel->GetNodes()) {
        for (const auto& mesh : node.meshes) {
            const auto& meshVerts = mesh->GetVertices();
            const auto& meshFaces = mesh->GetFaces();'''

    content_cpp = content_cpp.replace(target_loop, new_loop)
    
    # Wait, the braces are unclosed if I just replace that.
    # The original loop in cpp:
    #    for (auto& mesh : stageModel->GetMeshes()) {
    #        const auto& meshVerts = mesh->GetVertices();
    #        const auto& meshFaces = mesh->GetFaces();
    #
    #        int baseVertex = (int)(verts.size() / 3);
    # ...
    #        for (const auto& f : meshFaces) { ... }
    #    }
    # If I replace the start, it will just add a loop. I must close it properly!
    
    full_target_loop = '''    for (auto& mesh : stageModel->GetMeshes()) {
        const auto& meshVerts = mesh->GetVertices();
        const auto& meshFaces = mesh->GetFaces();

        int baseVertex = (int)(verts.size() / 3);

        for (const auto& v : meshVerts) {
            Math::Vector3 pos = Math::Vector3::Transform(v.Pos, worldTransform);
            verts.push_back(pos.x);
            verts.push_back(pos.y);
            verts.push_back(pos.z);
        }

        for (const auto& f : meshFaces) {
            tris.push_back(baseVertex + f.Idx[0]);
            tris.push_back(baseVertex + f.Idx[1]);
            tris.push_back(baseVertex + f.Idx[2]);
        }
    }'''
    
    full_new_loop = '''    for (const auto& node : stageModel->GetNodes()) {
        for (const auto& mesh : node.meshes) {
            const auto& meshVerts = mesh->GetVertices();
            const auto& meshFaces = mesh->GetFaces();

            int baseVertex = (int)(verts.size() / 3);

            for (const auto& v : meshVerts) {
                Math::Vector3 pos = Math::Vector3::Transform(v.Pos, worldTransform);
                verts.push_back(pos.x);
                verts.push_back(pos.y);
                verts.push_back(pos.z);
            }

            for (const auto& f : meshFaces) {
                tris.push_back(baseVertex + f.Idx[0]);
                tris.push_back(baseVertex + f.Idx[1]);
                tris.push_back(baseVertex + f.Idx[2]);
            }
        }
    }'''

    content_cpp = content_cpp.replace(full_target_loop, full_new_loop)

    with open(path_cpp, 'w', encoding='cp932') as f:
        f.write(content_cpp)

if __name__ == '__main__':
    fix_model_data()
