// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "RHIResources.h"

class FRHICommandList;
class FRHIComputeCommandList;

/** Compact representation of a bound shader parameter (read: value). Its offsets are for referencing their data in an associated blob. */
struct FRHIShaderParameter
{
	FRHIShaderParameter(uint16 InBufferIndex, uint16 InBaseIndex, uint16 InByteOffset, uint16 InByteSize)
		: BufferIndex(InBufferIndex)
		, BaseIndex(InBaseIndex)
		, ByteOffset(InByteOffset)
		, ByteSize(InByteSize)
	{
	}
	uint16 BufferIndex;
	uint16 BaseIndex;
	uint16 ByteOffset;
	uint16 ByteSize;
};

/** Compact representation of a bound resource parameter (Texture, SRV, UAV, SamplerState, or UniformBuffer) */
struct FRHIShaderParameterResource
{
	enum class EType : uint8
	{
		Texture,
		ResourceView,
		UnorderedAccessView,
		Sampler,
		UniformBuffer,
	};

	FRHIShaderParameterResource() = default;
	FRHIShaderParameterResource(EType InType, FRHIResource* InResource, uint16 InIndex)
		: Resource(InResource)
		, Index(InIndex)
		, Type(InType)
	{
	}
	FRHIShaderParameterResource(FRHITexture* InTexture, uint16 InIndex)
		: FRHIShaderParameterResource(FRHIShaderParameterResource::EType::Texture, InTexture, InIndex)
	{
	}
	FRHIShaderParameterResource(FRHIShaderResourceView* InView, uint16 InIndex)
		: FRHIShaderParameterResource(FRHIShaderParameterResource::EType::ResourceView, InView, InIndex)
	{
	}
	FRHIShaderParameterResource(FRHIUnorderedAccessView* InUAV, uint16 InIndex)
		: FRHIShaderParameterResource(FRHIShaderParameterResource::EType::UnorderedAccessView, InUAV, InIndex)
	{
	}
	FRHIShaderParameterResource(FRHISamplerState* InSamplerState, uint16 InIndex)
		: FRHIShaderParameterResource(FRHIShaderParameterResource::EType::Sampler, InSamplerState, InIndex)
	{
	}
	FRHIShaderParameterResource(FRHIUniformBuffer* InUniformBuffer, uint16 InIndex)
		: FRHIShaderParameterResource(FRHIShaderParameterResource::EType::UniformBuffer, InUniformBuffer, InIndex)
	{
	}

	FRHIResource* Resource = nullptr;
	uint16        Index = 0;
	EType         Type = EType::Texture;
};

/** Collection of parameters to set in the RHI. These parameters aren't bound to any specific shader until SetBatchedShaderParameters is called. */
struct FRHIBatchedShaderParameters
{
	TArray<uint8> ParametersData;
	TArray<FRHIShaderParameter> Parameters;
	TArray<FRHIShaderParameterResource> ResourceParameters;
	TArray<FRHIShaderParameterResource> BindlessParameters;

	inline bool HasParameters() const
	{
		return (Parameters.Num() + ResourceParameters.Num() + BindlessParameters.Num()) > 0;
	}

	void Reset()
	{
		ParametersData.Reset();
		Parameters.Reset();
		ResourceParameters.Reset();
		BindlessParameters.Reset();
	}

	FORCEINLINE_DEBUGGABLE void SetShaderParameter(uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		const int32 DestDataOffset = ParametersData.Num();
		ParametersData.Append((const uint8*)NewValue, NumBytes);
		Parameters.Emplace((uint16)BufferIndex, (uint16)BaseIndex, (uint16)DestDataOffset, (uint16)NumBytes);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(uint32 Index, FRHIUniformBuffer* UniformBuffer)
	{
		ResourceParameters.Emplace(UniformBuffer, (uint16)Index);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderTexture(uint32 Index, FRHITexture* Texture)
	{
		ResourceParameters.Emplace(Texture, (uint16)Index);
	}
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(uint32 Index, FRHIShaderResourceView* SRV)
	{
		ResourceParameters.Emplace(SRV, (uint16)Index);
	}
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(uint32 Index, FRHISamplerState* State)
	{
		ResourceParameters.Emplace(State, (uint16)Index);
	}
	FORCEINLINE_DEBUGGABLE void SetUAVParameter(uint32 Index, FRHIUnorderedAccessView* UAV)
	{
		ResourceParameters.Emplace(UAV, (uint16)Index);
	}

	FORCEINLINE_DEBUGGABLE void SetBindlessTexture(uint32 Index, FRHITexture* Texture)
	{
		BindlessParameters.Emplace(Texture, (uint16)Index);
	}
	FORCEINLINE_DEBUGGABLE void SetBindlessResourceView(uint32 Index, FRHIShaderResourceView* SRV)
	{
		BindlessParameters.Emplace(SRV, (uint16)Index);
	}
	FORCEINLINE_DEBUGGABLE void SetBindlessSampler(uint32 Index, FRHISamplerState* State)
	{
		BindlessParameters.Emplace(State, (uint16)Index);
	}
	FORCEINLINE_DEBUGGABLE void SetBindlessUAV(uint32 Index, FRHIUnorderedAccessView* UAV)
	{
		BindlessParameters.Emplace(UAV, (uint16)Index);
	}
};

/** Compact representation of a resource parameter unbind, limited to  SRVs and UAVs */
struct FRHIShaderParameterUnbind
{
	enum class EType : uint8
	{
		ResourceView,
		UnorderedAccessView,
	};

	FRHIShaderParameterUnbind() = default;
	FRHIShaderParameterUnbind(EType InType, uint16 InIndex)
		: Index(InIndex)
		, Type(InType)
	{
	}

	uint16  Index = 0;
	EType   Type = EType::ResourceView;
};

/** Collection of parameters to unbind in the RHI. These unbinds aren't tied to any specific shader until SetBatchedShaderUnbinds is called. */
struct FRHIBatchedShaderUnbinds
{
	TArray<FRHIShaderParameterUnbind> Unbinds;

	bool HasParameters() const
	{
		return Unbinds.Num() > 0;
	}

	void Reset()
	{
		Unbinds.Reset();
	}

	void UnsetSRV(uint32 Index)
	{
		Unbinds.Emplace(FRHIShaderParameterUnbind::EType::ResourceView, (uint16)Index);
	}
	void UnsetUAV(uint32 Index)
	{
		Unbinds.Emplace(FRHIShaderParameterUnbind::EType::UnorderedAccessView, (uint16)Index);
	}
};

struct FRHIShaderBundleDispatch
{
	uint32 RecordIndex = ~uint32(0u);
	class FComputePipelineState* PipelineState = nullptr;
	FRHIComputeShader* Shader = nullptr;
	FRHIComputePipelineState* RHIPipeline = nullptr;
	FRHIBatchedShaderParameters Parameters;
	FUint32Vector4 Constants;

	inline bool IsValid() const
	{
		return RecordIndex != ~uint32(0u);
	}
};