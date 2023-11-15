#pragma once
#include "pch.h"

#include "Descriptors.h"
#include "Device.h"

#include <vulkan/vulkan_core.h>
#include "Uniform.h"

namespace Vulture
{

	struct PipelineConfigInfo
	{
		VkViewport Viewport;
		VkRect2D Scissor;
		VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo;
		VkPipelineRasterizationStateCreateInfo RasterizationInfo;
		VkPipelineMultisampleStateCreateInfo MultisampleInfo;
		VkPipelineColorBlendAttachmentState ColorBlendAttachment;
		VkPipelineColorBlendStateCreateInfo ColorBlendInfo;
		VkPipelineDepthStencilStateCreateInfo DepthStencilInfo;
		VkPipelineLayout PipelineLayout = 0;
		VkRenderPass RenderPass = 0;
		uint32_t Subpass = 0;
		bool DepthClamp = false;
	};

	enum class PipelineType
	{
		Graphics,
		Compute,
		Undefined
	};

	struct PipelineCreateInfo
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
		std::vector<VkDescriptorSetLayout> UniformSetLayouts;
		VkPushConstantRange* PushConstants;
		VkRenderPass RenderPass;
	};

	class Pipeline
	{
	public:
		Pipeline() {};
		~Pipeline();

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		void Bind(VkCommandBuffer commandBuffer);

		static PipelineConfigInfo CreatePipelineConfigInfo(PipelineConfigInfo& configInfo, uint32_t width, uint32_t height, VkPrimitiveTopology topology, VkCullModeFlags cullMode, bool depthTestEnable, bool blendingEnable);
		void CreatePipeline(PipelineCreateInfo& info);

		inline VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; }
		inline VkPipeline GetPipeline() { return m_Pipeline; }

	private:
		static std::vector<char> ReadFile(const std::string& filepath);

		void CreateShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
		void CreatePipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSetsLayouts, VkPushConstantRange* pushConstants);

		VkPipeline m_Pipeline = 0;
		VkPipelineLayout m_PipelineLayout = 0;

		PipelineType m_PipelineType = PipelineType::Undefined;
	};

}