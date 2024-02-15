#pragma once
#include "pch.h"

#include "Device.h"

#include <vulkan/vulkan_core.h>
#include "DescriptorSet.h"

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

		void Bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint);

		inline VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
		inline VkPipeline GetPipeline() const { return m_PipelineHandle; }

	public:
		struct GraphicsCreateInfo
		{
			std::vector<std::string> ShaderFilepaths;
			std::vector<VkVertexInputBindingDescription> BindingDesc;
			std::vector<VkVertexInputAttributeDescription> AttributeDesc;
			uint32_t Width;
			uint32_t Height;
			VkPrimitiveTopology Topology;
			VkCullModeFlags CullMode;
			bool DepthTestEnable;
			bool DepthClamp = false;
			bool BlendingEnable;
			std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
			VkPushConstantRange* PushConstants;
			VkRenderPass RenderPass;
			int ColorAttachmentCount = 1;

			const char* debugName = "";
		};

		struct ComputeCreateInfo
		{
			std::string ShaderFilepath;
			std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
			VkPushConstantRange* PushConstants;

			const char* debugName = "";
		};

		struct RayTracingCreateInfo
		{
			std::vector<std::string> RayGenShaderFilepaths;
			std::vector<std::string> MissShaderFilepaths;
			std::vector<std::string> HitShaderFilepaths;
			VkPushConstantRange* PushConstants;
			std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;

			const char* debugName = "";
		};

	private:

		struct PipelineConfigInfo
		{
			VkViewport Viewport;
			VkRect2D Scissor;
			VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo;
			VkPipelineRasterizationStateCreateInfo RasterizationInfo;
			VkPipelineMultisampleStateCreateInfo MultisampleInfo;
			std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachment;
			VkPipelineColorBlendStateCreateInfo ColorBlendInfo;
			VkPipelineDepthStencilStateCreateInfo DepthStencilInfo;
			VkPipelineLayout PipelineLayout = 0;
			VkRenderPass RenderPass = 0;
			uint32_t Subpass = 0;
			bool DepthClamp = false;
		};

		void CreatePipelineConfigInfo(PipelineConfigInfo& configInfo, uint32_t width, uint32_t height, VkPrimitiveTopology topology, VkCullModeFlags cullMode, bool depthTestEnable, bool blendingEnable, int colorAttachmentCount = 1);
		static std::vector<char> ReadFile(const std::string& filepath);

		void CreateShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
		void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetsLayouts, VkPushConstantRange* pushConstants);
	
		VkPipeline m_PipelineHandle = 0;
		VkPipelineLayout m_PipelineLayout = 0;

		bool m_Initialized = false;
	};

}