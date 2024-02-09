#include "pch.h"
#include "Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "AssetManager.h"

#include "lodepng.h"

#ifdef VL_IMGUI
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

void ImGui_ImplVulkan_DestroyDeviceObjects();
#endif

namespace Vulture
{
	void Renderer::Destroy()
	{
		s_IsInitialized = false;
		vkDeviceWaitIdle(Device::GetDevice());
		vkFreeCommandBuffers(Device::GetDevice(), Device::GetGraphicsCommandPool(), (uint32_t)s_CommandBuffers.size(), s_CommandBuffers.data());
		
		s_Swapchain.reset();

		s_EnvToCubemapDescriptorSet.reset();
		s_BloomSeparateBrightnessDescriptorSet.reset();
		s_BloomDownSampleDescriptorSet.clear();
		s_BloomAccumulateDescriptorSet.reset();

		s_BloomImages.clear();
		s_RendererSampler.reset();

		s_HDRToPresentablePipeline.Destroy();
		s_ToneMapPipeline.Destroy();
		s_BloomSeparateBrightnessPipeline.Destroy();
		s_BloomAccumulatePipeline.Destroy();
		s_BloomDownSamplePipeline.Destroy();
		s_EnvToCubemapPipeline.Destroy();

		s_QuadMesh.Destroy();
		s_Pool.reset();

#ifdef VL_IMGUI
		ImGui_ImplVulkan_DestroyDeviceObjects();
#endif
	}

	static void CheckVkResult(VkResult err)
	{
		if (err == 0) return;
		fprintf(stderr, "[Vulkan] Error: VkResult = %d\n", err);
		if (err < 0) abort();
	}

	void Renderer::Init(Window& window)
	{
		s_RendererSampler = std::make_unique<Sampler>(SamplerInfo(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR));
		
		s_IsInitialized = true;
		s_Window = &window;
		CreatePool();
		RecreateSwapchain();
		CreateCommandBuffers();

		// Vertices for a simple quad
		const std::vector<Mesh::Vertex> vertices = 
		{
			Mesh::Vertex(glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)),  // Vertex 1 Bottom Left
			Mesh::Vertex(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)), // Vertex 2 Top Left
			Mesh::Vertex(glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)),  // Vertex 3 Top Right
			Mesh::Vertex(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f))    // Vertex 4 Bottom Right
		};

		const std::vector<uint32_t> indices = 
		{
			0, 1, 2,  // First triangle
			0, 2, 3   // Second triangle
		};

		s_QuadMesh.Init(vertices, indices);

#ifdef VL_IMGUI
		// ImGui Creation
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui_ImplGlfw_InitForVulkan(s_Window->GetGLFWwindow(), true);
		ImGui_ImplVulkan_InitInfo info{};
		info.Instance = Device::GetInstance();
		info.PhysicalDevice = Device::GetPhysicalDevice();
		info.Device = Device::GetDevice();
		info.Queue = Device::GetGraphicsQueue();
		info.DescriptorPool = s_Pool->GetDescriptorPoolHandle();
		info.Subpass = 0;
		info.MinImageCount = 2;
		info.ImageCount = s_Swapchain->GetImageCount();
		info.CheckVkResultFn = CheckVkResult;
		ImGui_ImplVulkan_Init(&info, s_Swapchain->GetSwapchainRenderPass());

		VkCommandBuffer cmdBuffer;
		Device::BeginSingleTimeCommands(cmdBuffer, Device::GetGraphicsCommandPool());
		ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
		Device::EndSingleTimeCommands(cmdBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());

		vkDeviceWaitIdle(Device::GetDevice());
		ImGui_ImplVulkan_DestroyFontUploadObjects();
