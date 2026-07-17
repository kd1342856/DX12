#include "../../Pch.h"
#include "RenderQueue.h"
#include <algorithm>

void RenderQueue::Submit(RenderPassType pass, const RenderItem& item)
{
	m_items[pass].push_back(item);
}

void RenderQueue::Sort()
{
	for (auto& pair : m_items) {
		auto pass = pair.first;
		auto& items = pair.second;

		if (pass == RenderPassType::Transparent) {
			// For transparent objects, sort back-to-front. For now, we sort by sortKey descending if needed.
			// Actually, distance to camera should be encoded in the sortKey.
			std::sort(items.begin(), items.end(), [](const RenderItem& a, const RenderItem& b) {
				return a.sortKey > b.sortKey; 
			});
		}
		else {
			// For opaque objects, sort front-to-back or by PSO (encoded in sortKey ascending).
			std::sort(items.begin(), items.end(), [](const RenderItem& a, const RenderItem& b) {
				return a.sortKey < b.sortKey; 
			});
		}
	}
}

void RenderQueue::Clear()
{
	for (auto& pair : m_items) {
		pair.second.clear();
	}
}

const std::vector<RenderItem>& RenderQueue::GetItems(RenderPassType pass) const
{
	auto it = m_items.find(pass);
	if (it != m_items.end()) {
		return it->second;
	}
	return m_emptyItems;
}
