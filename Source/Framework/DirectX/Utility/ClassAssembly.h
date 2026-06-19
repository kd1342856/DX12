#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "../ECS/Components/Data/NativeScript.h"

class ClassAssembly {
public:
    static ClassAssembly& Instance();

    void RegisterComponentType(const std::string& className, std::function<std::shared_ptr<NativeScript>()> factory) {
        m_factories[className] = factory;
        m_registeredClasses.push_back(className);
    }

    std::shared_ptr<NativeScript> Create(const std::string& className) {
        auto it = m_factories.find(className);
        if (it != m_factories.end()) {
            return it->second();
        }
        return nullptr;
    }

    const std::vector<std::string>& GetRegisteredClasses() const { return m_registeredClasses; }

private:
    std::unordered_map<std::string, std::function<std::shared_ptr<NativeScript>()>> m_factories;
    std::vector<std::string> m_registeredClasses;
};

#define REGISTER_COMPONENT(Type) \
    class Type##_Register { \
    public: \
        Type##_Register() { \
            ClassAssembly::Instance().RegisterComponentType(#Type, []() { return std::make_shared<Type>(); }); \
        } \
    }; \
    static Type##_Register s_##Type##_Register;
