import os

def fix_errors():
    path = r'c:\GitHub\DX12\Source\Framework\Manager\NavMeshManager.cpp'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    # Fix Pos to Position
    content = content.replace('v.Pos', 'v.Position')

    # Fix braces
    target_loop = '''        for (const auto& f : meshFaces) {
            tris.push_back(baseVertex + f.Idx[0]);
            tris.push_back(baseVertex + f.Idx[1]);
            tris.push_back(baseVertex + f.Idx[2]);
        }
    }

    if (verts.empty() || tris.empty()) return false;'''
    
    new_loop = '''        for (const auto& f : meshFaces) {
            tris.push_back(baseVertex + f.Idx[0]);
            tris.push_back(baseVertex + f.Idx[1]);
            tris.push_back(baseVertex + f.Idx[2]);
        }
        }
    }

    if (verts.empty() || tris.empty()) return false;'''
    content = content.replace(target_loop, new_loop)

    # Fix array assignment
    target_assign = '''    params.bmin = m_pmesh->bmin;
    params.bmax = m_pmesh->bmax;'''
    new_assign = '''    rcVcopy(params.bmin, m_pmesh->bmin);
    rcVcopy(params.bmax, m_pmesh->bmax);'''
    content = content.replace(target_assign, new_assign)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

if __name__ == '__main__':
    fix_errors()
