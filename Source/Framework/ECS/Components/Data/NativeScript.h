#pragma once
class GameObject;

// Base class for all native C++ scripts
class NativeScript {
public:
    virtual ~NativeScript() = default;
    
    virtual void Awake() {}
    virtual void Start() {}
    virtual void Update(float deltaTime) {}
    virtual void PostUpdate() {}
    virtual void PreDraw() {}
    virtual void Draw() {}
    virtual void OnDestroy() {}
    
    // Collision callbacks
    virtual void OnCollisionEnter(GameObject* other) {}
    virtual void OnCollisionStay(GameObject* other) {}
    virtual void OnCollisionExit(GameObject* other) {}
    
    virtual void OnTriggerEnter(GameObject* other) {}
    virtual void OnTriggerStay(GameObject* other) {}
    virtual void OnTriggerExit(GameObject* other) {}

    // GUI and Serialization
    virtual void ImGuiUpdate() {}
    virtual void Serialize(nlohmann::json& out) const {}
    virtual void Deserialize(const nlohmann::json& in) {}

    void SetGameObject(GameObject* obj) { m_pGameObject = obj; }
    GameObject* GetGameObject() const { return m_pGameObject; }

protected:
    GameObject* m_pGameObject = nullptr;
};