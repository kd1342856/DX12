#pragma once
#include "RenderQueue.h"

class GraphicsDevice;
class RenderContext;

class RenderManager
{
public:
	static RenderManager& Instance()
	{
		static RenderManager instance;
		return instance;
	}

	void Initialize(GraphicsDevice* pDevice);
	void Shutdown();

	// Submits an item to the queue for the current frame
	void Submit(RenderPassType pass, const RenderItem& item);

	// Executes submitted render items for a specific pass.
	// In the transitional phase, this might call ModelRenderer internally or handle Draw.
	void Execute(RenderContext& context, RenderPassType pass);

	RenderQueue& GetQueue() { return m_queue; }

private:
	RenderManager() = default;
	~RenderManager() = default;

	RenderManager(const RenderManager&) = delete;
	RenderManager& operator=(const RenderManager&) = delete;

	GraphicsDevice* m_pDevice = nullptr;
	RenderQueue m_queue;
};

