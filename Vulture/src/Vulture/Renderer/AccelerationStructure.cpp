#include "pch.h"

#include "AccelerationStructure.h"
#include "Scene/Components.h"

namespace Vulture
{
	BlasInput AccelerationStructure::MeshToGeometry(Mesh& mesh)
	{
		// BLAS builder requires raw device addresses.
		VkDeviceAddress vertexAddress = mesh.GetVertexBuffer()->GetDeviceAddress();
		VkDeviceAddress indexAddress = mesh.GetIndexBuffer()->GetDeviceAddress();

		uint32_t maxPrimitiveCount = mesh.GetIndexCount() / 3;

		// Describe buffer as array of Mesh::Vertex.
		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec4 vertex position data.
		triangles.vertexData.deviceAddress = vertexAddress;
		triangles.vertexStride = sizeof(Mesh::Vertex);
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = indexAddress;
		//triangles.transformData = {};
		triangles.maxVertex = mesh.GetVertexCount() - 1;

		// Identify the above data as containing opaque triangles.
		VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		asGeom.geometry.triangles = triangles;

		// The entire array will be used to build the BLAS.
		VkAccelerationStructureBuildRangeInfoKHR offset{};
		offset.firstVertex = 0;
		offset.primitiveCount = maxPrimitiveCount;
		offset.primitiveOffset = 0;
		offset.transformOffset = 0;

		// Our blas is made from only one geometry, but could be made of many geometries
		BlasInput input{};
		input.AsGeometry.emplace_back(asGeom);
		input.AsOffset.emplace_back(offset);

		return input;
	}

