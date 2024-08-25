// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommands.cpp: Vulkan RHI commands implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "EngineGlobals.h"
#include "VulkanLLM.h"
#include "RenderUtils.h"
#include "GlobalRenderResources.h"
#include "RHIShaderParametersShared.h"
#include "RHIUtilities.h"
#include "RHICoreShader.h"

static TAutoConsoleVariable<int32> GCVarSubmitOnDispatch(
	TEXT("r.Vulkan.SubmitOnDispatch"),
	0,
	TEXT("0 to not do anything special on dispatch(default)\n")\
	TEXT("1 to submit the cmd buffer after each dispatch"),
	ECVF_RenderThreadSafe
);

int32 GVulkanSubmitAfterEveryEndRenderPass = 0;
static FAutoConsoleVariableRef CVarVulkanSubmitAfterEveryEndRenderPass(
	TEXT("r.Vulkan.SubmitAfterEveryEndRenderPass"),
	GVulkanSubmitAfterEveryEndRenderPass,
	TEXT("Forces a submit after every end render pass.\n")
	TEXT(" 0: Don't(default)\n")
	TEXT(" 1: Enable submitting"),
	ECVF_Default
);

// make sure what the hardware expects matches what we give it for indirect arguments
static_assert(sizeof(FRHIDrawIndirectParameters) == sizeof(VkDrawIndirectCommand), "FRHIDrawIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, VertexCountPerInstance) == STRUCT_OFFSET(VkDrawIndirectCommand, vertexCount), "Wrong offset of FRHIDrawIndirectParameters::VertexCountPerInstance.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, InstanceCount) == STRUCT_OFFSET(VkDrawIndirectCommand, instanceCount), "Wrong offset of FRHIDrawIndirectParameters::InstanceCount.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, StartVertexLocation) == STRUCT_OFFSET(VkDrawIndirectCommand, firstVertex), "Wrong offset of FRHIDrawIndirectParameters::StartVertexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, StartInstanceLocation) == STRUCT_OFFSET(VkDrawIndirectCommand, firstInstance), "Wrong offset of FRHIDrawIndirectParameters::StartInstanceLocation.");

static_assert(sizeof(FRHIDrawIndexedIndirectParameters) == sizeof(VkDrawIndexedIndirectCommand), "FRHIDrawIndexedIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, IndexCountPerInstance) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, indexCount), "Wrong offset of FRHIDrawIndexedIndirectParameters::IndexCountPerInstance.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, InstanceCount) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, instanceCount), "Wrong offset of FRHIDrawIndexedIndirectParameters::InstanceCount.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, StartIndexLocation) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, firstIndex), "Wrong offset of FRHIDrawIndexedIndirectParameters::StartIndexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, BaseVertexLocation) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, vertexOffset), "Wrong offset of FRHIDrawIndexedIndirectParameters::BaseVertexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, StartInstanceLocation) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, firstInstance), "Wrong offset of FRHIDrawIndexedIndirectParameters::StartInstanceLocation.");

static_assert(sizeof(FRHIDispatchIndirectParameters) == sizeof(VkDispatchIndirectCommand), "FRHIDispatchIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountX) == STRUCT_OFFSET(VkDispatchIndirectCommand, x), "FRHIDispatchIndirectParameters X dimension is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountY) == STRUCT_OFFSET(VkDispatchIndirectCommand, y), "FRHIDispatchIndirectParameters Y dimension is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountZ) == STRUCT_OFFSET(VkDispatchIndirectCommand, z), "FRHIDispatchIndirectParameters Z dimension is wrong.");

static FORCEINLINE ShaderStage::EStage GetAndVerifyShaderStage(FRHIGraphicsShader* ShaderRHI, FVulkanPendingGfxState* PendingGfxState)
{
	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:
		check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Vertex) == GetShaderKey<FVulkanVertexShader>(ShaderRHI));
		return ShaderStage::Vertex;
	case SF_Geometry:
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Geometry) == GetShaderKey<FVulkanGeometryShader>(ShaderRHI));
		return ShaderStage::Geometry;
#else
		checkf(0, TEXT("Geometry shaders not supported on this platform!"));
		UE_LOG(LogVulkanRHI, Fatal, TEXT("Geometry shaders not supported on this platform!"));
		break;
#endif
	case SF_Pixel:
		check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Pixel) == GetShaderKey<FVulkanPixelShader>(ShaderRHI));
		return ShaderStage::Pixel;
	default:
		checkf(0, TEXT("Undefined FRHIShader Type %d!"), (int32)ShaderRHI->GetFrequency());
		break;
	}

	return ShaderStage::Invalid;
}

