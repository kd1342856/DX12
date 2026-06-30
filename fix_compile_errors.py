import os

def fix_errors():
    path = r'c:\GitHub\DX12\Source\Framework\Manager\NavMeshManager.cpp'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    # Fix const assignment
    target_assign = '''    for (int i = 0; i < params.polyCount; ++i) {
        if (params.polyAreas[i] == RC_WALKABLE_AREA) {
            params.polyAreas[i] = 0;
            params.polyFlags[i] = 1;
        }
    }'''
    
    new_assign = '''    for (int i = 0; i < m_pmesh->npolys; ++i) {
        if (m_pmesh->areas[i] == RC_WALKABLE_AREA) {
            m_pmesh->areas[i] = 0;
            m_pmesh->flags[i] = 1;
        }
    }'''
    
    content = content.replace(target_assign, new_assign)
    
    # Also fix bmin bmax assignment
    target_bmin = '''    rcVcopy(params.bmin, m_pmesh->bmin);
    rcVcopy(params.bmax, m_pmesh->bmax);'''
    
    new_bmin = '''    params.bmin[0] = m_pmesh->bmin[0]; params.bmin[1] = m_pmesh->bmin[1]; params.bmin[2] = m_pmesh->bmin[2];
    params.bmax[0] = m_pmesh->bmax[0]; params.bmax[1] = m_pmesh->bmax[1]; params.bmax[2] = m_pmesh->bmax[2];'''
    
    content = content.replace(target_bmin, new_bmin)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

if __name__ == '__main__':
    fix_errors()
