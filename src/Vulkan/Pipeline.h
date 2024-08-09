// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "pch.h"

#include "Device.h"

#include <vulkan/vulkan_core.h>
#include "DescriptorSet.h"

#include "Shader.h"

namespace Vulture
{
	class Pipeline
	{
	public:
		struct GraphicsCreateInfo;
		struct ComputeCreateInfo;
		struct RayTracingCreateInfo;

		void Init(const GraphicsCreateInfo& info);
		void Init(const ComputeCreateInfo& info);
		void Init(const RayTracingCreateInfo& info);
		void Destroy();

		Pipeline() = default;
		Pipeline(const GraphicsCreateInfo& info);
		Pipeline(const ComputeCreateInfo& info);
		Pipeline(const RayTracingCreateInfo& info);
		~Pipeline();

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		Pipeline(Pipeline&& other) noexcept;
		Pipeline& operator=(Pipeline&& other) noexcept;

		void Bind(VkCommandBuffer commandBuffer);

		inline VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
		inline VkPipeline GetPipeline() const { return m_PipelineHandle; }

		inline bool IsInitialized() const { return m_Initialized; }

	public:
		struct GraphicsCreateInfo
		{
			std::vector<Shader*> Shaders;
			std::vector<VkVertexInputBindingDescription> BindingDesc;
			std::vector<VkVertexInputAttributeDescription> AttributeDesc;
			VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;
			uint32_t Width = 0;
			uint32_t Height = 0;
			VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			VkCullModeFlags CullMode = VK_CULL_MODE_NONE;
			bool DepthTestEnable = false;
			bool DepthClamp = false;
			bool BlendingEnable = false;
			std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
			VkPushConstantRange* PushConstants = nullptr;
			VkRenderPass RenderPass = VK_NULL_HANDLE;
			int ColorAttachmentCount = 1;

			const char* debugName = "";
		};

		struct ComputeCreateInfo
		{
			Vulture::Shader* Shader = nullptr;
			std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
			VkPushConstantRange* PushConstants = nullptr;

			const char* debugName = "";
		};

		struct RayTracingCreateInfo
		{
			std::vector<Shader*> RayGenShaders;
			std::vector<Shader*> MissShaders;
			std::vector<Shader*> HitShaders;
			VkPushConstantRange* PushConstants = nullptr;
			std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;

			const char* debugName = "";
		};

	private:

		struct PipelineConfigInfo
		{
			VkViewport Viewport = {};
			VkRect2D Scissor = { 0, 0 };
			VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo = {};
			VkPipelineRasterizationStateCreateInfo RasterizationInfo = {};
			VkPipelineMultisampleStateCreateInfo MultisampleInfo = {};
			std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachment;
			VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {};
			VkPipelineDepthStencilStateCreateInfo DepthStencilInfo = {};
			VkPipelineLayout PipelineLayout = 0;
			uint32_t Subpass = 0;
			bool DepthClamp = false;
		};

		void CreatePipelineConfigInfo(PipelineConfigInfo& configInfo, uint32_t width, uint32_t height, VkPolygonMode polyMode, VkPrimitiveTopology topology, VkCullModeFlags cullMode, bool depthTestEnable, bool blendingEnable, int colorAttachmentCount = 1);
		static std::vector<char> ReadFile(const std::string& filepath);

		void CreateShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
		void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetsLayouts, VkPushConstantRange* pushConstants);
	
		enum class PipelineType
		{
			Graphics,
			Compute,
			RayTracing,
			Undefined
		};

		VkPipeline m_PipelineHandle = 0;
		VkPipelineLayout m_PipelineLayout = 0;
		PipelineType m_PipelineType = PipelineType::Undefined;

		bool m_Initialized = false;

		void Reset();
	};

}