static FORCEINLINE ShaderStage::EStage GetAndVerifyShaderStageAndVulkanShader(FRHIGraphicsShader* ShaderRHI, FVulkanPendingGfxState* PendingGfxState, FVulkanShader*& OutShader)
{
	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:
		//check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Vertex) == GetShaderKey<FVulkanVertexShader>(ShaderRHI));
		OutShader = static_cast<FVulkanVertexShader*>(static_cast<FRHIVertexShader*>(ShaderRHI));
		return ShaderStage::Vertex;
	case SF_Geometry:
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		//check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Geometry) == GetShaderKey<FVulkanGeometryShader>(ShaderRHI));
		OutShader = static_cast<FVulkanGeometryShader*>(static_cast<FRHIGeometryShader*>(ShaderRHI));
		return ShaderStage::Geometry;
#else
		checkf(0, TEXT("Geometry shaders not supported on this platform!"));
		UE_LOG(LogVulkanRHI, Fatal, TEXT("Geometry shaders not supported on this platform!"));
		break;
#endif
	case SF_Pixel:
		//check(PendingGfxState->GetCurrentShaderKey(ShaderStage::Pixel) == GetShaderKey<FVulkanPixelShader>(ShaderRHI));
		OutShader = static_cast<FVulkanPixelShader*>(static_cast<FRHIPixelShader*>(ShaderRHI));
		return ShaderStage::Pixel;
	default:
		checkf(0, TEXT("Undefined FRHIShader Type %d!"), (int32)ShaderRHI->GetFrequency());
		break;
	}

	OutShader = nullptr;
	return ShaderStage::Invalid;
}

void FVulkanCommandListContext::RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBufferRHI, uint32 Offset)
{
	FVulkanResourceMultiBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if (VertexBuffer != nullptr)
	{
		PendingGfxState->SetStreamSource(StreamIndex, VertexBuffer->GetHandle(), Offset + VertexBuffer->GetOffset());
	}
}

template <class PendingStateType>
struct FVulkanResourceBinder
{
	FVulkanCommandListContext& Context;
	const EShaderFrequency Frequency;
	const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& GlobalRemappingInfo;
	PendingStateType* PendingState;

	FVulkanResourceBinder(FVulkanCommandListContext& InContext, EShaderFrequency InFrequency, const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& InGlobalRemappingInfo, PendingStateType* InPendingState)
		: Context(InContext)
		, Frequency(InFrequency)
		, GlobalRemappingInfo(InGlobalRemappingInfo)
		, PendingState(InPendingState)
	{
	}

	void SetUAV(FRHIUnorderedAccessView* UAV, uint8 Index, bool bClearResources = false)
	{
		if (bClearResources)
		{
			//Context.ClearShaderResources(UAV);
		}

		PendingState->SetUAVForUBResource(GlobalRemappingInfo[Index].NewDescriptorSet, GlobalRemappingInfo[Index].NewBindingIndex, ResourceCast(UAV));
	}

	void SetSRV(FRHIShaderResourceView* SRV, uint8 Index)
	{
		PendingState->SetSRVForUBResource(GlobalRemappingInfo[Index].NewDescriptorSet, GlobalRemappingInfo[Index].NewBindingIndex, ResourceCast(SRV));
	}

	void SetTexture(FRHITexture* TextureRHI, uint8 Index)
	{
		FVulkanTexture* VulkanTexture = ResourceCast(TextureRHI);
		const ERHIAccess RHIAccess = (Frequency == SF_Compute) ? ERHIAccess::SRVCompute : ERHIAccess::SRVGraphics;
		const VkImageLayout ExpectedLayout = FVulkanLayoutManager::GetDefaultLayout(Context.GetCommandBufferManager()->GetActiveCmdBuffer(), *VulkanTexture, RHIAccess);
		PendingState->SetTextureForUBResource(GlobalRemappingInfo[Index].NewDescriptorSet, GlobalRemappingInfo[Index].NewBindingIndex, VulkanTexture, ExpectedLayout);
	}

	void SetSampler(FRHISamplerState* Sampler, uint8 Index)
	{
		PendingState->SetSamplerStateForUBResource(GlobalRemappingInfo[Index].NewDescriptorSet, GlobalRemappingInfo[Index].NewBindingIndex, ResourceCast(Sampler));
	}
};

