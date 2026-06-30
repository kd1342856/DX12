import os

def update_ghost_ai_h():
    path = r'c:\GitHub\DX12\Source\Application\Script\Character\Ghost\GhostAI.h'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    target = '    int m_animDead = 3;\n};'
    rep = '''    int m_animDead = 3;

    float m_detectionRadius = 15.0f;
    float m_loseRadius = 20.0f;
    float m_huntSpeed = 12.0f;
    std::vector<DirectX::SimpleMath::Vector3> m_currentPath;
    float m_pathUpdateTimer = 0.0f;
};'''
    if 'm_detectionRadius' not in content:
        content = content.replace(target, rep)
        with open(path, 'w', encoding='cp932') as f:
            f.write(content)

if __name__ == '__main__':
    update_ghost_ai_h()
