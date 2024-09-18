#pragma once
#include "pch.h"
#include "Mesh.h"
#include "../Utility/Utility.h"

#include "Scene/Scene.h"

namespace Vulture
{
	struct BlasInput
	{
		std::vector<VkAccelerationStructureGeometryKHR> AsGeometry;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> AsRange;
	};

	struct AccelKHR
	{
		Vulture::Ref<Vulture::Buffer> Buffer;
		VkAccelerationStructureKHR Accel;
	};

	struct BuildAccelerationStructure
	{
		AccelKHR As;
		AccelKHR CleanupAs;
		VkAccelerationStructureBuildGeometryInfoKHR BuildInfo;
		VkAccelerationStructureBuildRangeInfoKHR* RangeInfo;
		VkAccelerationStructureBuildSizesInfoKHR SizeInfo;
	};

	class AccelerationStructure
	{
	public:
		struct Instance
		{
			Vulture::Mesh* mesh;
			VkTransformMatrixKHR transform;
		};

		struct CreateInfo
		{
			std::vector<Instance> Instances;
		};

		void Destroy();
		void Init(const CreateInfo& info);

		AccelerationStructure(const CreateInfo& info);
		AccelerationStructure() = default;
		~AccelerationStructure();

		AccelerationStructure(const AccelerationStructure& other) = delete;
		AccelerationStructure& operator=(const AccelerationStructure& other) = delete;
		AccelerationStructure(AccelerationStructure&& other) noexcept;
		AccelerationStructure& operator=(AccelerationStructure&& other) noexcept;

		inline AccelKHR GetTlas() const { return m_Tlas; }
		inline AccelKHR GetBlas(int index) const { return m_Blas[index].As; }

		inline bool IsInitialized() const { return m_Initialized; }

	private:
		void CreateTopLevelAS(const CreateInfo& info);
		void CreateBottomLevelAS(const CreateInfo& info);
		BlasInput MeshToGeometry(Mesh* mesh);

		void CmdCreateBlas(
			VkCommandBuffer cmdBuf,
			std::vector<uint32_t> indices,
			std::vector<BuildAccelerationStructure>& buildAs,
			VkDeviceAddress scratchAddress,
			VkQueryPool queryPool);
		void CmdCompactBlas(
			VkCommandBuffer cmdBuf,
			std::vector<uint32_t> indices,
			std::vector<BuildAccelerationStructure>& buildAs,
			VkQueryPool queryPool);

		void CmdCreateTlas(
			VkCommandBuffer cmdBuf,
			uint32_t instanceCount,
			VkDeviceAddress instanceBufferAddr,
			Buffer* scratchBuffer,
			VkBuildAccelerationStructureFlagsKHR flags,
			bool update
		);

		void CreateAcceleration(VkAccelerationStructureCreateInfoKHR& createInfo, AccelKHR& As);
		void DestroyNonCompacted(std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs);

		VkDeviceAddress GetBlasDeviceAddress(uint32_t blasID);

	private:
		std::vector<BuildAccelerationStructure> m_Blas;
		AccelKHR m_Tlas;

		bool m_Initialized = false;

		void Reset();
	};
}