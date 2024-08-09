// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "Utility/Utility.h"

#include "DescriptorSet.h"
#include "Sampler.h"

namespace Vulture
{
	/**
	 * @brief Initializes the descriptor set with the provided pool and bindings.
	 * 
	 * @param pool - Pointer to the descriptor pool to allocate the descriptor set from.
	 * @param bindings - Descriptor bindings specifying the layout of the descriptor set.
	 */
	void DescriptorSet::Init(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings, Sampler* samplerForEmptyBindings)
	{
		// Check if the descriptor set has already been initialized.
		if (m_Initialized)
			Destroy();

		// Set the descriptor pool.
		m_Pool = pool;

		// Initialize the descriptor set layout with the provided bindings.
		m_DescriptorSetLayout.Init(bindings);

		// Push empty structs into writes
		for (int i = 0; i < bindings.size(); i++) 
		{
			if (bindings[i].Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || bindings[i].Type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				VkDescriptorImageInfo emptyInfo{};
				emptyInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				emptyInfo.imageView = VK_NULL_HANDLE;
				emptyInfo.sampler = samplerForEmptyBindings == nullptr ? VK_NULL_HANDLE : samplerForEmptyBindings->GetSamplerHandle();

				Binding binding{};
				binding.m_Type = bindings[i].Type;
				VL_CORE_ASSERT(bindings[i].Type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "");
				for (int j = 0; j < (int)bindings[i].DescriptorsCount; j++)
				{
					binding.m_ImageInfo.push_back(emptyInfo);
				}
				m_BindingsWriteInfo.push_back(binding);
			}
			else if (bindings[i].Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || bindings[i].Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
			{
				VkDescriptorBufferInfo emptyInfo{};
				emptyInfo.buffer = VK_NULL_HANDLE;
				emptyInfo.offset = 0;
				emptyInfo.range = VK_WHOLE_SIZE;

				Binding binding{};
				binding.m_Type = bindings[i].Type;
				for (int j = 0; j < (int)bindings[i].DescriptorsCount; j++)
				{
					binding.m_BufferInfo.push_back(emptyInfo);
				}
				m_BindingsWriteInfo.push_back(binding);
			}
			else if (bindings[i].Type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
			{
				VkWriteDescriptorSetAccelerationStructureKHR emptyInfo{};
				emptyInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
				emptyInfo.accelerationStructureCount = 0;
				emptyInfo.pAccelerationStructures = nullptr;
				emptyInfo.pNext = nullptr;

				Binding binding{};
				binding.m_Type = bindings[i].Type;
				for (int j = 0; j < (int)bindings[i].DescriptorsCount; j++)
				{
					binding.m_AccelInfo.push_back(emptyInfo);
				}
				m_BindingsWriteInfo.push_back(binding);
			}
			else 
			{
				VL_CORE_ASSERT(false, "Trying to create descriptor with unsupported binding type! type: {}", bindings[i].Type);
			}
		}

		m_Initialized = true;
	}

	/**
	 * @brief Destroys the descriptor set.
	 */
	void DescriptorSet::Destroy()
	{
		if (!m_Initialized)
			return;

		// Destroy the descriptor set layout.
		m_DescriptorSetLayout.Destroy();

		// Clear the bindings write information.
		Reset();
	}

	/**
	 * @brief Constructor for the DescriptorSet class.
	 *
	 * @param pool - Pointer to the descriptor pool to allocate the descriptor set from.
	 * @param bindings - Descriptor bindings specifying the layout of the descriptor set.
	 */
	DescriptorSet::DescriptorSet(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings)
	{
		// Initialize the descriptor set with the provided pool and bindings.
		Init(pool, bindings);
	}

	DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_BindingsWriteInfo		= std::move(other.m_BindingsWriteInfo);
		m_DescriptorSetLayout	= std::move(other.m_DescriptorSetLayout);
		m_DescriptorSetHandle	= std::move(other.m_DescriptorSetHandle);
		m_Pool					= std::move(other.m_Pool);
		m_Initialized			= std::move(other.m_Initialized);

		other.Reset();
	}

	DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_BindingsWriteInfo = std::move(other.m_BindingsWriteInfo);
		m_DescriptorSetLayout = std::move(other.m_DescriptorSetLayout);
		m_DescriptorSetHandle = std::move(other.m_DescriptorSetHandle);
		m_Pool = std::move(other.m_Pool);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	/**
	 * @brief Destructor for the DescriptorSet class.
	 */
	DescriptorSet::~DescriptorSet()
	{
		Destroy();
	}

	// TODO
	void DescriptorSet::AddImageSampler(uint32_t binding, const VkDescriptorImageInfo& info)
	{
		// Check if the DescriptorSet has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Check if the binding number is valid.
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		// Check if the binding type in the descriptor set layout matches VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER or VK_DESCRIPTOR_TYPE_STORAGE_IMAGE.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			"Wrong Binding Type! Type inside layout is: {}", m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type
		);

		// Check if the number of descriptors exceeds the specified count for the binding in the descriptor set layout.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount >= m_BindingsWriteInfo[binding].m_ImageInfo.size(),
			"Too many descriptors! Descriptor Count specified in layout: {0}",
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount
		);

