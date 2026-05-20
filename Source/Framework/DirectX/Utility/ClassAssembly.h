#pragma once

class ComponentBase;

class ClassAssembly {
public:
    static ClassAssembly& Instance() {
        static ClassAssembly instance;
        return instance;
    }

    void RegisterComponentType(const std::string& className, std::function<std::shared_ptr<ComponentBase>()> factory) {
        m_factories[className] = factory;
    }

    const std::unordered_map<std::string, std::function<std::shared_ptr<ComponentBase>()>>& GetFactories() const { return m_factories; }

    std::shared_ptr<ComponentBase> Create(const std::string& className) {
        auto it = m_factories.find(className);
        if (it != m_factories.end()) {
            return it->second();
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, std::function<std::shared_ptr<ComponentBase>()>> m_factories;
};

// コンポ�Eネント登録用マクロ (これをcpp等に書くと自動登録されめE
#define REGISTER_COMPONENT(className) \
class className##_Register { \
public: \
    className##_Register() { \
        ClassAssembly::Instance().RegisterComponentType(#className, []() { return std::make_shared<className>(); }); \
    } \
}; \
static className##_Register s_##className##_Register;