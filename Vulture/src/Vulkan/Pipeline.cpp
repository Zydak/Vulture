#include "pch.h"
#include "Utility/Utility.h"

#include "Pipeline.h"
#include <vulkan/vulkan_core.h>

namespace Vulture
{
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

	void Pipeline::Init(const GraphicsCreateInfo& info)
	{
		if (m_Initialized)
		{
			Destroy();
		}

		CreatePipelineLayout(info.DescriptorSetLayouts, info.PushConstants);
		PipelineConfigInfo configInfo{};
		configInfo.RenderPass = info.RenderPass;
		configInfo.DepthClamp = info.DepthClamp;
		CreatePipelineConfigInfo(configInfo, info.Width, info.Height, info.Topology, info.CullMode, info.DepthTestEnable, info.BlendingEnable, info.ColorAttachmentCount);

		std::vector<ShaderModule> shaderModules;
		shaderModules.resize(info.Shaders.size());
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		for (int i = 0; i < info.Shaders.size(); i++)
		{
			shaderStages.push_back(info.Shaders[i]->GetStageCreateInfo());
		}

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

		VL_CORE_RETURN_ASSERT(
			vkCreateGraphicsPipelines(Device::GetDevice(), VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &m_PipelineHandle),
			VK_SUCCESS,
			"failed to create graphics pipeline!"
		);

		if (info.debugName != "")
		{
			Device::SetObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_PipelineHandle, info.debugName);
		}