template <class ShaderType> 
void FVulkanCommandListContext::SetResourcesFromTables(const ShaderType* Shader)
{
	checkSlow(Shader);

	static constexpr EShaderFrequency Frequency = static_cast<EShaderFrequency>(ShaderType::StaticFrequency);

	if (Frequency == SF_Compute)
	{
		const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& GlobalRemappingInfo =
			PendingComputeState->CurrentState->GetComputePipelineDescriptorInfo().GetGlobalRemappingInfo();

		FVulkanResourceBinder Binder(*this, Frequency, GlobalRemappingInfo, PendingComputeState);
		UE::RHICore::SetResourcesFromTables(
			Binder
			, *Shader
			, Shader->ShaderResourceTable
			, DirtyUniformBuffers[Frequency]
			, BoundUniformBuffers[Frequency]
#if ENABLE_RHI_VALIDATION
			, Tracker
#endif
		);
	}
	else
	{
		const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& GlobalRemappingInfo =
		PendingGfxState->CurrentState->GetGfxPipelineDescriptorInfo().GetGlobalRemappingInfo(ShaderStage::GetStageForFrequency(Frequency));

		FVulkanResourceBinder Binder(*this, Frequency, GlobalRemappingInfo, PendingGfxState);
		UE::RHICore::SetResourcesFromTables(
			Binder
			, *Shader
			, Shader->ShaderResourceTable
			, DirtyUniformBuffers[Frequency]
			, BoundUniformBuffers[Frequency]
#if ENABLE_RHI_VALIDATION
			, Tracker
#endif
		);
	}
}

void FVulkanCommandListContext::CommitGraphicsResourceTables()
{
	checkSlow(PendingGfxState);

	if (const FVulkanShader* Shader = PendingGfxState->GetCurrentShader(SF_Vertex))
	{
		checkSlow(Shader->Frequency == SF_Vertex);
		const FVulkanVertexShader* VertexShader = static_cast<const FVulkanVertexShader*>(Shader);
		SetResourcesFromTables(VertexShader);
	}

	if (const FVulkanShader* Shader = PendingGfxState->GetCurrentShader(SF_Pixel))
	{
		checkSlow(Shader->Frequency == SF_Pixel);
		const FVulkanPixelShader* PixelShader = static_cast<const FVulkanPixelShader*>(Shader);
		SetResourcesFromTables(PixelShader);
	}

#if PLATFORM_SUPPORTS_MESH_SHADERS
	// :todo-jn: mesh shaders
	//if (const FVulkanShader* Shader = PendingGfxState->GetCurrentShader(SF_Mesh))
	//{
	//  checkSlow(Shader->Frequency == SF_Mesh);
	//	const FVulkanMeshShader* MeshShader = static_cast<const FVulkanMeshShader*>(Shader);
	//	SetResourcesFromTables(MeshShader);
	//}
	//if (const FVulkanShader* Shader = PendingGfxState->GetCurrentShader(SF_Amplification))
	//{
	//  checkSlow(Shader->Frequency == SF_Amplification);
	//	const FVulkanAmplificationShader* AmplificationShader = static_cast<const FVulkanAmplificationShader*>(Shader);
	//	SetResourcesFromTables(AmplificationShader);
	//}
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	if (const FVulkanShader* Shader = PendingGfxState->GetCurrentShader(SF_Geometry))
	{
		checkSlow(Shader->Frequency == SF_Geometry);
		const FVulkanGeometryShader* GeometryShader = static_cast<const FVulkanGeometryShader*>(Shader);
		SetResourcesFromTables(GeometryShader);
	}
#endif
}

void FVulkanCommandListContext::CommitComputeResourceTables()
{
	SetResourcesFromTables(PendingComputeState->GetCurrentShader());
}


void FVulkanCommandListContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallTime);
#endif

	CommitComputeResourceTables();

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	ensure(Cmd->IsOutsideRenderPass());
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingComputeState->PrepareForDispatch(Cmd);

	VulkanRHI::vkCmdDispatch(CmdBuffer, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	if (GCVarSubmitOnDispatch.GetValueOnRenderThread())
	{
		InternalSubmitActiveCmdBuffer();
	}

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUDispatch(FIntVector(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));	
	}

	VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer, 2);
}

void FVulkanCommandListContext::RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	static_assert(sizeof(FRHIDispatchIndirectParameters) == sizeof(VkDispatchIndirectCommand), "Dispatch indirect doesn't match!");
	FVulkanResourceMultiBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	CommitComputeResourceTables();

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	ensure(Cmd->IsOutsideRenderPass());
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingComputeState->PrepareForDispatch(Cmd);

	VulkanRHI::vkCmdDispatchIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentBuffer->GetOffset() + ArgumentOffset);

	if (GCVarSubmitOnDispatch.GetValueOnRenderThread())
	{
		InternalSubmitActiveCmdBuffer();
	}

	if (FVulkanPlatform::RegisterGPUWork()/* && IsImmediate()*/)
	{
		GpuProfiler.RegisterGPUDispatch(FIntVector(1, 1, 1));	
	}

	VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer, 2);
}

void FVulkanCommandListContext::RHISetUAVParameter(FRHIPixelShader* PixelShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI)
{
	if (UAVRHI)
	{
		FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
		PendingGfxState->SetUAVForStage(ShaderStage::Pixel, UAVIndex, UAV);
	}
}

