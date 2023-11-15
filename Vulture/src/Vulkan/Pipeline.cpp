#include "pch.h"
#include "Utility/Utility.h"

#include "Pipeline.h"

#include "Descriptors.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{

	Pipeline::~Pipeline()
	{
		vkDestroyPipeline(Device::GetDevice(), m_Pipeline, nullptr);
		vkDestroyPipelineLayout(Device::GetDevice(), m_PipelineLayout, nullptr);
	}

	std::vector<char> Pipeline::ReadFile(const std::string& filepath)
	{
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);    // ate goes to the end of the file so reading filesize is easier and binary avoids text transformation
		VL_CORE_ASSERT(file.is_open(), "failed to open file: " + filepath);

		uint32_t fileSize = (uint32_t)(file.tellg());    // tellg gets current position in file
		std::vector<char> buffer(fileSize);
		file.seekg(0);    // return to the begining of the file
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}

	void Pipeline::CreateShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule)
	{
		VkShaderModuleCreateInfo createInfo{};

		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VL_CORE_RETURN_ASSERT(vkCreateShaderModule(Device::GetDevice(), &createInfo, nullptr, shaderModule),
			VK_SUCCESS,
			"failed to create shader Module"
		);
	}

	void Pipeline::Bind(VkCommandBuffer commandBuffer)
	{
		if (m_PipelineType == PipelineType::Compute)
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
		else
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
	}

	PipelineConfigInfo Pipeline::CreatePipelineConfigInfo(PipelineConfigInfo& configInfo, uint32_t width, uint32_t height, VkPrimitiveTopology topology, VkCullModeFlags cullMode, bool depthTestEnable, bool blendingEnable)
	{
		configInfo.InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		configInfo.InputAssemblyInfo.topology = topology;
		if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) configInfo.InputAssemblyInfo.primitiveRestartEnable = VK_TRUE;
		else configInfo.InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		configInfo.Viewport.x = 0.0f;
		configInfo.Viewport.y = 0.0f;
		configInfo.Viewport.width = static_cast<float>(width);
		configInfo.Viewport.height = static_cast<float>(height);
		configInfo.Viewport.minDepth = 0.0f;
		configInfo.Viewport.maxDepth = 1.0f;

		configInfo.Scissor.offset = { 0, 0 };
		configInfo.Scissor.extent = { width, height };

		configInfo.RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		configInfo.RasterizationInfo.depthClampEnable = configInfo.DepthClamp;
		configInfo.RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
		configInfo.RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
		configInfo.RasterizationInfo.lineWidth = 1.0f;
		configInfo.RasterizationInfo.cullMode = cullMode;
		configInfo.RasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		configInfo.RasterizationInfo.depthBiasEnable = VK_FALSE;

		configInfo.MultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		configInfo.MultisampleInfo.sampleShadingEnable = VK_FALSE;
		configInfo.MultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		configInfo.MultisampleInfo.minSampleShading = 1.0f;
		configInfo.MultisampleInfo.pSampleMask = nullptr;
		configInfo.MultisampleInfo.alphaToCoverageEnable = VK_FALSE;
		configInfo.MultisampleInfo.alphaToOneEnable = VK_FALSE;

		if (blendingEnable)
		{
			configInfo.ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			configInfo.ColorBlendAttachment.blendEnable = VK_TRUE;
			configInfo.ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			configInfo.ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			configInfo.ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			configInfo.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			configInfo.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			configInfo.ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		}
		else
		{
			configInfo.ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			configInfo.ColorBlendAttachment.blendEnable = VK_FALSE;
			configInfo.ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			configInfo.ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			configInfo.ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			configInfo.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			configInfo.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			configInfo.ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		}

		configInfo.ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		configInfo.ColorBlendInfo.logicOpEnable = VK_FALSE;
		configInfo.ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
		configInfo.ColorBlendInfo.attachmentCount = 1;
		configInfo.ColorBlendInfo.pAttachments = &configInfo.ColorBlendAttachment;
		configInfo.ColorBlendInfo.blendConstants[0] = 0.0f;
		configInfo.ColorBlendInfo.blendConstants[1] = 0.0f;
		configInfo.ColorBlendInfo.blendConstants[2] = 0.0f;
		configInfo.ColorBlendInfo.blendConstants[3] = 0.0f;

		configInfo.DepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		configInfo.DepthStencilInfo.depthTestEnable = depthTestEnable;
		configInfo.DepthStencilInfo.depthWriteEnable = VK_TRUE;
		configInfo.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		configInfo.DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
		configInfo.DepthStencilInfo.minDepthBounds = 0.0f;
		configInfo.DepthStencilInfo.maxDepthBounds = 1.0f;
		configInfo.DepthStencilInfo.stencilTestEnable = VK_FALSE;
		configInfo.DepthStencilInfo.front = {};
		configInfo.DepthStencilInfo.back = {};

		return configInfo;
	}

	enum class ShaderType
	{
		Vertex,
		Fragment,
		Compute
	};

	struct ShaderModule
	{
		VkShaderModule Module;
		VkShaderStageFlagBits Type;
	};

	void Pipeline::CreatePipeline(PipelineCreateInfo& info)
	{
		if (m_Pipeline)
		{
			vkDestroyPipeline(Device::GetDevice(), m_Pipeline, nullptr);
			vkDestroyPipelineLayout(Device::GetDevice(), m_PipelineLayout, nullptr);
		}
		CreatePipelineLayout(info.UniformSetLayouts, info.PushConstants);
		PipelineConfigInfo configInfo{};
		configInfo.RenderPass = info.RenderPass;
		configInfo.DepthClamp = info.DepthClamp;
		CreatePipelineConfigInfo(configInfo, info.Width, info.Height, info.Topology, info.CullMode, info.DepthTestEnable, info.BlendingEnable);

		std::vector<ShaderModule> shaderModules;
		shaderModules.resize(info.ShaderFilepaths.size());
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		for (int i = 0; i < info.ShaderFilepaths.size(); i++)
		{
			auto code = ReadFile(info.ShaderFilepaths[i]);

			if (info.ShaderFilepaths[i].rfind(".vert") != std::string::npos)
			{
				shaderModules[i].Type = VK_SHADER_STAGE_VERTEX_BIT;
				CreateShaderModule(code, &shaderModules[i].Module);
				if (m_PipelineType != PipelineType::Compute)
					m_PipelineType = PipelineType::Graphics;
				else
					VL_CORE_ASSERT(false, "Can't create compute pipeline with graphics shader!");
			}
			else if (info.ShaderFilepaths[i].rfind(".frag") != std::string::npos)
			{
				shaderModules[i].Type = VK_SHADER_STAGE_FRAGMENT_BIT;
				CreateShaderModule(code, &shaderModules[i].Module);
				if (m_PipelineType != PipelineType::Compute)
					m_PipelineType = PipelineType::Graphics;
				else
					VL_CORE_ASSERT(false, "Can't create compute pipeline with graphics shader!");
			}
			else if (info.ShaderFilepaths[i].rfind(".comp") != std::string::npos)
			{
				shaderModules[i].Type = VK_SHADER_STAGE_COMPUTE_BIT;
				CreateShaderModule(code, &shaderModules[i].Module);
				if (m_PipelineType != PipelineType::Graphics)
					m_PipelineType = PipelineType::Compute;
				else
					VL_CORE_ASSERT(false, "Can't create graphics pipeline with compute shader!");
			}

			VkPipelineShaderStageCreateInfo stage;
			stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage.stage = shaderModules[i].Type;
			stage.module = shaderModules[i].Module;
			stage.pName = "main";
			stage.flags = 0;
			stage.pNext = nullptr;
			stage.pSpecializationInfo = nullptr;

			shaderStages.push_back(stage);
		}

		if (m_PipelineType == PipelineType::Compute && shaderModules.size() != 1)
			VL_CORE_ASSERT(false, "Can't have more than 1 shader in compute pipeline!");

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)info.AttributeDesc.size();
		vertexInputInfo.pVertexAttributeDescriptions = info.AttributeDesc.data();
		vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)info.BindingDesc.size();
		vertexInputInfo.pVertexBindingDescriptions = info.BindingDesc.data();

		VkPipelineViewportStateCreateInfo viewportInfo{};
		viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportInfo.viewportCount = 1;
		viewportInfo.pViewports = &configInfo.Viewport;
		viewportInfo.scissorCount = 1;
		viewportInfo.pScissors = &configInfo.Scissor;

		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = 2;
		dynamicStateInfo.pDynamicStates = dynamicStates;

		VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {};
		VkComputePipelineCreateInfo computePipelineInfo = {};

		if (m_PipelineType == PipelineType::Graphics)
		{
			graphicsPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			graphicsPipelineInfo.stageCount = 2;
			graphicsPipelineInfo.pStages = shaderStages.data();
			graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
			graphicsPipelineInfo.pInputAssemblyState = &configInfo.InputAssemblyInfo;
			graphicsPipelineInfo.pViewportState = &viewportInfo;
			graphicsPipelineInfo.pRasterizationState = &configInfo.RasterizationInfo;
			graphicsPipelineInfo.pMultisampleState = &configInfo.MultisampleInfo;
			graphicsPipelineInfo.pColorBlendState = &configInfo.ColorBlendInfo;
			graphicsPipelineInfo.pDynamicState = &dynamicStateInfo;
			graphicsPipelineInfo.pDepthStencilState = &configInfo.DepthStencilInfo;

			graphicsPipelineInfo.layout = m_PipelineLayout;
			graphicsPipelineInfo.renderPass = configInfo.RenderPass;
			graphicsPipelineInfo.subpass = configInfo.Subpass;

			graphicsPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
			graphicsPipelineInfo.basePipelineIndex = -1;
		}

		if (m_PipelineType == PipelineType::Compute)
		{
			computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			computePipelineInfo.layout = m_PipelineLayout;
			computePipelineInfo.stage = shaderStages[0];
		}

		switch (m_PipelineType)
		{
		case PipelineType::Graphics:
			VL_CORE_RETURN_ASSERT(vkCreateGraphicsPipelines(Device::GetDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &m_Pipeline),
				VK_SUCCESS, "failed to create graphics pipeline!");
			break;
		case PipelineType::Compute:
			VL_CORE_RETURN_ASSERT(vkCreateComputePipelines(Device::GetDevice(), VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_Pipeline),
				VK_SUCCESS,
				"failed to create graphics pipeline!"
			);
			break;
		case PipelineType::Undefined:
			break;
		default:
			break;
		}

		for (int i = 0; i < shaderModules.size(); i++)
		{
			vkDestroyShaderModule(Device::GetDevice(), shaderModules[i].Module, nullptr);
		}
	}

	void Pipeline::CreatePipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSetsLayouts, VkPushConstantRange* pushConstants)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = descriptorSetsLayouts.empty() ? 0 : (uint32_t)descriptorSetsLayouts.size();
		pipelineLayoutInfo.pSetLayouts = descriptorSetsLayouts.empty() ? nullptr : descriptorSetsLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = (pushConstants == nullptr) ? 0 : 1;
		pipelineLayoutInfo.pPushConstantRanges = pushConstants;
		VL_CORE_RETURN_ASSERT(vkCreatePipelineLayout(Device::GetDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout),
			VK_SUCCESS, 
			"failed to create pipeline layout!"
		);
	}

}