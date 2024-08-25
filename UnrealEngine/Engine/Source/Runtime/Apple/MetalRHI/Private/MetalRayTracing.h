// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	MetalRayTracing.h: MetalRT Implementation
==============================================================================*/

#pragma once
#include "MetalRHIPrivate.h"

#if METAL_RHI_RAYTRACING

THIRD_PARTY_INCLUDES_START
#include "MetalInclude.h"
THIRD_PARTY_INCLUDES_END

struct FMetalRayTracingGeometryParameters
{
	FMetalRHIBuffer* IndexBuffer;
	FMetalRHIBuffer* VertexBuffer;
	uint64 RootConstantsBufferOffsetInBytes;
	uint64 VertexBufferOffset;
};

class FMetalRayTracingGeometry : public FRHIRayTracingGeometry
{
public:
	FMetalRayTracingGeometry(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& InInitializer);
	~FMetalRayTracingGeometry();

	void ReleaseUnderlyingResource();

	/** FRHIRayTracingGeometry Interface */
	virtual FRayTracingAccelerationStructureAddress GetAccelerationStructureAddress(uint64 GPUIndex) const final override { return (FRayTracingAccelerationStructureAddress)SceneIndex;
	}
	virtual void SetInitializer(const FRayTracingGeometryInitializer& Initializer) final override;
	/** FRHIRayTracingGeometry Interface */

	void Swap(FMetalRayTracingGeometry& Other);
	void RebuildDescriptors();

	void RemoveCompactionRequest();

	using FRHIRayTracingGeometry::Initializer;
	using FRHIRayTracingGeometry::SizeInfo;

	MTL::PrimitiveAccelerationStructureDescriptor* AccelerationStructureDescriptor;

	bool bHasPendingCompactionRequests;
	uint32_t CompactionSizeIndex;

	uint32_t SceneIndex; // TODO: Workaround since we can't provide a GPU VA when we build the instance descriptors for the TLAS (we need to use the AS index instead).

	static constexpr uint32 MaxNumAccelerationStructure = 2;
	static constexpr uint32 IndicesPerPrimitive = 3; // Triangle geometry only

	inline TRefCountPtr<FMetalRHIBuffer> GetAccelerationStructureRead()
	{
		return AccelerationStructure[AccelerationStructureIndex];
	}

	inline TRefCountPtr<FMetalRHIBuffer> GetAccelerationStructureWrite()
	{
		uint32 NextAccelerationStructure = (AccelerationStructureIndex + 1) % MaxNumAccelerationStructure;
		return AccelerationStructure[NextAccelerationStructure];
	}

	inline void NextAccelerationStructure()
	{
		AccelerationStructureIndex = (++AccelerationStructureIndex % MaxNumAccelerationStructure);
	}

private:
	NSMutableArray<MTLAccelerationStructureGeometryDescriptor*>* GeomArray;

	uint32 AccelerationStructureIndex;
	TRefCountPtr<FMetalRHIBuffer> AccelerationStructure[MaxNumAccelerationStructure];
};

class FMetalRayTracingScene : public FRHIRayTracingScene
{
public:
	FMetalRayTracingScene(FRayTracingSceneInitializer2 InInitializer);
	virtual ~FMetalRayTracingScene();

	void BindBuffer(FRHIBuffer* InBuffer, uint32 InBufferOffset);
	void BuildAccelerationStructure(
		FMetalRHICommandContext& CommandContext,
		FMetalRHIBuffer* ScratchBuffer, uint32 ScratchOffset,
		FMetalRHIBuffer* InstanceBuffer, uint32 InstanceOffset);

	void BuildPerInstanceGeometryParameterBuffer();

	inline const FRayTracingSceneInitializer2& GetInitializer() const override final { return Initializer; }
	inline uint32 GetLayerBufferOffset(uint32 LayerIndex) const override final { return Layers[LayerIndex].BufferOffset; }

	TRefCountPtr<FMetalShaderResourceView> InstanceBufferSRV;

	struct FLayerData
	{
		TRefCountPtr<FMetalShaderResourceView> ShaderResourceView;
		uint32 BufferOffset;
		uint32 ScratchBufferOffset;
		FRayTracingAccelerationStructureSize SizeInfo;
	};
	TArray<FLayerData> Layers;

private:
	friend class FMetalRHICommandContext;

private:
	/** The initializer provided to build the scene. Contains all the free standing stuff that used to be owned by the RT implementation. */
	const FRayTracingSceneInitializer2 Initializer;

	/** Acceleration Structure for the whole scene (shared between each layer). */
	TRefCountPtr<FMetalRHIBuffer> AccelerationStructureBuffer;

	/** Root Constants for geometry evaluation in HitGroup/Miss (emulates D3D12 RootConstants with a global scope). */
	TArray<FMetalRayTracingGeometryParameters> PerInstanceGeometryParameters;

	// Buffer that contains per-instance index and vertex buffer binding data
	TRefCountPtr<FMetalRHIBuffer> PerInstanceGeometryParameterBuffer;
	TRefCountPtr<FMetalShaderResourceView> PerInstanceGeometryParameterSRV;

	/** Segments descriptors  (populated when the constructor is called). */
	NSMutableArray<id<MTLAccelerationStructure>>* MutableAccelerationStructures;
};
#endif // METAL_RHI_RAYTRACING