void FVulkanCommandListContext::RHISetUAVParameter(FRHIComputeShader* ComputeShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI)
{
	if (UAVRHI)
	{
		check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

		FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
		PendingComputeState->SetUAVForStage(UAVIndex, UAV);
	}
}

void FVulkanCommandListContext::RHISetUAVParameter(FRHIComputeShader* ComputeShaderRHI,uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI, uint32 InitialCount)
{
	ensure(0);
}


void FVulkanCommandListContext::RHISetShaderTexture(FRHIGraphicsShader* ShaderRHI, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{
	FVulkanTexture* VulkanTexture = ResourceCast(NewTextureRHI);
	const VkImageLayout ExpectedLayout = FVulkanLayoutManager::GetDefaultLayout(GetCommandBufferManager()->GetActiveCmdBuffer(), *VulkanTexture, ERHIAccess::SRVGraphics);

	ShaderStage::EStage Stage = GetAndVerifyShaderStage(ShaderRHI, PendingGfxState);
	PendingGfxState->SetTextureForStage(Stage, TextureIndex, VulkanTexture, ExpectedLayout);
	NewTextureRHI->SetLastRenderTime((float)FPlatformTime::Seconds());
}

void FVulkanCommandListContext::RHISetShaderTexture(FRHIComputeShader* ComputeShaderRHI, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{
	FVulkanComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	check(PendingComputeState->GetCurrentShader() == ComputeShader);

	FVulkanTexture* VulkanTexture = ResourceCast(NewTextureRHI);
	const VkImageLayout ExpectedLayout = FVulkanLayoutManager::GetDefaultLayout(GetCommandBufferManager()->GetActiveCmdBuffer(), *VulkanTexture, ERHIAccess::SRVCompute);
	PendingComputeState->SetTextureForStage(TextureIndex, VulkanTexture, ExpectedLayout);
	NewTextureRHI->SetLastRenderTime((float)FPlatformTime::Seconds());
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FRHIGraphicsShader* ShaderRHI, uint32 TextureIndex, FRHIShaderResourceView* SRVRHI)
{
	if (SRVRHI)
	{
		ShaderStage::EStage Stage = GetAndVerifyShaderStage(ShaderRHI, PendingGfxState);
		FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
		PendingGfxState->SetSRVForStage(Stage, TextureIndex, SRV);
	}
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FRHIComputeShader* ComputeShaderRHI,uint32 TextureIndex, FRHIShaderResourceView* SRVRHI)
{
	if (SRVRHI)
	{
		check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

		FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
		PendingComputeState->SetSRVForStage(TextureIndex, SRV);
	}
}

void FVulkanCommandListContext::RHISetShaderSampler(FRHIGraphicsShader* ShaderRHI, uint32 SamplerIndex, FRHISamplerState* NewStateRHI)
{
	ShaderStage::EStage Stage = GetAndVerifyShaderStage(ShaderRHI, PendingGfxState);
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerStateForStage(Stage, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FRHIComputeShader* ComputeShaderRHI, uint32 SamplerIndex, FRHISamplerState* NewStateRHI)
{
	FVulkanComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	check(PendingComputeState->GetCurrentShader() == ComputeShader);

	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingComputeState->SetSamplerStateForStage(SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderParameter(FRHIGraphicsShader* ShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	ShaderStage::EStage Stage = GetAndVerifyShaderStage(ShaderRHI, PendingGfxState);
	PendingGfxState->SetPackedGlobalShaderParameter(Stage, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FRHIComputeShader* ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	FVulkanComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	check(PendingComputeState->GetCurrentShader() == ComputeShader);

	PendingComputeState->SetPackedGlobalShaderParameter(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameters(FRHIGraphicsShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters)
{
	UE::RHICore::RHISetShaderParametersShared(
		*this
		, Shader
		, InParametersData
		, InParameters
		, InResourceParameters
		, InBindlessParameters
	);
}

void FVulkanCommandListContext::RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters)
{
	UE::RHICore::RHISetShaderParametersShared(
		*this
		, Shader
		, InParametersData
		, InParameters
		, InResourceParameters
		, InBindlessParameters
	);
}

void FVulkanCommandListContext::RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers)
{
	FMemory::Memzero(GlobalUniformBuffers.GetData(), GlobalUniformBuffers.Num() * sizeof(FRHIUniformBuffer*));

	for (int32 Index = 0; Index < InUniformBuffers.GetUniformBufferCount(); ++Index)
	{
		GlobalUniformBuffers[InUniformBuffers.GetSlot(Index)] = InUniformBuffers.GetUniformBuffer(Index);
	}
}

void FVulkanCommandListContext::RHISetStaticUniformBuffer(FUniformBufferStaticSlot InSlot, FRHIUniformBuffer* InBuffer)
{
	GlobalUniformBuffers[InSlot] = InBuffer;
}

void FVulkanCommandListContext::RHISetUniformBufferDynamicOffset(FUniformBufferStaticSlot InSlot, uint32 InOffset)
{
	check(IsAligned(InOffset, Device->GetLimits().minUniformBufferOffsetAlignment));

	FVulkanUniformBuffer* UniformBuffer = ResourceCast(GlobalUniformBuffers[InSlot]);
	const FVulkanGfxPipelineDescriptorInfo& DescriptorInfo = PendingGfxState->CurrentState->GetGfxPipelineDescriptorInfo();

	static const ShaderStage::EStage Stages[2] = 
	{
		ShaderStage::Vertex,
		ShaderStage::Pixel
	};

	for (int32 i = 0; i < UE_ARRAY_COUNT(Stages); i++)
	{
		ShaderStage::EStage Stage = Stages[i];
		FVulkanShader* Shader = PendingGfxState->CurrentPipeline->VulkanShaders[Stage];
		if (Shader == nullptr)
		{
			continue;
		}

		const auto& StaticSlots = Shader->StaticSlots;

		for (int32 BufferIndex = 0; BufferIndex < StaticSlots.Num(); ++BufferIndex)
		{
			const FUniformBufferStaticSlot Slot = StaticSlots[BufferIndex];
			if (Slot == InSlot)
			{
				uint8 DescriptorSet;
				uint32 BindingIndex;
				if (DescriptorInfo.GetDescriptorSetAndBindingIndex(FVulkanShaderHeader::UniformBuffer, Stage, BufferIndex, DescriptorSet, BindingIndex))
				{
					// Uniform views always bind max supported range, so make sure Offset+Range is within buffer allocation
					check((InOffset + PLATFORM_MAX_UNIFORM_BUFFER_RANGE) <= UniformBuffer->Allocation.Size);
					uint32 DynamicOffset = InOffset + UniformBuffer->GetOffset();
					PendingGfxState->CurrentState->SetUniformBufferDynamicOffset(DescriptorSet, BindingIndex, DynamicOffset);
				}
				break;
			}
		}
	}
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FRHIGraphicsShader* ShaderRHI, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetUniformBufferTime);
#endif

	FVulkanShader* Shader = nullptr;
	const ShaderStage::EStage Stage = GetAndVerifyShaderStageAndVulkanShader(ShaderRHI, PendingGfxState, Shader);
	check(Shader->GetShaderKey() == PendingGfxState->GetCurrentShaderKey(Stage));

	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	const FVulkanShaderHeader& CodeHeader = Shader->GetCodeHeader();
	const FVulkanShaderHeader::FUniformBufferInfo& HeaderUBInfo = CodeHeader.UniformBuffers[BufferIndex];
	checkfSlow(!HeaderUBInfo.LayoutHash || HeaderUBInfo.LayoutHash == UniformBuffer->GetLayout().GetHash(), TEXT("Mismatched UB layout! Got hash 0x%x, expected 0x%x!"), UniformBuffer->GetLayout().GetHash(), HeaderUBInfo.LayoutHash);
	const FVulkanGfxPipelineDescriptorInfo& DescriptorInfo = PendingGfxState->CurrentState->GetGfxPipelineDescriptorInfo();

	if (!HeaderUBInfo.bOnlyHasResources)
	{
		checkSlow(UniformBuffer->GetLayout().ConstantBufferSize > 0);

		uint8 DescriptorSet;
		uint32 BindingIndex;
		if (!DescriptorInfo.GetDescriptorSetAndBindingIndex(FVulkanShaderHeader::UniformBuffer, Stage, BufferIndex, DescriptorSet, BindingIndex))
		{
			return;
		}

		const VkDescriptorType DescriptorType = DescriptorInfo.GetDescriptorType(DescriptorSet, BindingIndex);

		if (DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
		{
			PendingGfxState->SetUniformBuffer<true>(DescriptorSet, BindingIndex, UniformBuffer);
		}
		else
		{
			check(DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			PendingGfxState->SetUniformBuffer<false>(DescriptorSet, BindingIndex, UniformBuffer);
		}
	}

	if (HeaderUBInfo.ResourceEntries.Num())
	{
		checkSlow(Shader->Frequency < SF_NumStandardFrequencies);
		check(BufferIndex < MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE);
		BoundUniformBuffers[Shader->Frequency][BufferIndex] = UniformBuffer;
		DirtyUniformBuffers[Shader->Frequency] |= (1 << BufferIndex);
	}
	else
	{
		// Internal error: Completely empty UB!
		checkSlow(!HeaderUBInfo.bOnlyHasResources);
	}
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FRHIComputeShader* ComputeShaderRHI, uint32 BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	FVulkanComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	check(PendingComputeState->GetCurrentShader() == ComputeShader);

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetUniformBufferTime);
#endif
	FVulkanComputePipelineDescriptorState& State = *PendingComputeState->CurrentState;

	// Walk through all resources to set all appropriate states
	FVulkanComputeShader* Shader = ResourceCast(ComputeShaderRHI);
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);

	const FVulkanComputePipelineDescriptorInfo& DescriptorInfo = PendingComputeState->CurrentState->GetComputePipelineDescriptorInfo();
	const FVulkanShaderHeader& CodeHeader = Shader->GetCodeHeader();
	const FVulkanShaderHeader::FUniformBufferInfo& HeaderUBInfo = CodeHeader.UniformBuffers[BufferIndex];
	checkfSlow(!HeaderUBInfo.LayoutHash || HeaderUBInfo.LayoutHash == UniformBuffer->GetLayout().GetHash(), TEXT("Mismatched UB layout! Got hash 0x%x, expected 0x%x!"), UniformBuffer->GetLayout().GetHash(), HeaderUBInfo.LayoutHash);

	// Uniform Buffers
	if (!HeaderUBInfo.bOnlyHasResources)
	{
		checkSlow(UniformBuffer->GetLayout().ConstantBufferSize > 0);
		
		uint8 DescriptorSet;
		uint32 BindingIndex;
		if (!DescriptorInfo.GetDescriptorSetAndBindingIndex(FVulkanShaderHeader::UniformBuffer, BufferIndex, DescriptorSet, BindingIndex))
		{
			return;
		}

		const VkDescriptorType DescriptorType = DescriptorInfo.GetDescriptorType(DescriptorSet, BindingIndex);

		if (DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
		{
			State.SetUniformBuffer<true>(DescriptorSet, BindingIndex, UniformBuffer);
		}
		else
		{
			check(DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			State.SetUniformBuffer<false>(DescriptorSet, BindingIndex, UniformBuffer);
		}
	}

	if (HeaderUBInfo.ResourceEntries.Num())
	{
		checkSlow(ComputeShaderRHI->GetFrequency() == SF_Compute);
		check(BufferIndex < MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE);
		BoundUniformBuffers[SF_Compute][BufferIndex] = UniformBuffer;
		DirtyUniformBuffers[SF_Compute] |= (1 << BufferIndex);
	}
	else
	{
		// Internal error: Completely empty UB!
		checkSlow(!HeaderUBInfo.bOnlyHasResources);
	}
}

void FVulkanCommandListContext::RHISetStencilRef(uint32 StencilRef)
{
	PendingGfxState->SetStencilRef(StencilRef);
}

void FVulkanCommandListContext::RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	NumInstances = FMath::Max(1U, NumInstances);

	RHI_DRAW_CALL_STATS(PendingGfxState->PrimitiveType, NumInstances*NumPrimitives);

	CommitGraphicsResourceTables();

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareForDraw(CmdBuffer);
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PendingGfxState->PrimitiveType);
	VulkanRHI::vkCmdDraw(CmdBuffer->GetHandle(), NumVertices, NumInstances, BaseVertexIndex, 0);

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
}

void FVulkanCommandListContext::RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
	static_assert(sizeof(FRHIDrawIndirectParameters) == sizeof(VkDrawIndirectCommand), "Draw indirect doesn't match!");

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_INC();

	CommitGraphicsResourceTables();

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd);

	FVulkanResourceMultiBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);


	VulkanRHI::vkCmdDrawIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentBuffer->GetOffset() + ArgumentOffset, 1, sizeof(VkDrawIndirectCommand));

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitive(FRHIBuffer* IndexBufferRHI, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	NumInstances = FMath::Max(1U, NumInstances);
	RHI_DRAW_CALL_STATS(PendingGfxState->PrimitiveType, NumInstances*NumPrimitives);
	checkf(GRHISupportsFirstInstance || FirstInstance == 0, TEXT("FirstInstance must be 0, see GRHISupportsFirstInstance"));

	CommitGraphicsResourceTables();

	FVulkanResourceMultiBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd);
	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PendingGfxState->PrimitiveType);
	VulkanRHI::vkCmdDrawIndexed(CmdBuffer, NumIndices, NumInstances, StartIndex, BaseVertexIndex, FirstInstance);

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 /*NumInstances*/)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_INC();

	CommitGraphicsResourceTables();

	FVulkanResourceMultiBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd);
	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	FVulkanResourceMultiBuffer* ArgumentBuffer = ResourceCast(ArgumentsBufferRHI);
	VkDeviceSize ArgumentOffset = DrawArgumentsIndex * sizeof(VkDrawIndexedIndirectCommand);


	VulkanRHI::vkCmdDrawIndexedIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentBuffer->GetOffset() + ArgumentOffset, 1, sizeof(VkDrawIndexedIndirectCommand));

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_INC();

	CommitGraphicsResourceTables();

	FVulkanResourceMultiBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd);
	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	FVulkanResourceMultiBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);


	VulkanRHI::vkCmdDrawIndexedIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentBuffer->GetOffset() + ArgumentOffset, 1, sizeof(VkDrawIndexedIndirectCommand));

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1); 
	}
}

void FVulkanCommandListContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	if (!(bClearColor || bClearDepth || bClearStencil))
	{
		return;
	}

	check(bClearColor ? NumClearColors > 0 : true);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	//FRCLog::Printf(TEXT("RHIClearMRT"));

	const uint32 NumColorAttachments = CurrentFramebuffer->GetNumColorAttachments();
	check(!bClearColor || (uint32)NumClearColors <= NumColorAttachments);
	InternalClearMRT(CmdBuffer, bClearColor, bClearColor ? NumClearColors : 0, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil);
}

void FVulkanCommandListContext::InternalClearMRT(FVulkanCmdBuffer* CmdBuffer, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	if (CurrentRenderPass)
	{
		const VkExtent2D& Extents = CurrentRenderPass->GetLayout().GetExtent2D();
		VkClearRect Rect;
		FMemory::Memzero(Rect);
		Rect.rect.offset.x = 0;
		Rect.rect.offset.y = 0;
		Rect.rect.extent = Extents;

		VkClearAttachment Attachments[MaxSimultaneousRenderTargets + 1];
		FMemory::Memzero(Attachments);

		uint32 NumAttachments = NumClearColors;
		if (bClearColor)
		{
			for (int32 i = 0; i < NumClearColors; ++i)
			{
				Attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				Attachments[i].colorAttachment = i;
				Attachments[i].clearValue.color.float32[0] = ClearColorArray[i].R;
				Attachments[i].clearValue.color.float32[1] = ClearColorArray[i].G;
				Attachments[i].clearValue.color.float32[2] = ClearColorArray[i].B;
				Attachments[i].clearValue.color.float32[3] = ClearColorArray[i].A;
			}
		}

		if (bClearDepth || bClearStencil)
		{
			Attachments[NumClearColors].aspectMask = bClearDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			Attachments[NumClearColors].aspectMask |= bClearStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
			Attachments[NumClearColors].colorAttachment = 0;
			Attachments[NumClearColors].clearValue.depthStencil.depth = Depth;
			Attachments[NumClearColors].clearValue.depthStencil.stencil = Stencil;
			++NumAttachments;
		}

		VulkanRHI::vkCmdClearAttachments(CmdBuffer->GetHandle(), NumAttachments, Attachments, 1, &Rect);
	}
	else
	{
		ensure(0);
		//VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), )
	}
}