	void AccelerationStructure::CmdCreateBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs, VkDeviceAddress scratchAddress, VkQueryPool queryPool)
	{
		if (queryPool)  // For querying the compaction size
			vkResetQueryPool(Device::GetDevice(), queryPool, 0, static_cast<uint32_t>(indices.size()));
		uint32_t queryCnt{ 0 };

		for (const auto& idx : indices)
		{
			// Actual allocation of buffer and acceleration structure.
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			createInfo.size = buildAs[idx].SizeInfo.accelerationStructureSize;  // Will be used to allocate memory.
			CreateAcceleration(createInfo, buildAs[idx].As);

			// BuildInfo #2 part
			buildAs[idx].BuildInfo.dstAccelerationStructure = buildAs[idx].As.Accel;  // Setting where the build lands
			buildAs[idx].BuildInfo.scratchData.deviceAddress = scratchAddress;  // All builds are using the same scratch buffer

			// Building the bottom-level-acceleration-structure
			Device::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[idx].BuildInfo, &buildAs[idx].RangeInfo);

			// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
			// is finished before starting the next one.
			VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

			if (queryPool)
			{
				// Add a query to find the 'real' amount of memory needed, use for compaction
				Device::vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &buildAs[idx].BuildInfo.dstAccelerationStructure,
					VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCnt);
				queryCnt++;
			}
		}
	}

	void AccelerationStructure::CmdCompactBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool)
	{
		uint32_t queryCtn{ 0 };

		// Get the compacted size result back
		std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
		vkGetQueryPoolResults(Device::GetDevice(), queryPool, 0, (uint32_t)compactSizes.size(), compactSizes.size() * sizeof(VkDeviceSize),
			compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);

		for (auto idx : indices)
		{
			buildAs[idx].CleanupAs = buildAs[idx].As;           // previous AS to destroy
			buildAs[idx].SizeInfo.accelerationStructureSize = compactSizes[queryCtn++];  // new reduced size

			// Creating a compact version of the AS
			VkAccelerationStructureCreateInfoKHR asCreateInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			asCreateInfo.size = buildAs[idx].SizeInfo.accelerationStructureSize;
			asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			CreateAcceleration(asCreateInfo, buildAs[idx].As);

			// Copy the original BLAS to a compact version
			VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
			copyInfo.src = buildAs[idx].BuildInfo.dstAccelerationStructure;
			copyInfo.dst = buildAs[idx].As.Accel;
			copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
			Device::vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
		}
	}

	void AccelerationStructure::CmdCreateTlas(VkCommandBuffer cmdBuf, uint32_t instanceCount, VkDeviceAddress instanceBufferAddr, Ref<Buffer> scratchBuffer, VkBuildAccelerationStructureFlagsKHR flags, bool update)
	{
		// Wraps a device pointer to the above uploaded instances.
		VkAccelerationStructureGeometryInstancesDataKHR instancesVk{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		instancesVk.data.deviceAddress = instanceBufferAddr;

		// Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
		VkAccelerationStructureGeometryKHR topASGeometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topASGeometry.geometry.instances = instancesVk;

		// Find sizes
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		buildInfo.flags = flags;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &topASGeometry;
		buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		Device::vkGetAccelerationStructureBuildSizesKHR(Device::GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &instanceCount, &sizeInfo);

		// Create TLAS
		if (update == false)
		{
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			createInfo.size = sizeInfo.accelerationStructureSize;

			CreateAcceleration(createInfo, m_Tlas);
		}

		// Allocate the scratch memory
		scratchBuffer = std::make_shared<Buffer>(sizeInfo.buildScratchSize, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Update build information
		buildInfo.srcAccelerationStructure = update ? m_Tlas.Accel : VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = m_Tlas.Accel;
		buildInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

		// Build Offsets info: n instances
		VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{ instanceCount, 0, 0, 0 };
		const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

		// Build the TLAS
		Device::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);
	}

	void AccelerationStructure::CreateAcceleration(VkAccelerationStructureCreateInfoKHR& createInfo, AccelKHR& As)
	{
		As.Buffer = std::make_shared<Buffer>(createInfo.size, 1, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 128);
		// TODO alignment size

		// Setting the buffer
		createInfo.buffer = As.Buffer->GetBuffer();
		// Create the acceleration structure
		Device::vkCreateAccelerationStructureKHR(Device::GetDevice(), &createInfo, &As.Accel);
	}

	void AccelerationStructure::DestroyNonCompacted(std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs)
	{
		for (auto& i : indices)
		{
			Device::vkDestroyAccelerationStructureKHR(Device::GetDevice(), buildAs[i].CleanupAs.Accel);
		}
	}

	VkDeviceAddress AccelerationStructure::GetBlasDeviceAddress(uint32_t blasID)
	{
		VL_CORE_ASSERT(blasID < m_Blas.size(), "There is no such index");
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		addressInfo.accelerationStructure = m_Blas[blasID].As.Accel;
		return Device::vkGetAccelerationStructureDeviceAddressKHR(Device::GetDevice(), &addressInfo);
	}

	void AccelerationStructure::Init(Scene& scene)
	{
		CreateBottomLevelAS(scene);
		CreateTopLevelAS(scene);
	}

	void AccelerationStructure::CreateTopLevelAS(Scene& scene)
	{
		std::vector<VkAccelerationStructureInstanceKHR> tlas;
		auto view = scene.GetRegistry().view<MeshComponent, TransformComponent>();
		int i = 0;
		for (auto& entity : view)
		{
			const auto& [meshComponent, transformComponent] = view.get<MeshComponent, TransformComponent>(entity);
			VkAccelerationStructureInstanceKHR rayInst{};
			rayInst.transform = transformComponent.transform.GetKhrMat();
			rayInst.instanceCustomIndex = i;
			rayInst.accelerationStructureReference = GetBlasDeviceAddress(i);
			rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			rayInst.mask = 0xFF;
			rayInst.instanceShaderBindingTableRecordOffset = 0;
			tlas.emplace_back(rayInst);
			i++;
		}

		//VL_CORE_ASSERT(m_tlas.accel == VK_NULL_HANDLE || update, "Cannot call buildTlas twice except to update");
		uint32_t instanceCount = static_cast<uint32_t>(tlas.size());

		// Create a buffer holding the actual instance data (matrices++) for use by the AS builder

		Ref<Buffer> stagingBuffer = std::make_shared<Buffer>(instanceCount * sizeof(VkAccelerationStructureInstanceKHR), 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, Device::GetAccelerationProperties().minAccelerationStructureScratchOffsetAlignment);
		stagingBuffer->Map();
		stagingBuffer->WriteToBuffer(tlas.data());

		Ref<Buffer> instancesBuffer = std::make_shared<Buffer>(instanceCount * sizeof(VkAccelerationStructureInstanceKHR), 1, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Device::GetAccelerationProperties().minAccelerationStructureScratchOffsetAlignment);
		instancesBuffer->CopyBuffer(stagingBuffer->GetBuffer(), instancesBuffer->GetBuffer(), instancesBuffer->GetBufferSize(), Device::GetGraphicsQueue(), Device::GetCommandPool());

		VkCommandBuffer cmdBuf;
		Device::BeginSingleTimeCommands(cmdBuf, Device::GetCommandPool());

		// Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);

		// Creating the TLAS
		Ref<Buffer> scratchBuffer;
		CmdCreateTlas(cmdBuf, instanceCount, instancesBuffer->GetDeviceAddress(), scratchBuffer, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, false);

		// Finalizing and destroying temporary data
		Device::EndSingleTimeCommands(cmdBuf, Device::GetGraphicsQueue(), Device::GetCommandPool());
		stagingBuffer->Unmap();
	}

	void AccelerationStructure::CreateBottomLevelAS(Scene& scene)
	{
		// BLAS - Storing each primitive in a geometry
		std::vector<BlasInput> allBlas;

		auto view = scene.GetRegistry().view<MeshComponent, TransformComponent>();
		for (auto entity : view)
		{
			MeshComponent& meshComponent = view.get<MeshComponent>(entity);

			BlasInput blas = MeshToGeometry(meshComponent.Mesh);

			allBlas.emplace_back(blas);
		}

		uint32_t     nbBlas = static_cast<uint32_t>(allBlas.size());
		VkDeviceSize totalSize{ 0 };     // Memory size of all allocated BLAS
		uint32_t     nbCompactions{ 0 };   // Nb of BLAS requesting compaction
		VkDeviceSize maxScratchSize{ 0 };  // Largest scratch size

		std::vector<BuildAccelerationStructure> buildAs(nbBlas);
		for (uint32_t idx = 0; idx < nbBlas; idx++)
		{
			// Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
			// Other information will be filled in the createBlas (see #2)
			buildAs[idx].BuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
			buildAs[idx].BuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildAs[idx].BuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildAs[idx].BuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			buildAs[idx].BuildInfo.geometryCount = static_cast<uint32_t>(allBlas[idx].AsGeometry.size());
			buildAs[idx].BuildInfo.pGeometries = allBlas[idx].AsGeometry.data();

			// Build range information
			buildAs[idx].RangeInfo = allBlas[idx].AsOffset.data();

			// Build size information
			buildAs[idx].SizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

			// Finding sizes to create acceleration structures and scratch
			std::vector<uint32_t> maxPrimCount(allBlas[idx].AsOffset.size());
			for (auto tt = 0; tt < allBlas[idx].AsOffset.size(); tt++)
				maxPrimCount[tt] = allBlas[idx].AsOffset[tt].primitiveCount;  // Number of primitives/triangles
			Device::vkGetAccelerationStructureBuildSizesKHR(Device::GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&buildAs[idx].BuildInfo, maxPrimCount.data(), &buildAs[idx].SizeInfo);

			// Extra info
			totalSize += buildAs[idx].SizeInfo.accelerationStructureSize;
			maxScratchSize = std::max(maxScratchSize, buildAs[idx].SizeInfo.buildScratchSize);
			if ((buildAs[idx].BuildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) != 0)
			{
				nbCompactions++;
			}
		}

		// Allocate the scratch buffers holding the temporary data of the acceleration structure builder
		Ref<Buffer> scratchBuffer = std::make_shared<Buffer>(maxScratchSize, 1, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Device::GetAccelerationProperties().minAccelerationStructureScratchOffsetAlignment);
		VkDeviceAddress scratchAddress = scratchBuffer->GetDeviceAddress();

		// Allocate a query pool for storing the needed size for every BLAS compaction.
		VkQueryPool queryPool{ VK_NULL_HANDLE };
		if (nbCompactions > 0)  // Is compaction requested?
		{
			VL_CORE_ASSERT(nbCompactions == nbBlas, "Don't allow mix of on/off compaction");
			VkQueryPoolCreateInfo qpci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
			qpci.queryCount = nbBlas;
			qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
			vkCreateQueryPool(Device::GetDevice(), &qpci, nullptr, &queryPool);
		}

		// Batching creation/compaction of BLAS to allow staying in restricted amount of memory
		std::vector<uint32_t> indices;  // Indices of the BLAS to create
		VkDeviceSize          batchSize{ 0 };
		VkDeviceSize          batchLimit{ 256'000'000 };  // 256 MB
		for (uint32_t idx = 0; idx < nbBlas; idx++)
		{
			indices.push_back(idx);
			batchSize += buildAs[idx].SizeInfo.accelerationStructureSize;
			// Over the limit or last BLAS element
			if (batchSize >= batchLimit || idx == nbBlas - 1)
			{
				VkCommandBuffer cmdBuf;
				Device::BeginSingleTimeCommands(cmdBuf, Device::GetCommandPool());
				CmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
				Device::EndSingleTimeCommands(cmdBuf, Device::GetGraphicsQueue(), Device::GetCommandPool());

				if (queryPool)
				{
					VkCommandBuffer cmdBuf;
					Device::BeginSingleTimeCommands(cmdBuf, Device::GetCommandPool());
					CmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
					Device::EndSingleTimeCommands(cmdBuf, Device::GetGraphicsQueue(), Device::GetCommandPool());

					// Destroy the non-compacted version
					DestroyNonCompacted(indices, buildAs);
				}
				// Reset

				batchSize = 0;
				indices.clear();
			}
		}

		// Keeping all the created acceleration structures
		for (auto& b : buildAs)
		{
			m_Blas.emplace_back(b.As);
		}

		vkDestroyQueryPool(Device::GetDevice(), queryPool, nullptr);
	}
}
