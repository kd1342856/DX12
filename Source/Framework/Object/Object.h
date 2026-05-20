#pragma once
#include <string>

class Object {
public:
    Object() {}
    virtual ~Object() {}

    void SetName(const std::string& name) { m_name = name; }
    const std::string& GetName() const { return m_name; }

    void SetActive(bool active) { m_isActive = active; }
    bool IsActive() const { return m_isActive; }

protected:
    std::string m_name = "GameObject";
    bool m_isActive = true;
};