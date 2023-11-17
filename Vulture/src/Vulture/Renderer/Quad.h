#pragma once
#include "pch.h"
#include "Vulkan/Buffer.h"
#include "../Utility/Utility.h"
#include "glm/glm.hpp"

namespace Vulture
{
	class Quad
	{
	public:
		struct Vertex
		{
			glm::vec3 position;
			glm::vec2 texCoord;

			static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions();
			static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
		};

		Quad();
		~Quad();

		void Init();

		void Bind(VkCommandBuffer commandBuffer);
		void Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount = 1);
	private:
		void CreateVertexBuffer(const std::vector<Vertex>& vertices);
		void CreateIndexBuffer(const std::vector<uint32_t>& indices);

		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;
	};
}