		// Add the image sampler to the bindings write information for the specified binding.
		Binding& writeInfo = m_BindingsWriteInfo[binding];
		writeInfo.m_ImageInfo[writeInfo.m_DescriptorCount] = info;
		writeInfo.m_DescriptorCount++;
	}

	/**
	 * @brief Adds an acceleration structure to the specified descriptor set binding.
	 *
	 * @param binding - Binding number to which the acceleration structure will be added.
	 * @param asInfo - VkWriteDescriptorSetAccelerationStructureKHR structure containing acceleration structure info.
	 */
	void DescriptorSet::AddAccelerationStructure(uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR& asInfo)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Check if the binding number is valid.
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		// Check if the binding type in the descriptor set layout matches VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			"Wrong Binding Type! Type inside layout is: {}", m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type
		);

		// Check if the number of descriptors exceeds the specified count for the binding in the descriptor set layout.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount >= m_BindingsWriteInfo[binding].m_AccelInfo.size(),
			"Too many descriptors! Descriptor Count specified in layout: {0}",
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount
		);

		// Add the acceleration structure to the bindings write information for the specified binding.
		Binding& writeInfo = m_BindingsWriteInfo[binding];
		writeInfo.m_AccelInfo[writeInfo.m_DescriptorCount] = asInfo;
		writeInfo.m_DescriptorCount++;
	}

	// TODO
	void DescriptorSet::AddBuffer(uint32_t binding, const VkDescriptorBufferInfo& info)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Check if the binding number is valid.
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() > binding, "There is no such binding: {0}", binding);

		// Check if the binding type in the descriptor set layout matches VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			"Wrong Binding Type! Type inside layout is: {}", m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type
		);

		// Check if the number of descriptors exceeds the specified count for the binding in the descriptor set layout.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount >= m_BindingsWriteInfo[binding].m_BufferInfo.size(),
			"Too many descriptors! Descriptor Count specified in layout: {0}",
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount
		);

		// Add the buffer descriptor info to the bindings write information for the specified binding.
		Binding& writeInfo = m_BindingsWriteInfo[binding];
		writeInfo.m_BufferInfo[writeInfo.m_DescriptorCount] = info;
		writeInfo.m_DescriptorCount++;
	}

	/**
	 * @brief Builds the descriptor set.
	 */
	void DescriptorSet::Build()
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Create a descriptor writer using the descriptor set layout and the descriptor pool.
		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);

		// Iterate over each binding and write data to the descriptor set.
		for (int i = 0; i < m_BindingsWriteInfo.size(); i++)
		{
			Binding& binding = m_BindingsWriteInfo[i];
			// Determine the type of binding and call the corresponding write function.
			if (binding.m_Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || binding.m_Type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				writer.WriteImage(i, binding.m_ImageInfo.data());
			}
			else if (binding.m_Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || binding.m_Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
			{
				writer.WriteBuffer(i, binding.m_BufferInfo.data());
			}
			else if (binding.m_Type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
			{
				writer.WriteAs(i, binding.m_AccelInfo.data());
			}
			else
			{
				// This case should not happen. If it does, it indicates an unknown binding type.
				VL_CORE_ASSERT(false, "Unknown binding type: {0}", binding.m_Type);
			}
		}

		// Build the descriptor set using the descriptor writer and store the handle.
		writer.Build(&m_DescriptorSetHandle);
	}

	// TODO
	void DescriptorSet::UpdateImageSampler(uint32_t binding, VkDescriptorImageInfo& info)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Create a descriptor writer.
		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);

		// Write the image descriptor to the specified binding.
		writer.WriteImage(binding, &info);

		// Update the descriptor set with the new descriptor.
		writer.Overwrite(&m_DescriptorSetHandle);
	}

	// TODO
	void DescriptorSet::UpdateBuffer(uint32_t binding, VkDescriptorBufferInfo& info)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Create a descriptor writer.
		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);

		// Write the image descriptor to the specified binding.
		writer.WriteBuffer(binding, &info);

		// Update the descriptor set with the new descriptor.
		writer.Overwrite(&m_DescriptorSetHandle);
	}

	/*
	 * @brief Binds the descriptor set to a Vulkan command buffer.
	 *
	 * @param set - Index of the descriptor set to bind to.
	 * @param layout - Vulkan pipeline layout to use for the binding.
	 * @param bindPoint - Vulkan pipeline bind point (e.g., VK_PIPELINE_BIND_POINT_GRAPHICS or VK_PIPELINE_BIND_POINT_COMPUTE).
	 * @param cmdBuffer - Vulkan command buffer to which the descriptor set is bound.
	 */
	void DescriptorSet::Bind(uint32_t set, VkPipelineLayout layout, VkPipelineBindPoint bindPoint, VkCommandBuffer cmdBuffer) const
	{
		// Check if the DescriptorSet has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		vkCmdBindDescriptorSets(
			cmdBuffer,
			bindPoint,
			layout,
			set,
			1,
			&m_DescriptorSetHandle,
			0,
			nullptr
		);
	}

	void DescriptorSet::Reset()
	{
		m_BindingsWriteInfo.clear();
		//m_DescriptorSetLayout.Destroy();
		m_DescriptorSetHandle = VK_NULL_HANDLE;

		m_Pool = nullptr;

		m_Initialized = false;
	}

}