void FVulkanDynamicRHI::RHISuspendRendering()
{
}

void FVulkanDynamicRHI::RHIResumeRendering()
{
}

bool FVulkanDynamicRHI::RHIIsRenderingSuspended()
{
	return false;
}

void FVulkanDynamicRHI::RHIBlockUntilGPUIdle()
{
	Device->SubmitCommandsAndFlushGPU();
	Device->WaitUntilIdle();
}

uint32 FVulkanDynamicRHI::RHIGetGPUFrameCycles(uint32 GPUIndex)
{
	check(GPUIndex == 0);
	return GGPUFrameTime;
}

void FVulkanCommandListContext::RHISetDepthBounds(float MinDepth, float MaxDepth)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	VulkanRHI::vkCmdSetDepthBounds(CmdBuffer->GetHandle(), MinDepth, MaxDepth);
}

void FVulkanCommandListContext::RequestSubmitCurrentCommands()
{
	if (Device->GetComputeQueue() == Queue)
	{
		if (CommandBufferManager->HasPendingUploadCmdBuffer())
		{
			CommandBufferManager->SubmitUploadCmdBuffer();
		}
		bSubmitAtNextSafePoint = true;
		SafePointSubmit();
	}
	else
	{
		ensure(IsImmediate());
		bSubmitAtNextSafePoint = true;
	}
}

