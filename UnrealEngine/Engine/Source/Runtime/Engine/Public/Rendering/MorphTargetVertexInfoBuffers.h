// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"

struct FSkelMeshRenderSection;
class UMorphTarget;

class FMorphTargetVertexInfoBuffers : public FRenderResource
{
public:
	ENGINE_API FMorphTargetVertexInfoBuffers() : NumTotalBatches(0)
	{
	}

	ENGINE_API void InitMorphResources(EShaderPlatform ShaderPlatform, const TArray<FSkelMeshRenderSection>& RenderSections, const TArray<UMorphTarget*>& MorphTargets, int NumVertices, int32 LODIndex, float TargetPositionErrorTolerance);

	inline bool IsMorphResourcesInitialized() const { return bResourcesInitialized; }
	inline bool IsRHIIntialized() const { return bRHIIntialized; }
	inline bool IsMorphCPUDataValid() const{ return bIsMorphCPUDataValid; }
	
	ENGINE_API bool GetEmptyMorphCPUDataOnInitRHI() const { return bEmptyMorphCPUDataOnInitRHI; }
	ENGINE_API void SetEmptyMorphCPUDataOnInitRHI(bool bEmpty) { bEmptyMorphCPUDataOnInitRHI = bEmpty; }

	ENGINE_API virtual void InitRHI() override;
	ENGINE_API virtual void ReleaseRHI() override;

	static uint32 GetMaximumThreadGroupSize()
	{
		//D3D11 there can be at most 65535 Thread Groups in each dimension of a Dispatch call.
		uint64 MaximumThreadGroupSize = uint64(GMaxComputeDispatchDimension) * 32ull;
		return uint32(FMath::Min<uint64>(MaximumThreadGroupSize, UINT32_MAX));
	}

	ENGINE_API uint32 GetNumBatches(uint32 index = UINT_MAX) const
	{
		check(index == UINT_MAX || index < (uint32)BatchesPerMorph.Num());
		return index != UINT_MAX ? BatchesPerMorph[index] : NumTotalBatches;
	}

	ENGINE_API uint32 GetNumMorphs() const
	{
		return BatchesPerMorph.Num();
	}

	uint32 GetBatchStartOffset(uint32 Index) const
	{
		check(Index < (uint32)BatchStartOffsetPerMorph.Num());
		return BatchStartOffsetPerMorph[Index];
	}

	const FVector4f& GetMaximumMorphScale(uint32 Index) const
	{
		check(Index < (uint32)MaximumValuePerMorph.Num());
		return MaximumValuePerMorph[Index];
	}

	const FVector4f& GetMinimumMorphScale(uint32 Index) const
	{
		check(Index < (uint32)MinimumValuePerMorph.Num());
		return MinimumValuePerMorph[Index];
	}

	const float GetPositionPrecision() const
	{
		return PositionPrecision;
	}

	static const float CalculatePositionPrecision(float TargetPositionErrorTolerance);

	const float GetTangentZPrecision() const
	{
		return TangentZPrecision;
	}

	static bool IsPlatformShaderSupported(EShaderPlatform ShaderPlatform);

	FBufferRHIRef MorphDataBuffer;
	FShaderResourceViewRHIRef MorphDataSRV;

	/** Create an RHI vertex buffer with CPU data. CPU data may be discarded after creation (see TResourceArray::Discard) */
	FBufferRHIRef CreateMorphRHIBuffer_RenderThread();
	FBufferRHIRef CreateMorphRHIBuffer_Async();

	/** Similar to Init/ReleaseRHI but only update existing SRV so references to the SRV stays valid */
	template <uint32 MaxNumUpdates>
	void InitRHIForStreaming(
		FRHIBuffer* IntermediatMorphTargetBuffer,
		TRHIResourceUpdateBatcher<MaxNumUpdates>& Batcher)
	{
		if (MorphDataBuffer && IntermediatMorphTargetBuffer)
		{
			Batcher.QueueUpdateRequest(MorphDataBuffer, IntermediatMorphTargetBuffer);
			Batcher.QueueUpdateRequest(MorphDataSRV, MorphDataBuffer);
		}
	}

	template<uint32 MaxNumUpdates>
	void ReleaseRHIForStreaming(TRHIResourceUpdateBatcher<MaxNumUpdates>& Batcher)
	{
		if (MorphDataBuffer)
		{
			Batcher.QueueUpdateRequest(MorphDataBuffer, nullptr);
		}
		if (MorphDataSRV)
		{
			Batcher.QueueUpdateRequest(MorphDataSRV, nullptr, 0, 0);
		}
	}

protected:
	void ResetCPUData()
	{
		MorphData.Empty();
		MaximumValuePerMorph.Empty();
		MinimumValuePerMorph.Empty();
		BatchStartOffsetPerMorph.Empty();
		BatchesPerMorph.Empty();
		NumTotalBatches = 0;
		PositionPrecision = 0.0f;
		TangentZPrecision = 0.0f;
		bResourcesInitialized = false;
		bIsMorphCPUDataValid = false;
	}

	void ValidateVertexBuffers(bool bMorphTargetsShouldBeValid);
	void Serialize(FArchive& Ar);

	// Transient data. Gets deleted as soon as the GPU resource has been initialized (unless excplicitly disabled by bEmptyMorphCPUDataOnInitRHI).
	TResourceArray<uint32> MorphData;

	//x,y,y separate for position and shared w for tangent
	TArray<FVector4f> MaximumValuePerMorph;
	TArray<FVector4f> MinimumValuePerMorph;
	TArray<uint32> BatchStartOffsetPerMorph;
	TArray<uint32> BatchesPerMorph;
	
	uint32 NumTotalBatches;
	float PositionPrecision;
	float TangentZPrecision;

	bool bIsMorphCPUDataValid = false;
	bool bResourcesInitialized = false;
	bool bRHIIntialized = false;
	bool bEmptyMorphCPUDataOnInitRHI = true;

	friend class FSkeletalMeshLODRenderData;
	ENGINE_API friend FArchive& operator<<(FArchive& Ar, FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers);

private:
	template <bool bRenderThread>
	FBufferRHIRef CreateMorphRHIBuffer_Internal();
};

ENGINE_API FArchive& operator<<(FArchive& Ar, FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers);
