import os

def fix_component_manager():
    path = r'c:\GitHub\DX12\Source\Framework\ECS\ComponentManager.h'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    # Add constructor to ComponentArray
    target_class = '''template<typename T>
class ComponentArray : public IComponentArray
{
public:
	// R|[lgǉ
	void InsertData(Entity entity, T component)'''

    rep_class = '''template<typename T>
class ComponentArray : public IComponentArray
{
public:
	ComponentArray() {
		m_componentArray.resize(MAX_ENTITIES);
	}

	// R|[lgǉ
	void InsertData(Entity entity, T component)'''
    
    if "m_componentArray.resize" not in content:
        content = content.replace(target_class, rep_class)

    # Change std::array to std::vector
    target_array = '''private:
	std::array<T, MAX_ENTITIES> m_componentArray{};'''
    
    rep_array = '''private:
	std::vector<T> m_componentArray;'''
    
    content = content.replace(target_array, rep_array)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

def fix_entity_h():
    path = r'c:\GitHub\DX12\Source\Framework\ECS\Entity\Entity.h'
    with open(path, 'r', encoding='cp932') as f:
        content = f.read()

    target = 'constexpr uint8_t MAX_COMPONENTS = 32;'
    rep = 'constexpr uint8_t MAX_COMPONENTS = 64;'
    
    content = content.replace(target, rep)

    with open(path, 'w', encoding='cp932') as f:
        f.write(content)

if __name__ == '__main__':
    fix_component_manager()
    fix_entity_h()
