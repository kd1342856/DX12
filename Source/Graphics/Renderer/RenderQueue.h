#pragma once
#include <vector>
#include <unordered_map>
#include "RenderItem.h"

enum class RenderPassType
{
	Shadow,
	DepthPrePass,
	Opaque,
	Transparent,
	PostProcess
};

class RenderQueue
{
public:
	void Submit(RenderPassType pass, const RenderItem& item);
	void Sort();
	void Clear();

	const std::vector<RenderItem>& GetItems(RenderPassType pass) const;

private:
	std::unordered_map<RenderPassType, std::vector<RenderItem>> m_items;
	std::vector<RenderItem> m_emptyItems;
};