#endif
	}

	/*
	 * @brief Begins recording a command buffer for rendering. If it returns false, you should
	 * recreate all resources that are tied to the swapchain (for example framebuffers with the swapchain extent).
	 *
	 * @return True if the frame started successfully; false if window was resized.
	 */
	bool Renderer::BeginFrame()
	{
		return BeginFrameInternal();
	}

	/*
	 * @brief End recording a command buffer for rendering. If it returns false, you should
	 * recreate all resources that are tied to the swapchain (for example framebuffers with the swapchain extent).
	 *
	 * @return True if the frame ended successfully; false if window was resized.
	 */
	bool Renderer::EndFrame()
	{
		return EndFrameInternal();
	}

	void Renderer::RenderImGui(std::function<void()> fn)
	{
		s_ImGuiFunction = fn;
	}

	/*
	 * @brief Acquires the next swap chain image and begins recording a command buffer for rendering. 
	 * If the swap chain is out of date, it may trigger swap chain recreation.
	 *
	 * @return True if the frame started successfully; false if the swap chain needs recreation.
	 */
	bool Renderer::BeginFrameInternal()
	{
		VL_CORE_ASSERT(!s_IsFrameStarted, "Can't call BeginFrame while already in progress!");

		auto result = s_Swapchain->AcquireNextImage(s_CurrentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapchain();

			return false;
		}
		VL_CORE_ASSERT((result == VK_SUCCESS && result != VK_SUBOPTIMAL_KHR), "failed to acquire swap chain image!");

		s_IsFrameStarted = true;
		auto commandBuffer = GetCurrentCommandBuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VL_CORE_RETURN_ASSERT(
			vkBeginCommandBuffer(commandBuffer, &beginInfo),
			VK_SUCCESS,
			"failed to begin recording command buffer!"
		);
		return true;
	}

	/*
	 * @brief Finalizes the recorded command buffer, submits it for execution, and presents the swap chain image. 
	 * If the swap chain is out of date, it triggers swap chain recreation and returns false.
	 * 
	 * @return True if the frame ended successfully; false if the swap chain needs recreation.
	 */
	bool Renderer::EndFrameInternal()
	{
		bool retVal = true;
		auto commandBuffer = GetCurrentCommandBuffer();
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot call EndFrame while frame is not in progress");

		// End recording the command buffer
		auto success = vkEndCommandBuffer(commandBuffer);
		VL_CORE_ASSERT(success == VK_SUCCESS, "Failed to record command buffer!");

		// Submit the command buffer for execution and present the image
		auto result = s_Swapchain->SubmitCommandBuffers(&commandBuffer, s_CurrentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || s_Window->WasWindowResized())
		{
			// Recreate swap chain or handle window resize
			s_Window->ResetWindowResizedFlag();
			RecreateSwapchain();
			retVal = false;
		}
		else
		{
			VL_CORE_ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");
		}

		// End the frame and update frame index
		s_IsFrameStarted = false;
		s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % Swapchain::MAX_FRAMES_IN_FLIGHT;
		return retVal;
	}

	/*
	 * @brief Sets up the rendering viewport, scissor, and begins the specified render pass on the given framebuffer. 
	 * It also clears the specified colors in the render pass.
	 *
	 * @param clearColors - A vector of clear values for the attachments in the render pass.
	 * @param framebuffer - The framebuffer to use in the render pass.
	 * @param renderPass - The render pass to begin.
	 * @param extent - The extent (width and height) of the render area.
	 */
	void Renderer::BeginRenderPass(const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent)
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot call BeginSwapchainRenderPass while frame is not in progress");

		// Set up viewport
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = extent.x;
		viewport.height = extent.y;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Set up scissor
		VkRect2D scissor{
			{0, 0},
			VkExtent2D{(uint32_t)extent.x, (uint32_t)extent.y}
		};

		// Set viewport and scissor for the current command buffer
		vkCmdSetViewport(GetCurrentCommandBuffer(), 0, 1, &viewport);
		vkCmdSetScissor(GetCurrentCommandBuffer(), 0, 1, &scissor);

		// Set up render pass information
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = framebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{ (uint32_t)extent.x, (uint32_t)extent.y };
		renderPassInfo.clearValueCount = (uint32_t)clearColors.size();
		renderPassInfo.pClearValues = clearColors.data();

		// Begin the render pass for the current command buffer
		vkCmdBeginRenderPass(GetCurrentCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	/**
	 * @brief Ends the current render pass in progress. 
	 * It should be called after rendering commands within a render pass have been recorded.
	 */
	void Renderer::EndRenderPass()
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Can't call EndSwapchainRenderPass while frame is not in progress");
		VL_CORE_ASSERT(GetCurrentCommandBuffer() == GetCurrentCommandBuffer(), "Can't end render pass on command buffer from different frame");

		vkCmdEndRenderPass(GetCurrentCommandBuffer());
	}

	// TODO: description
	void Renderer::SaveImageToFile(const std::string& filepath, Ref<Image> image, VkCommandBuffer cmd)
	{
		int width = image->GetImageSize().width;
		int height = image->GetImageSize().height;

		Image::CreateInfo imageInfo{};
		imageInfo.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		imageInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.Height = image->GetImageSize().height;
		imageInfo.Width = image->GetImageSize().width;
		imageInfo.LayerCount = 1;
		imageInfo.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageInfo.SamplerInfo = SamplerInfo{};
		imageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.Type = Image::ImageType::Image2D;
		imageInfo.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		Image image8Bit(imageInfo);
		image8Bit.TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, cmd);

		VkImageBlit blitRegion{};
		blitRegion.dstOffsets[0] = {0, 0, 0};
		blitRegion.dstOffsets[1] = { width, height, 1};
		blitRegion.srcOffsets[0] = { 0, 0, 0 };
		blitRegion.srcOffsets[1] = { width, height, 1 };
		blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

		vkCmdBlitImage(cmd, image->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image8Bit.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_LINEAR);
		image8Bit.TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, cmd);

		Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = width * height * 4;
		info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		info.UsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		Buffer tempBuffer(info);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.imageExtent = {image->GetImageSize().width, image->GetImageSize().height, 1};
		region.imageOffset = { 0, 0, 0 };
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		tempBuffer.Map();

		vkCmdCopyImageToBuffer(cmd, image8Bit.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tempBuffer.GetBuffer(), 1, &region);
		Device::EndSingleTimeCommands(cmd, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());

		vkDeviceWaitIdle(Device::GetDevice());

		void* bufferData = tempBuffer.GetMappedMemory();

		std::vector<unsigned char> imageBuffer(width * height * 4);
		memcpy(imageBuffer.data(), bufferData, width * height * 4);

		uint32_t imagesCount = 0;
		std::string path = "Rendered_Images/";
		for (const auto& entry : std::filesystem::directory_iterator(path)) 
		{
			if (std::filesystem::is_regular_file(entry.status())) 
			{
				imagesCount++;
			}
		}

		WriteToFile(std::string("Rendered_Images/Render" + std::to_string(imagesCount) + ".png").c_str(), imageBuffer, width, height);
	}

	/**
	 * @brief Takes descriptor set with single combined image sampler descriptor and copies data
	 * from the image onto presentable swapchain framebuffer
	 * 
	 * @param descriptorWithImageSampler - descriptor set with single image sampler
	 */
	void Renderer::FramebufferCopyPassImGui(Ref<DescriptorSet> descriptorWithImageSampler)
	{
		std::vector<VkClearValue> clearColors;
		clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
		// Begin the render pass
		BeginRenderPass(
			clearColors,
			s_Swapchain->GetPresentableFrameBuffer(s_CurrentImageIndex),
			s_Swapchain->GetSwapchainRenderPass(),
			glm::vec2(s_Swapchain->GetSwapchainExtent().width, s_Swapchain->GetSwapchainExtent().height)
		);
		// Bind the geometry pipeline
		s_HDRToPresentablePipeline.Bind(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS);

		descriptorWithImageSampler->Bind(
			0,
			s_HDRToPresentablePipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			GetCurrentCommandBuffer()
		);

		s_QuadMesh.Bind(GetCurrentCommandBuffer());
		s_QuadMesh.Draw(GetCurrentCommandBuffer(), 1);

#ifdef VL_IMGUI
		s_ImGuiFunction();
#endif
		// End the render pass
		EndRenderPass();
	}

	void Renderer::ImGuiPass()
	{
		std::vector<VkClearValue> clearColors;
		clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
		BeginRenderPass(
			clearColors,
			s_Swapchain->GetPresentableFrameBuffer(s_CurrentImageIndex),
			s_Swapchain->GetSwapchainRenderPass(),
			glm::vec2(s_Swapchain->GetSwapchainExtent().width, s_Swapchain->GetSwapchainExtent().height)
		);

#ifdef VL_IMGUI
		s_ImGuiFunction();
#endif
		EndRenderPass();
	}

	// TODO description, note that image has to be in transfer src optimal layout
	void Renderer::FramebufferCopyPassBlit(Ref<Image> image)
	{
		Image::TransitionImageLayout(
			s_Swapchain->GetPresentableImage(GetCurrentFrameIndex()),
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			GetCurrentCommandBuffer()
		);
		 
		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { (int32_t)image->GetImageSize().width, (int32_t)image->GetImageSize().height, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { (int32_t)s_Swapchain->GetSwapchainExtent().width, (int32_t)s_Swapchain->GetSwapchainExtent().height, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(
			GetCurrentCommandBuffer(),
			image->GetImage(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			s_Swapchain->GetPresentableImage(GetCurrentFrameIndex()),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&blit,
			VK_FILTER_NEAREST
		);

		Image::TransitionImageLayout(
			s_Swapchain->GetPresentableImage(GetCurrentFrameIndex()),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			GetCurrentCommandBuffer()
		);
	}

	// TODO description
	// TODO why the fuck is there uniform AND image?
	void Renderer::ToneMapPass(Ref<DescriptorSet> descriptorWithImageSampler, Ref<Image> image, float exposure)
	{
		VkImageMemoryBarrier barrierWrite{};
		barrierWrite.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrierWrite.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrierWrite.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrierWrite.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrierWrite.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrierWrite.image = image->GetImage();
		barrierWrite.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrierWrite.subresourceRange.baseArrayLayer = 0;
		barrierWrite.subresourceRange.baseMipLevel = 0;
		barrierWrite.subresourceRange.layerCount = 1;
		barrierWrite.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(
			Vulture::Renderer::GetCurrentCommandBuffer(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrierWrite
		);

		s_ToneMapPipeline.Bind(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE);

		descriptorWithImageSampler->Bind(
			0,
			s_ToneMapPipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_COMPUTE,
			GetCurrentCommandBuffer()
		);

		vkCmdPushConstants(
			GetCurrentCommandBuffer(),
			s_ToneMapPipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_COMPUTE_BIT,
			0,
			sizeof(float),
			&exposure
		);

		vkCmdDispatch(GetCurrentCommandBuffer(), ((int)image->GetImageSize().width) / 8 + 1, ((int)image->GetImageSize().width) / 8 + 1, 1);
	
		VkImageMemoryBarrier barrierRead{};
		barrierRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrierRead.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrierRead.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrierRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrierRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrierRead.image = image->GetImage();
		barrierRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrierRead.subresourceRange.baseArrayLayer = 0;
		barrierRead.subresourceRange.baseMipLevel = 0;
		barrierRead.subresourceRange.layerCount = 1;
		barrierRead.subresourceRange.levelCount = 1;

		vkCmdPipelineBarrier(
			Vulture::Renderer::GetCurrentCommandBuffer(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrierRead
		);
	}

	void Renderer::BloomPass(Ref<Image> image, int mipsCount)
	{
		if (mipsCount <= 0)
		{
			VL_CORE_WARN("Incorrect mips count! {}", mipsCount);
			return;
		}
		if ((image->GetImageSize().width != s_MipSize.width || image->GetImageSize().height != s_MipSize.height) || mipsCount != m_PrevMipsCount)
		{
			VL_CORE_INFO("Recreating bloom framebuffers");
			m_PrevMipsCount = mipsCount;
			CreateBloomImages(image, mipsCount);
		}

		s_BloomImages[0]->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL, 
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			GetCurrentCommandBuffer()
		);

		s_BloomSeparateBrightnessPipeline.Bind(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE);
		s_BloomSeparateBrightnessDescriptorSet->Bind(0, s_BloomSeparateBrightnessPipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, GetCurrentCommandBuffer());

		float threshold = 1.0f;
		vkCmdPushConstants(
			GetCurrentCommandBuffer(), 
			s_BloomSeparateBrightnessPipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_COMPUTE_BIT,
			0,
			sizeof(float),
			&threshold
		);

		vkCmdDispatch(GetCurrentCommandBuffer(), s_BloomImages[0]->GetImageSize().width / 8 + 1, s_BloomImages[0]->GetImageSize().height / 8 + 1, 1);
		
		s_BloomImages[0]->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			GetCurrentCommandBuffer()
		);

		for (int i = 1; i < (int)s_BloomImages.size(); i++)
		{
			s_BloomImages[i]->TransitionImageLayout(
				VK_IMAGE_LAYOUT_GENERAL,
				VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 
				VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				GetCurrentCommandBuffer()
			);
		}

		s_BloomDownSamplePipeline.Bind(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE);
		for (int i = 1; i < (int)s_BloomImages.size(); i++)
		{
			s_BloomDownSampleDescriptorSet[i-1]->Bind(0, s_BloomDownSamplePipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, GetCurrentCommandBuffer());
		
			vkCmdDispatch(GetCurrentCommandBuffer(), s_BloomImages[i-1]->GetImageSize().width / 8 + 1, s_BloomImages[i-1]->GetImageSize().height / 8 + 1, 1);
		
			s_BloomImages[i]->TransitionImageLayout(
				VK_IMAGE_LAYOUT_GENERAL,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				GetCurrentCommandBuffer()
			);
		}

		for (int i = 1; i < (int)s_BloomImages.size(); i++)
		{
			s_BloomImages[i]->TransitionImageLayout(
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
				VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, 
				VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
				GetCurrentCommandBuffer()
			);
		}
		s_BloomAccumulatePipeline.Bind(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE);
		s_BloomAccumulateDescriptorSet->Bind(0, s_BloomAccumulatePipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, GetCurrentCommandBuffer());

		vkCmdPushConstants(
			GetCurrentCommandBuffer(),
			s_BloomAccumulatePipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_COMPUTE_BIT,
			0,
			sizeof(int),
			&mipsCount
		);

		vkCmdDispatch(GetCurrentCommandBuffer(), image->GetImageSize().width / 8 + 1, image->GetImageSize().height / 8 + 1, 1);
	}

	void Renderer::EnvMapToCubemapPass(Ref<Image> envMap, Ref<Image> cubemap)
	{
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			s_EnvToCubemapDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
			s_EnvToCubemapDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
			s_EnvToCubemapDescriptorSet->AddImageSampler(0, envMap->GetSamplerHandle(), envMap->GetImageView(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);
			s_EnvToCubemapDescriptorSet->AddImageSampler(1, cubemap->GetSamplerHandle(), cubemap->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL
			);
			s_EnvToCubemapDescriptorSet->Build();
		}

		VkCommandBuffer cmdBuf;
		Device::BeginSingleTimeCommands(cmdBuf, Device::GetGraphicsCommandPool());

		// TODO automatically deduce layer count for range in cubemaps
		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 6;

		cubemap->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			cmdBuf,
			range
		);

		s_EnvToCubemapPipeline.Bind(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE);
		s_EnvToCubemapDescriptorSet->Bind(0, s_EnvToCubemapPipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmdBuf);
	
		vkCmdDispatch(cmdBuf, cubemap->GetImageSize().width / 8 + 1, cubemap->GetImageSize().height / 8 + 1, 1);

		cubemap->TransitionImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			cmdBuf,
			range
		);

		Device::EndSingleTimeCommands(cmdBuf, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());
	}

	/*
	 * @brief This function is called when the window is resized or the swapchain needs to be recreated for any reason.
	 */
	void Renderer::RecreateSwapchain()
	{
		// Wait for the window to have a valid extent
		auto extent = s_Window->GetExtent();
		while (extent.width == 0 || extent.height == 0)
		{
			extent = s_Window->GetExtent();
			glfwWaitEvents();
		}

		// Wait for the device to be idle before recreating the swapchain
		vkDeviceWaitIdle(Device::GetDevice());

		// Recreate the swapchain
		if (s_Swapchain == nullptr)
		{
			s_Swapchain = std::make_unique<Swapchain>(extent, PresentModes::VSync);
		}
		else
		{
			// Move the old swapchain into a shared pointer to ensure it is properly cleaned up
			std::shared_ptr<Swapchain> oldSwapchain = std::move(s_Swapchain);

			// Create a new swapchain using the old one as a reference
			s_Swapchain = std::make_unique<Swapchain>(extent, PresentModes::VSync, oldSwapchain);

			// Check if the swap formats are consistent
			VL_CORE_ASSERT(oldSwapchain->CompareSwapFormats(*s_Swapchain), "Swap chain image or depth formats have changed!");
		}

		// Recreate other resources dependent on the swapchain, such as pipelines or framebuffers
		CreatePipeline();
		CreateBloomImages(nullptr, 0);
	}

	/*
	 * @brief Allocates primary command buffers from the command pool for each swap chain image.
	 */
	void Renderer::CreateCommandBuffers()
	{
		// Resize the command buffer array to match the number of swap chain images
		s_CommandBuffers.resize(s_Swapchain->GetImageCount());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = Device::GetGraphicsCommandPool();
		allocInfo.commandBufferCount = (uint32_t)s_CommandBuffers.size();

		// Allocate primary command buffers
		VL_CORE_RETURN_ASSERT(
			vkAllocateCommandBuffers(Device::GetDevice(), &allocInfo, s_CommandBuffers.data()),
			VK_SUCCESS,
			"Failed to allocate command buffers!"
		);
	}

	/*
	 * @brief Creates the descriptor pool for managing descriptor sets.
	 */
	void Renderer::CreatePool()
	{
		// Create and initialize the descriptor pool
		std::vector<DescriptorPool::PoolSize> poolSizes;
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000 });
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 100 });
		s_Pool = std::make_unique<DescriptorPool>(poolSizes, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
	}

	/**
	 * @brief Creates the graphics pipeline for rendering geometry.
	 */
	void Renderer::CreatePipeline()
	{
		//
		// HDR to presentable
		//

		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
			DescriptorSetLayout imageLayout({bin});

			Pipeline::GraphicsCreateInfo info{};
			info.AttributeDesc = Mesh::Vertex::GetAttributeDescriptions();
			info.BindingDesc = Mesh::Vertex::GetBindingDescriptions();
			info.ShaderFilepaths.push_back("../Vulture/src/Vulture/Shaders/spv/HDRToPresentable.vert.spv");
			info.ShaderFilepaths.push_back("../Vulture/src/Vulture/Shaders/spv/HDRToPresentable.frag.spv");
			info.BlendingEnable = false;
			info.DepthTestEnable = false;
			info.CullMode = VK_CULL_MODE_BACK_BIT;
			info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			info.Width = s_Swapchain->GetWidth();
			info.Height = s_Swapchain->GetHeight();
			info.PushConstants = nullptr;
			info.RenderPass = s_Swapchain->GetSwapchainRenderPass();

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;

			// Create the graphics pipeline
			s_HDRToPresentablePipeline.Init(info);
		}

		// Tone map
		{
			VkPushConstantRange range{};
			range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			range.offset = 0;
			range.size = sizeof(float);

			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin });

			Pipeline::ComputeCreateInfo info{};
			info.ShaderFilepath = "../Vulture/src/Vulture/Shaders/spv/Tonemap.comp.spv";

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;
			info.PushConstants = &range;

			// Create the graphics pipeline
			s_ToneMapPipeline.Init(info);
		}

		// Env to cubemap
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			info.ShaderFilepath = "../Vulture/src/Vulture/Shaders/spv/EnvToCubemap.comp.spv";

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;

			// Create the graphics pipeline
			s_EnvToCubemapPipeline.Init(info);
		}
	}

	void Renderer::CreateDescriptorSets()
	{
		
	}

	void Renderer::CreateBloomImages(Ref<Image> image, int mipsCount)
	{
		if (image == nullptr)
		{
			s_BloomImages.clear();
			m_MipsCount = 0;
			m_PrevMipsCount = 0;
			s_MipSize = VkExtent2D{ 0, 0 };
			return;
		}
		s_MipSize = image->GetImageSize();
		s_BloomImages.clear();

		Image::CreateInfo info{};
		info.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
		info.Width = image->GetImageSize().width;
		info.Height = image->GetImageSize().height;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Properties = image->GetMemoryProperties();
		info.LayerCount = 1;
		info.SamplerInfo = SamplerInfo{};
		info.Type = Image::ImageType::Image2D;

		// First Image is just a copy with separated bright values
		s_BloomImages.push_back(std::make_shared<Image>(info));

		for (int i = 0; i < mipsCount; i++)
		{
			info.Width = glm::max(1, (int)info.Width / 2);
			info.Height = glm::max(1, (int)info.Height / 2);
			s_BloomImages.push_back(std::make_shared<Image>(info));
		}


		// TODO move this from here
		//-----------------------------------------------
		// Pipelines
		//-----------------------------------------------

		// Bloom Separate Bright Values
		{
			VkPushConstantRange range{};
			range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			range.offset = 0;
			range.size = sizeof(int);

			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			info.ShaderFilepath = "../Vulture/src/Vulture/Shaders/spv/SeparateBrightValues.comp.spv";
			info.PushConstants = &range;

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;

			// Create the graphics pipeline
			s_BloomSeparateBrightnessPipeline.Init(info);
		}

		// Bloom Accumulate
		{
			VkPushConstantRange range{};
			range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			range.offset = 0;
			range.size = sizeof(int);

			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, (uint32_t)mipsCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			info.ShaderFilepath = "../Vulture/src/Vulture/Shaders/spv/Bloom.comp.spv";
			info.PushConstants = &range;

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;

			// Create the graphics pipeline
			s_BloomAccumulatePipeline.Init(info);
		}

		// Bloom Down Sample
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			info.ShaderFilepath = "../Vulture/src/Vulture/Shaders/spv/BloomDownSample.comp.spv";
			info.PushConstants = nullptr;

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;

			// Create the graphics pipeline
			s_BloomDownSamplePipeline.Init(info);
		}


		//-----------------------------------------------
		// Descriptor sets
		//-----------------------------------------------

		// Bloom Separate Bright Values
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			s_BloomSeparateBrightnessDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
			s_BloomSeparateBrightnessDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
			s_BloomSeparateBrightnessDescriptorSet->AddImageSampler(0, image->GetSamplerHandle(), image->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL
			);
			s_BloomSeparateBrightnessDescriptorSet->AddImageSampler(1, s_BloomImages[0]->GetSamplerHandle(), s_BloomImages[0]->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL
			);
			s_BloomSeparateBrightnessDescriptorSet->Build();
		}

		// Bloom Accumulate
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, (uint32_t)mipsCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };

			s_BloomAccumulateDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
			s_BloomAccumulateDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
			s_BloomAccumulateDescriptorSet->AddImageSampler(0, image->GetSamplerHandle(), image->GetImageView(),VK_IMAGE_LAYOUT_GENERAL);
			for (int i = 1; i < mipsCount+1; i++)
			{
				s_BloomAccumulateDescriptorSet->AddImageSampler(1, s_BloomImages[i]->GetSamplerHandle(), s_BloomImages[i]->GetImageView(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
			s_BloomAccumulateDescriptorSet->Build();
		}

		// Bloom Down Sample
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			s_BloomDownSampleDescriptorSet.resize(mipsCount);
			for (int i = 0; i < s_BloomDownSampleDescriptorSet.size(); i++)
			{
				s_BloomDownSampleDescriptorSet[i] = std::make_shared<Vulture::DescriptorSet>();
				s_BloomDownSampleDescriptorSet[i]->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
				s_BloomDownSampleDescriptorSet[i]->AddImageSampler(0, s_BloomImages[i]->GetSamplerHandle(), s_BloomImages[i]->GetImageView(),
					VK_IMAGE_LAYOUT_GENERAL 
				);
				s_BloomDownSampleDescriptorSet[i]->AddImageSampler(1, s_BloomImages[i + 1]->GetSamplerHandle(), s_BloomImages[i+1]->GetImageView(),
					VK_IMAGE_LAYOUT_GENERAL
				);
				s_BloomDownSampleDescriptorSet[i]->Build();
			}
		}
	}

	void Renderer::WriteToFile(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height)
	{
		unsigned error = lodepng::encode(filename, image, width, height);

		//if there's an error, display it
		VL_CORE_ASSERT(!error, "encoder error {} | {}", error, lodepng_error_text(error));
	}

	/**
	 * @brief Retrieves the current command buffer for rendering.
	 *
	 * @return The current command buffer.
	 */
	VkCommandBuffer Renderer::GetCurrentCommandBuffer()
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot get command buffer when frame is not in progress");
		return s_CommandBuffers[s_CurrentImageIndex];
	}

	/**
	 * @brief Retrieves the index of the current frame in progress.
	 *
	 * @return The index of the current frame.
	 */
	int Renderer::GetFrameIndex()
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot get frame index when frame is not in progress");
		return s_CurrentFrameIndex;
	}

	Window* Renderer::s_Window;
	Scope<DescriptorPool> Renderer::s_Pool;
	Scope<Swapchain> Renderer::s_Swapchain;
	std::vector<VkCommandBuffer> Renderer::s_CommandBuffers;
	bool Renderer::s_IsFrameStarted = false;
	uint32_t Renderer::s_CurrentImageIndex = 0;
	uint32_t Renderer::s_CurrentFrameIndex = 0;
	Scene* Renderer::s_CurrentSceneRendered;
	bool Renderer::s_IsInitialized = true;
	Pipeline Renderer::s_HDRToPresentablePipeline;
	Vulture::Pipeline Renderer::s_ToneMapPipeline;
	Vulture::Pipeline Renderer::s_BloomSeparateBrightnessPipeline;
	Vulture::Pipeline Renderer::s_BloomAccumulatePipeline;
	Vulture::Pipeline Renderer::s_BloomDownSamplePipeline;
	Vulture::Pipeline Renderer::s_EnvToCubemapPipeline;
	std::vector<Ref<Image>> Renderer::s_BloomImages;
	Mesh Renderer::s_QuadMesh;
	Scope<Sampler> Renderer::s_RendererSampler;
	Ref<Vulture::DescriptorSet> Renderer::s_BloomSeparateBrightnessDescriptorSet;
	Ref<Vulture::DescriptorSet> Renderer::s_BloomAccumulateDescriptorSet;
	std::vector<Ref<Vulture::DescriptorSet>> Renderer::s_BloomDownSampleDescriptorSet;
	Ref<Vulture::DescriptorSet> Renderer::s_EnvToCubemapDescriptorSet;
	VkExtent2D Renderer::s_MipSize = { 0, 0 };
	int Renderer::m_PrevMipsCount = 0;
	int Renderer::m_MipsCount = 0;
	std::function<void()> Renderer::s_ImGuiFunction;


}