void FVulkanCommandListContext::InternalSubmitActiveCmdBuffer()
{
	CommandBufferManager->SubmitActiveCmdBuffer();
	CommandBufferManager->PrepareForNewActiveCommandBuffer();
}

void FVulkanCommandListContext::PrepareForCPURead()
{
	ensure(IsImmediate());
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer && CmdBuffer->HasBegun())
	{
		check(!CmdBuffer->IsInsideRenderPass());

		CommandBufferManager->SubmitActiveCmdBuffer();
		if (!GWaitForIdleOnSubmit)
		{
			// The wait has already happened if GWaitForIdleOnSubmit is set
			CommandBufferManager->WaitForCmdBuffer(CmdBuffer);
		}
	}
}

void FVulkanCommandListContext::RHISubmitCommandsHint()
{
	RequestSubmitCurrentCommands();
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer && CmdBuffer->HasBegun() && CmdBuffer->IsOutsideRenderPass())
	{
		SafePointSubmit();
	}
	CommandBufferManager->RefreshFenceStatus();
}

void FVulkanCommandListContext::RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* StagingBufferRHI, uint32 Offset, uint32 NumBytes)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	FVulkanResourceMultiBuffer* VertexBuffer = ResourceCast(SourceBufferRHI);

	ensure(CmdBuffer->IsOutsideRenderPass());

	FVulkanStagingBuffer* StagingBuffer = ResourceCast(StagingBufferRHI);
	if (!StagingBuffer->StagingBuffer || StagingBuffer->StagingBuffer->GetSize() < NumBytes) //-V1051
	{
		if (StagingBuffer->StagingBuffer)
		{
			Device->GetStagingManager().ReleaseBuffer(nullptr, StagingBuffer->StagingBuffer);
		}

		VulkanRHI::FStagingBuffer* ReadbackStagingBuffer = Device->GetStagingManager().AcquireBuffer(NumBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		StagingBuffer->StagingBuffer = ReadbackStagingBuffer;
		StagingBuffer->Device = Device;
	}

	StagingBuffer->QueuedNumBytes = NumBytes;

	VkBufferCopy Region;
	FMemory::Memzero(Region);
	Region.size = NumBytes;
	Region.srcOffset = Offset + VertexBuffer->GetOffset();
	//Region.dstOffset = 0;
	VulkanRHI::vkCmdCopyBuffer(CmdBuffer->GetHandle(), VertexBuffer->GetHandle(), StagingBuffer->StagingBuffer->GetHandle(), 1, &Region);
}

