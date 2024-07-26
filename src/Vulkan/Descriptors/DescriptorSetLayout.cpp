#include "pch.h"
#include "DescriptorSetLayout.h"
#include "../DeleteQueue.h"

namespace Vulture
{
	/**
	 * @brief Initializes the descriptor set layout with the given bindings.
	 *
	 * @param bindings - Vector containing Binding objects representing the bindings 
	 *					 to be associated with this descriptor set layout.
	 */
	void DescriptorSetLayout::Init(std::vector<Binding> bindings)
	{
		// Check if the descriptor set layout has already been initialized.
		if (m_Initialized)
			Destroy(); // If already initialized, destroy the existing layout.

		// Store the given bindings.
		m_Bindings = bindings;

		// Prepare a vector to hold Vulkan descriptor set layout bindings.
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};

		// Iterate through each binding and convert them to Vulkan descriptor set layout bindings.
		for (int i = 0; i < bindings.size(); i++)
		{
			// Ensure each binding is correctly initialized.
			VL_CORE_ASSERT(bindings[i], "Incorrectly initialized binding in array!");

			// Create a Vulkan descriptor set layout binding based on the provided Binding object.
			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.binding = bindings[i].BindingNumber;
			layoutBinding.descriptorType = bindings[i].Type;
			layoutBinding.descriptorCount = bindings[i].DescriptorsCount;
			layoutBinding.stageFlags = bindings[i].StageFlags;

			// Add the created Vulkan descriptor set layout binding to the vector.
			setLayoutBindings.push_back(layoutBinding);
		}

		// Prepare the descriptor set layout creation information.
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
		descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

		// Attempt to create the Vulkan descriptor set layout object.
		VL_CORE_RETURN_ASSERT(vkCreateDescriptorSetLayout(Device::GetDevice(), &descriptorSetLayoutInfo, nullptr, &m_DescriptorSetLayoutHandle),
			VK_SUCCESS,
			"Failed to create descriptor set layout!"
		);

		// Mark the descriptor set layout as initialized.
		m_Initialized = true;
	}

	/**
	 * @brief Destroys the descriptor set layout.
	 */
	void DescriptorSetLayout::Destroy()
	{
		// Mark the descriptor set layout as not initialized.
		if (!m_Initialized)
			return;

		m_Initialized = false;

		// Destroy the Vulkan descriptor set layout object.
		DeleteQueue::TrashDescriptorSetLayout(*this);

		Reset();
	}

	/**
	 * @brief Constructor for DescriptorSetLayout.
	 * 
	 * @param bindings - Vector containing Binding objects representing the bindings 
	 *					 to be associated with this descriptor set layout.
	 */
	DescriptorSetLayout::DescriptorSetLayout(const std::vector<Binding>& bindings)
	{
		// Initialize the descriptor set layout with the provided bindings.
		Init(bindings);
	}

	DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_DescriptorSetLayoutHandle = std::move(other.m_DescriptorSetLayoutHandle);
		m_Bindings					= std::move(other.m_Bindings);
		m_Initialized				= std::move(other.m_Initialized);

		other.Reset();
	}

	DescriptorSetLayout& DescriptorSetLayout::operator=(DescriptorSetLayout&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_DescriptorSetLayoutHandle = std::move(other.m_DescriptorSetLayoutHandle);
		m_Bindings = std::move(other.m_Bindings);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	void DescriptorSetLayout::Reset()
	{
		m_DescriptorSetLayoutHandle = VK_NULL_HANDLE;
		m_Bindings.clear();

		bool m_Initialized = false;
	}

	/**
	 * @brief Destructor for DescriptorSetLayout.
	 */
	DescriptorSetLayout::~DescriptorSetLayout()
	{
		Destroy();
	}
}