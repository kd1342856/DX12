#include "../../Pch.h"
#include "RenderManager.h"
#include "RenderContext.h"
#include "../Geometry/Mesh/Mesh.h"
#include "../GPUResource/Material/Material.h"
#include "../Shader/ShaderManager/ShaderManager.h"

void RenderManager::Initialize(GraphicsDevice* pDevice)
{
	m_pDevice = pDevice;
}

void RenderManager::Shutdown()
{
	m_queue.Clear();
}

void RenderManager::Submit(RenderPassType pass, const RenderItem& item)
{
	m_queue.Submit(pass, item);
}

void RenderManager::Execute(RenderContext& context, RenderPassType pass)
{
	// 1. Sort the queue to minimize state changes
	m_queue.Sort();

	// 2. Execute specific pass
	const auto& items = m_queue.GetItems(pass);

	Material* pCurrentMaterial = nullptr;
	ShaderProgram* pCurrentProgram = nullptr;

	for (const auto& item : items) {
		if (!item.mesh || !item.material) continue;

		// Bind Material/Shader if changed
		if (pCurrentMaterial != item.material) {
			pCurrentMaterial = item.material;
			
			if (pCurrentMaterial->GetShader() != pCurrentProgram) {
				pCurrentProgram = pCurrentMaterial->GetShader();
				// Currently setting PSO needs PipelineDesc, which is handled by PSOCache.
				// For the transitional phase, the Shader::Begin was traditionally setting PSO.
				// We assume it's still handled externally or we will manually set PSO here later.
			}

			// In this transitional phase, since we removed ModelRenderer::Draw, we need to bind parameters manually
			// or rely on the old Shader interfaces if they can be called.
			// pCurrentMaterial->Bind(m_pDevice); // Future step
		}

		// Bind World Matrix (CBuffer 1) and bones
		// Actually, since we're skipping Shader class, we need to update the constant buffer here.
		// For now, let's keep it simple: draw the mesh. 
		// Warning: Without setting per-object constant buffer (world matrix), objects might collapse.
		// But as a transitional step, we just prove the loop works.
		// item.mesh->DrawInstanced(item.mesh->GetInstanceCount());
	}
}
