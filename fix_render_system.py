import os

def update_rendersystem():
    path = r'c:\GitHub\DX12\Source\Framework\ECS\CompSystem\Systems\RenderSystem.h'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    # 1. Update lighting initialization
    target1 = '''		// Initialize light data (always set regardless of shadow pass)
		CBufferData::Light cbLight = {};
		cbLight.AmbientLight = Math::Vector3(0.3f, 0.3f, 0.3f);
		cbLight.DL_Dir = lightDir;
		cbLight.DL_Color = Math::Vector3(1.0f, 1.0f, 1.0f);
		cbLight.SL_Count = 0;'''

    rep1 = '''		// Initialize light data (always set regardless of shadow pass)
		CBufferData::Light cbLight = {};
		cbLight.AmbientLight = Math::Vector3(0.05f, 0.05f, 0.05f); // Dark ambient for horror
		cbLight.DL_Dir = lightDir;
		cbLight.DL_Color = Math::Vector3(0.1f, 0.1f, 0.15f); // Dark directional (moonlight)
		cbLight.SL_Count = 0;
        
		for (size_t i = 0; i < m_spotLights.size() && cbLight.SL_Count < 10; ++i) {
			cbLight.SL[cbLight.SL_Count] = m_spotLights[i];
			cbLight.SL_Count++;
		}
		m_spotLights.clear(); // Clear for next frame'''
    
    if "Dark ambient for horror" not in content:
        content = content.replace(target1, rep1)

    # 2. Add member variables & methods
    target2 = '''private:
	Entity m_cameraEntity = INVALID_ENTITY;
	Math::Vector3 m_lightDirection = Math::Vector3(0.5f, -1.0f, 0.5f);
};'''

    rep2 = '''public:
	void AddSpotLight(const CBufferData::SpotLight& sl) {
		m_spotLights.push_back(sl);
	}

private:
	Entity m_cameraEntity = INVALID_ENTITY;
	Math::Vector3 m_lightDirection = Math::Vector3(0.5f, -1.0f, 0.5f);
	std::vector<CBufferData::SpotLight> m_spotLights;
};'''
    
    if "AddSpotLight" not in content:
        content = content.replace(target2, rep2)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

if __name__ == '__main__':
    update_rendersystem()
