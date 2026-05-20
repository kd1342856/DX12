#pragma once

// =============================================
// System基盤
// SystemBase: 全Systemの基底クラス
// SystemManager: System登録・Entity Signature変更通知
// =============================================

class ECSCoordinator;

// System基底クラス
class SystemBase
{
public:
	virtual ~SystemBase() = default;

	// 毎フレーム呼ばれる更新処理
	virtual void Update() = 0;

	// このSystemが対象とするEntity一覧
	std::set<Entity> m_entities;

	// コーディネーターへの参照（Component取得用）
	ECSCoordinator* m_pCoordinator = nullptr;
};

// System管理クラス
class SystemManager
{
public:
	// Systemを登録
	template<typename T>
	std::shared_ptr<T> RegisterSystem()
	{
		const char* typeName = typeid(T).name();
		assert(m_systems.find(typeName) == m_systems.end() && "同じSystemは二重登録できません");
		auto system = std::make_shared<T>();
		m_systems.insert({ typeName, system });
		return system;
	}

	// SystemのSignatureをセット（どのComponentを要求するか）
	template<typename T>
	void SetSignature(Signature signature)
	{
		const char* typeName = typeid(T).name();
		assert(m_systems.find(typeName) != m_systems.end() && "未登録のSystem");
		m_signatures.insert({ typeName, signature });
	}

	// Entity破棄時に全Systemから除去
	void EntityDestroyed(Entity entity)
	{
		for (auto const& pair : m_systems)
		{
			pair.second->m_entities.erase(entity);
		}
	}

	// EntityのSignature変更時に各Systemのリストを更新
	void EntitySignatureChanged(Entity entity, Signature entitySignature)
	{
		for (auto const& pair : m_systems)
		{
			auto const& systemSignature = m_signatures[pair.first];
			// EntityのSignatureがSystemの要求を満たしてるか
			if ((entitySignature & systemSignature) == systemSignature)
			{
				pair.second->m_entities.insert(entity);
			}
			else
			{
				pair.second->m_entities.erase(entity);
			}
		}
	}

private:
	//	Signatureのマッピング
	std::unordered_map<const char*, Signature> m_signatures;
	//	Systemインスタンスのマッピング
	std::unordered_map<const char*, std::shared_ptr<SystemBase>> m_systems;
};