		m_Initialized = true;
	}

	void Pipeline::Init(const RayTracingCreateInfo& info)
	{
		if (m_Initialized)
		{
			Destroy();
		}

		// All stages
		int count = (int)info.RayGenShaders.size() + (int)info.MissShaders.size() + (int)info.HitShaders.size();
		std::vector<VkPipelineShaderStageCreateInfo> stages(count);
		VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		stage.pName = "main";  // All the same entry point
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

		int stageCount = 0;

		// Ray gen
		for (int i = 0; i < info.RayGenShaders.size(); i++)
		{
			stage.module = info.RayGenShaders[i]->GetModuleHandle();
			stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			stages[stageCount] = stage;
			stageCount++;
		}

		// Miss
		for (int i = 0; i < info.MissShaders.size(); i++)
		{
			stage.module = info.MissShaders[i]->GetModuleHandle();
			stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
			stages[stageCount] = stage;
			stageCount++;
		}

		// Hit Group - Closest Hit
		for (int i = 0; i < info.HitShaders.size(); i++)
		{
			stage.module = info.HitShaders[i]->GetModuleHandle();
			stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			stages[stageCount] = stage;
			stageCount++;
		}

		// Shader groups
		VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
		group.anyHitShader = VK_SHADER_UNUSED_KHR;
		group.closestHitShader = VK_SHADER_UNUSED_KHR;
		group.generalShader = VK_SHADER_UNUSED_KHR;
		group.intersectionShader = VK_SHADER_UNUSED_KHR;

		// Ray gen
		stageCount = 0;
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> rtShaderGroups;
		for (int i = 0; i < info.RayGenShaders.size(); i++)
		{
			group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			group.generalShader = stageCount;
			rtShaderGroups.push_back(group);
			stageCount++;
		}

		// Miss
		for (int i = 0; i < info.MissShaders.size(); i++)
		{
			group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			group.generalShader = stageCount;
			rtShaderGroups.push_back(group);
			stageCount++;
		}

		group.generalShader = VK_SHADER_UNUSED_KHR;

		// closest hit shader
		for (int i = 0; i < info.HitShaders.size(); i++)
		{
			group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			group.closestHitShader = stageCount;
			rtShaderGroups.push_back(group);
			stageCount++;
		}

		// create layout
		CreatePipelineLayout(info.DescriptorSetLayouts, info.PushConstants);

		// Assemble the shader stages and recursion depth info into the ray tracing pipeline
		VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
		rayPipelineInfo.stageCount = (uint32_t)stages.size();  // Stages are shaders
		rayPipelineInfo.pStages = stages.data();

		rayPipelineInfo.groupCount = (uint32_t)rtShaderGroups.size();
		rayPipelineInfo.pGroups = rtShaderGroups.data();

		rayPipelineInfo.maxPipelineRayRecursionDepth = 2;  // Ray depth
		rayPipelineInfo.layout = m_PipelineLayout;

		Device::vkCreateRayTracingPipelinesKHR(Device::GetDevice(), {}, {}, 1, &rayPipelineInfo, nullptr, &m_PipelineHandle);

		if (info.debugName != "")
		{
			Device::SetObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_PipelineHandle, info.debugName);
		}

		m_Initialized = true;
	}

	void Pipeline::Init(const ComputeCreateInfo& info)
	{
		if (m_Initialized)
		{
			Destroy();
		}

		CreatePipelineLayout(info.DescriptorSetLayouts, info.PushConstants);

		VkComputePipelineCreateInfo computePipelineInfo = {};

		computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineInfo.layout = m_PipelineLayout;
		computePipelineInfo.stage = info.Shader->GetStageCreateInfo();

		VL_CORE_RETURN_ASSERT(
			vkCreateComputePipelines(Device::GetDevice(), VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_PipelineHandle),
			VK_SUCCESS,
			"failed to create graphics pipeline!"
		);

		if (info.debugName != "")
		{
			Device::SetObjectName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_PipelineHandle, info.debugName);
		}

		m_Initialized = true;
	}

	void Pipeline::Destroy()
	{
		vkDestroyPipeline(Device::GetDevice(), m_PipelineHandle, nullptr);
		vkDestroyPipelineLayout(Device::GetDevice(), m_PipelineLayout, nullptr);
		m_Initialized = false;
	}

	Pipeline::~Pipeline()
	{
		if (m_Initialized)
			Destroy();
	}

	Pipeline::Pipeline(const GraphicsCreateInfo& info)
	{
		Init(info);
	}

	Pipeline::Pipeline(const ComputeCreateInfo& info)
	{
		Init(info);
	}

	Pipeline::Pipeline(const RayTracingCreateInfo& info)
	{
		Init(info);
	}

	/*
	 * @brief Reads the contents of a file into a vector of characters.
	 *
	 * @param filepath - The path to the file to be read.
	 * @return A vector of characters containing the file contents.
	 */
	std::vector<char> Pipeline::ReadFile(const std::string& filepath)
	{
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);    // ate goes to the end of the file so reading filesize is easier and binary avoids text transformation
		VL_CORE_ASSERT(file.is_open(), "failed to open file: " + filepath);

		uint32_t fileSize = (uint32_t)(file.tellg());    // tellg gets current position in file
		std::vector<char> buffer(fileSize);
		file.seekg(0);    // return to the beginning of the file
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}

	/*
	 * @brief Creates a Vulkan shader module from the given shader code.
	 *
	 * @param code - The vector of characters containing the shader code.
	 * @param shaderModule - Pointer to the Vulkan shader module to be created.
	 */
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

	/*
	 * @brief Binds the pipeline to the given Vulkan command buffer.
	 *
	 * @param commandBuffer - The Vulkan command buffer to which the pipeline is bound.
	 */
	void Pipeline::Bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint)
	{
		vkCmdBindPipeline(commandBuffer, bindPoint, m_PipelineHandle);
	}

	/*
	 * @brief Creates a Vulkan pipeline layout using the specified descriptor set layouts and push constant ranges.
	 *
	 * @param descriptorSetsLayouts - The descriptor set layouts to be used in the pipeline layout.
	 * @param pushConstants - The push constant ranges to be used in the pipeline layout.
	 */
	void Pipeline::CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetsLayouts, VkPushConstantRange* pushConstants)
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

	/**
	 * @brief Initializes a Vulkan pipeline configuration based on the specified parameters.
	 *
	 * @param configInfo - The pipeline configuration information structure to be initialized.
	 * @param width - The width of the viewport.
	 * @param height - The height of the viewport.
	 * @param topology - The primitive topology.
	 * @param cullMode - The cull mode flags.
	 * @param depthTestEnable - Flag indicating whether depth testing is enabled.
	 * @param blendingEnable - Flag indicating whether blending is enabled.
	 */
	void Pipeline::CreatePipelineConfigInfo(PipelineConfigInfo& configInfo, uint32_t width, uint32_t height, VkPrimitiveTopology topology, VkCullModeFlags cullMode, bool depthTestEnable, bool blendingEnable, int colorAttachmentCount)
	{
		configInfo.InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		configInfo.InputAssemblyInfo.topology = topology;
		if (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) configInfo.InputAssemblyInfo.primitiveRestartEnable = VK_TRUE;
		else configInfo.InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		configInfo.Viewport.x = 0.0f;
		configInfo.Viewport.y = 0.0f;
		configInfo.Viewport.width = (float)width;
		configInfo.Viewport.height = (float)height;
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

		configInfo.ColorBlendAttachment.resize(colorAttachmentCount);
		for (int i = 0; i < colorAttachmentCount; i++)
		{
			if (blendingEnable)
			{
				configInfo.ColorBlendAttachment[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				configInfo.ColorBlendAttachment[i].blendEnable = VK_TRUE;
				configInfo.ColorBlendAttachment[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				configInfo.ColorBlendAttachment[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				configInfo.ColorBlendAttachment[i].colorBlendOp = VK_BLEND_OP_ADD;
				configInfo.ColorBlendAttachment[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				configInfo.ColorBlendAttachment[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
				configInfo.ColorBlendAttachment[i].alphaBlendOp = VK_BLEND_OP_ADD;
			}
			else
			{
				configInfo.ColorBlendAttachment[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				configInfo.ColorBlendAttachment[i].blendEnable = VK_FALSE;
				configInfo.ColorBlendAttachment[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				configInfo.ColorBlendAttachment[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				configInfo.ColorBlendAttachment[i].colorBlendOp = VK_BLEND_OP_ADD;
				configInfo.ColorBlendAttachment[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				configInfo.ColorBlendAttachment[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
				configInfo.ColorBlendAttachment[i].alphaBlendOp = VK_BLEND_OP_ADD;
			}
		}

		configInfo.ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		configInfo.ColorBlendInfo.logicOpEnable = VK_FALSE;
		configInfo.ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
		configInfo.ColorBlendInfo.attachmentCount = colorAttachmentCount;
		configInfo.ColorBlendInfo.pAttachments = configInfo.ColorBlendAttachment.data();
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
	}
}