void FVulkanCommandListContext::RHIWriteGPUFence(FRHIGPUFence* FenceRHI)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	FVulkanGPUFence* Fence = ResourceCast(FenceRHI);

	Fence->CmdBuffer = CmdBuffer;
	Fence->FenceSignaledCounter = CmdBuffer->GetFenceSignaledCounter();
}




struct FVulkanPlatformCommandList : public IRHIPlatformCommandList
{
	FVulkanCommandListContext* CmdContext = nullptr;
};

template<>
struct TVulkanResourceTraits<IRHIPlatformCommandList>
{
	typedef FVulkanPlatformCommandList TConcreteType;
};

IRHIComputeContext* FVulkanDynamicRHI::RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask)
{
	// @todo: RHI command list refactor - fix async compute
	checkf(Pipeline == ERHIPipeline::Graphics, TEXT("Async compute command contexts not currently implemented."));

	FVulkanCommandListContext* CmdContext = Device->AcquireDeferredContext();

	FVulkanCommandBufferManager* CmdMgr = CmdContext->GetCommandBufferManager();
	FVulkanCmdBuffer* CmdBuffer = CmdMgr->GetActiveCmdBuffer();
	if (!CmdBuffer)
	{
		CmdMgr->PrepareForNewActiveCommandBuffer();
		CmdBuffer = CmdMgr->GetActiveCmdBuffer();
	}
	else if (CmdBuffer->IsSubmitted())
	{
		CmdMgr->PrepareForNewActiveCommandBuffer();
		CmdBuffer = CmdMgr->GetActiveCmdBuffer();
	}
	if (!CmdBuffer->HasBegun())
	{
		CmdBuffer->Begin();
	}

	return CmdContext;
}

IRHIPlatformCommandList* FVulkanDynamicRHI::RHIFinalizeContext(IRHIComputeContext* Context)
{
	FVulkanPlatformCommandList* PlatformCmdList = new FVulkanPlatformCommandList();
	PlatformCmdList->CmdContext = static_cast<FVulkanCommandListContext*>(Context);
	return PlatformCmdList;
}

void FVulkanDynamicRHI::RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists, bool bFlushResources)
{
	for (IRHIPlatformCommandList* Ptr : CommandLists)
	{
		FVulkanPlatformCommandList* PlatformCmdList = ResourceCast(Ptr);

		if (PlatformCmdList->CmdContext->IsImmediate())
		{
			PlatformCmdList->CmdContext->RHISubmitCommandsHint();
		}
		else
		{
			FVulkanCommandBufferManager* CmdBufMgr = PlatformCmdList->CmdContext->GetCommandBufferManager();
			check(!CmdBufMgr->HasPendingUploadCmdBuffer());  // todo-jn
			FVulkanCmdBuffer* CmdBuffer = CmdBufMgr->GetActiveCmdBuffer();
			check(!CmdBuffer->IsInsideRenderPass());
			CmdBufMgr->SubmitActiveCmdBuffer();

			Device->ReleaseDeferredContext(PlatformCmdList->CmdContext);
		}

		delete PlatformCmdList;
	}
}
