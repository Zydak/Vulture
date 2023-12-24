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
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> AsOffset;
	};

	struct AccelKHR
	{
		Ref<Buffer> Buffer;
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
		void Init(Scene& scene);

		AccelKHR GetTlas() { return m_Tlas; }
		AccelKHR GetBlas(int index) { return m_Blas[index].As; }

	private:
		void CreateTopLevelAS(Scene& scene);
		void CreateBottomLevelAS(Scene& scene);
		BlasInput MeshToGeometry(Mesh& mesh);
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
			Ref<Buffer> scratchBuffer,
			VkBuildAccelerationStructureFlagsKHR flags,
			bool update
		);

		void CreateAcceleration(VkAccelerationStructureCreateInfoKHR& createInfo, AccelKHR& As);
		void DestroyNonCompacted(std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs);

		VkDeviceAddress GetBlasDeviceAddress(uint32_t blasID);

	private:
		std::vector<BuildAccelerationStructure> m_Blas;
		AccelKHR m_Tlas;
	};
}