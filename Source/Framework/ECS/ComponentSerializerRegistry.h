#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include "../../../Library/nlohmann/json.hpp"
#include "ECS.h"

class ComponentSerializerRegistry {
public:
    using DeserializeFn = std::function<void(ECSCoordinator&, Entity, const nlohmann::json&, class GameObject*)>;
    using SerializeFn = std::function<bool(ECSCoordinator&, Entity, nlohmann::json&, class GameObject*)>;

    static ComponentSerializerRegistry& Instance() {
        static ComponentSerializerRegistry instance;
        return instance;
    }

    void Register(const std::string& componentName, DeserializeFn deserialize, SerializeFn serialize) {
        m_deserializers[componentName] = std::move(deserialize);
        m_serializers[componentName] = std::move(serialize);
        // d•¡ŽÀs‚ð–h‚®‚½‚ßA³Ž®–¼Ì‚ð‹L˜^
        if (std::find(m_registeredNames.begin(), m_registeredNames.end(), componentName) == m_registeredNames.end()) {
            m_registeredNames.push_back(componentName);
        }
    }

    void RegisterAlias(const std::string& aliasName, const std::string& targetName) {
        if (m_deserializers.count(targetName)) {
            m_deserializers[aliasName] = m_deserializers[targetName];
        }
    }

    void DeserializeComponent(ECSCoordinator& ecs, Entity entity, const std::string& typeName, const nlohmann::json& j, class GameObject* obj) const {
        auto it = m_deserializers.find(typeName);
        if (it != m_deserializers.end()) {
            it->second(ecs, entity, j, obj);
        }
    }

    void SerializeAllComponents(ECSCoordinator& ecs, Entity entity, nlohmann::json& outComponentsArray, class GameObject* obj) const {
        for (const auto& name : m_registeredNames) {
            auto it = m_serializers.find(name);
            if (it != m_serializers.end()) {
                nlohmann::json compJson;
                if (it->second(ecs, entity, compJson, obj)) {
                    compJson["Type"] = name;
                    outComponentsArray.push_back(compJson);
                }
            }
        }
    }

private:
    ComponentSerializerRegistry() = default;
    std::unordered_map<std::string, DeserializeFn> m_deserializers;
    std::unordered_map<std::string, SerializeFn> m_serializers;
    std::vector<std::string> m_registeredNames;
};
