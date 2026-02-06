#pragma once
class Mesh;
class ModelData
{
public:
	struct Node
	{
		std::shared_ptr<Mesh> spMesh;
	};

	bool Load(const std::string& filepath);

	const std::vector<Node>& GetNodes() const { return m_nodes; }
private:
	std::vector<Node> m_nodes;
};