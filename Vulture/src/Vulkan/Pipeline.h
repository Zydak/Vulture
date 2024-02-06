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
		struct CreateInfo;
		struct RayTracingCreateInfo;

		void Init(CreateInfo* info);
		void Init(RayTracingCreateInfo* info);
		void Destroy();

		Pipeline() = default;
		~Pipeline();

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		void Bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint);

		inline VkPipelineLayout GetPipelineLayout() const { return m_Info.PipelineLayout; }
		inline VkPipeline GetPipeline() const { return m_Info.Pipeline; }

	public:
		struct CreateInfo
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
		};

		enum class PipelineType
		{
			Graphics,
			Compute,
			Undefined
		};

		struct RayTracingCreateInfo
		{
			std::vector<std::string> RayGenShaderFilepaths;
			std::vector<std::string> MissShaderFilepaths;
			std::vector<std::string> HitShaderFilepaths;
			VkPushConstantRange* PushConstants;
			std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
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
		void CreatePipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSetsLayouts, VkPushConstantRange* pushConstants);
	
		struct Info
		{
			VkPipeline Pipeline = 0;
			VkPipelineLayout PipelineLayout = 0;

			Vulture::Pipeline::PipelineType PipelineType = PipelineType::Undefined;

			bool Initialized = false;
		};

		Info m_Info;
	};

}