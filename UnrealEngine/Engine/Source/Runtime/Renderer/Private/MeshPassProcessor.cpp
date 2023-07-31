// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshPassProcessor.cpp: 
=============================================================================*/

#include "MeshPassProcessor.h"
#include "SceneUtils.h"
#include "SceneRendering.h"
#include "Logging/LogMacros.h"
#include "RendererModule.h"
#include "SceneCore.h"
#include "ScenePrivate.h"
#include "SceneInterface.h"
#include "MeshPassProcessor.inl"
#include "PipelineStateCache.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"
#include "Hash/CityHash.h"
#include "ComponentRecreateRenderStateContext.h"

FRWLock FGraphicsMinimalPipelineStateId::PersistentIdTableLock;
FGraphicsMinimalPipelineStateId::PersistentTableType FGraphicsMinimalPipelineStateId::PersistentIdTable;

#if MESH_DRAW_COMMAND_DEBUG_DATA
std::atomic<int32> FGraphicsMinimalPipelineStateId::LocalPipelineIdTableSize(0);
std::atomic<int32> FGraphicsMinimalPipelineStateId::CurrentLocalPipelineIdTableSize(0);
#endif //MESH_DRAW_COMMAND_DEBUG_DATA

bool FGraphicsMinimalPipelineStateId::NeedsShaderInitialisation = true;

const FMeshDrawCommandSortKey FMeshDrawCommandSortKey::Default = { {0} };

int32 GEmitMeshDrawEvent = 0;
static FAutoConsoleVariableRef CVarEmitMeshDrawEvent(
	TEXT("r.EmitMeshDrawEvents"),
	GEmitMeshDrawEvent,
	TEXT("Emits a GPU event around each drawing policy draw call.  /n")
	TEXT("Useful for seeing stats about each draw call, however it greatly distorts total time and time per draw call."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarSafeStateLookup(
	TEXT("r.SafeStateLookup"),
	1,
	TEXT("Forces new-style safe state lookup for easy runtime perf comparison\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

int32 GSkipDrawOnPSOPrecaching = 0;
static FAutoConsoleVariableRef CVarSkipDrawOnPSOPrecaching(
	TEXT("r.SkipDrawOnPSOPrecaching"),
	GSkipDrawOnPSOPrecaching,
	TEXT("Skips mesh draw call when the PSO is still compiling (default 0)."),
	ECVF_RenderThreadSafe
);

#if WITH_EDITORONLY_DATA

int32 GNaniteIsolateInvalidCoarseMesh = 0;
static FAutoConsoleVariableRef CVarNaniteIsolateInvalidCoarseMesh(
	TEXT("r.Nanite.IsolateInvalidCoarseMesh"),
	GNaniteIsolateInvalidCoarseMesh,
	TEXT("Debug mode to render only non-Nanite proxies that incorrectly reference coarse static mesh assets."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// Needed to force a recache of all the static mesh draw commands
		FGlobalComponentRecreateRenderStateContext Context;
	})
);

#endif

class FReadOnlyMeshDrawSingleShaderBindings : public FMeshDrawShaderBindingsLayout
{
public:
	FReadOnlyMeshDrawSingleShaderBindings(const FMeshDrawShaderBindingsLayout& InLayout, const uint8* InData) :
		FMeshDrawShaderBindingsLayout(InLayout)
	{
		Data = InData;
	}

	inline FRHIUniformBuffer*const* GetUniformBufferStart() const
	{
		return (FRHIUniformBuffer**)(Data + GetUniformBufferOffset());
	}

	inline FRHISamplerState** GetSamplerStart() const
	{
		const uint8* SamplerDataStart = Data + GetSamplerOffset();
		return (FRHISamplerState**)SamplerDataStart;
	}

	inline FRHIResource** GetSRVStart() const
	{
		const uint8* SRVDataStart = Data + GetSRVOffset();
		return (FRHIResource**)SRVDataStart;
	}

	inline const uint8* GetSRVTypeStart() const
	{
		const uint8* SRVTypeDataStart = Data + GetSRVTypeOffset();
		return SRVTypeDataStart;
	}

	inline const uint8* GetLooseDataStart() const
	{
		const uint8* LooseDataStart = Data + GetLooseDataOffset();
		return LooseDataStart;
	}

private:
	const uint8* Data;
};

template<typename TRHICmdList, typename TRHIShader>
FORCEINLINE void SetBindlessParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, FShaderResourceParameterInfo Parameter, FRHIDescriptorHandle Handle)
{
	if (Handle.IsValid())
	{
		const uint32 BindlessIndex = Handle.GetIndex();
		RHICmdList.SetShaderParameter(Shader, Parameter.BufferIndex, Parameter.BaseIndex, sizeof(BindlessIndex), &BindlessIndex);
	}
}

template<class TRHICmdList, class TRHIShader>
inline void SetTextureParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, FShaderBindingState& RESTRICT ShaderBindingState, FShaderResourceParameterInfo Parameter, FRHITexture* TextureRHI)
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (Parameter.Type == EShaderParameterType::BindlessResourceIndex)
	{
		const FRHIDescriptorHandle Handle = TextureRHI ? TextureRHI->GetDefaultBindlessHandle() : FRHIDescriptorHandle();
		SetBindlessParameter(RHICmdList, Shader, Parameter, Handle);
	}
	else
#endif
	{
		checkSlow(Parameter.BaseIndex < UE_ARRAY_COUNT(ShaderBindingState.Textures));
		if (TextureRHI != ShaderBindingState.Textures[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderTexture(Shader, Parameter.BaseIndex, TextureRHI);

			ShaderBindingState.Textures[Parameter.BaseIndex] = TextureRHI;
			ShaderBindingState.MaxTextureUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxTextureUsed);
		}
	}
}

template<class TRHICmdList, class TRHIShader>
inline void SetTextureParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, FShaderResourceParameterInfo Parameter, FRHITexture* TextureRHI)
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (Parameter.Type == EShaderParameterType::BindlessResourceIndex)
	{
		const FRHIDescriptorHandle Handle = TextureRHI ? TextureRHI->GetDefaultBindlessHandle() : FRHIDescriptorHandle();
		SetBindlessParameter(RHICmdList, Shader, Parameter, Handle);
	}
	else
#endif
	{
		RHICmdList.SetShaderTexture(Shader, Parameter.BaseIndex, TextureRHI);
	}
}

template<class TRHICmdList, class TRHIShader>
inline void SetSrvParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, FShaderBindingState& RESTRICT ShaderBindingState, FShaderResourceParameterInfo Parameter, FRHIShaderResourceView* SrvRHI)
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (Parameter.Type == EShaderParameterType::BindlessResourceIndex)
	{
		const FRHIDescriptorHandle Handle = SrvRHI ? SrvRHI->GetBindlessHandle() : FRHIDescriptorHandle();
		SetBindlessParameter(RHICmdList, Shader, Parameter, Handle);
	}
	else
#endif
	{
		checkSlow(Parameter.BaseIndex < UE_ARRAY_COUNT(ShaderBindingState.SRVs));
		if (SrvRHI != ShaderBindingState.SRVs[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderResourceViewParameter(Shader, Parameter.BaseIndex, SrvRHI);

			ShaderBindingState.SRVs[Parameter.BaseIndex] = SrvRHI;
			ShaderBindingState.MaxSRVUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxSRVUsed);
		}
	}
}

template<class TRHICmdList, class TRHIShader>
inline void SetSrvParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, FShaderResourceParameterInfo Parameter, FRHIShaderResourceView* SrvRHI)
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (Parameter.Type == EShaderParameterType::BindlessResourceIndex)
	{
		const FRHIDescriptorHandle Handle = SrvRHI ? SrvRHI->GetBindlessHandle() : FRHIDescriptorHandle();
		SetBindlessParameter(RHICmdList, Shader, Parameter, Handle);
	}
	else
#endif
	{
		RHICmdList.SetShaderResourceViewParameter(Shader, Parameter.BaseIndex, SrvRHI);
	}
}

template<class TRHICmdList, class TRHIShader>
inline void SetSamplerParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, FShaderBindingState& RESTRICT ShaderBindingState, FShaderResourceParameterInfo Parameter, FRHISamplerState* SamplerStateRHI)
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (Parameter.Type == EShaderParameterType::BindlessSamplerIndex)
	{
		const FRHIDescriptorHandle Handle = SamplerStateRHI ? SamplerStateRHI->GetBindlessHandle() : FRHIDescriptorHandle();
		SetBindlessParameter(RHICmdList, Shader, Parameter, Handle);
	}
	else
#endif
	{
		checkSlow(Parameter.BaseIndex < UE_ARRAY_COUNT(ShaderBindingState.Samplers));
		if (SamplerStateRHI != ShaderBindingState.Samplers[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderSampler(Shader, Parameter.BaseIndex, SamplerStateRHI);

			ShaderBindingState.Samplers[Parameter.BaseIndex] = SamplerStateRHI;
			ShaderBindingState.MaxSamplerUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxSamplerUsed);
		}
	}
}

template<class TRHICmdList, class TRHIShader>
inline void SetSamplerParameter(TRHICmdList& RHICmdList, TRHIShader* Shader, FShaderResourceParameterInfo Parameter, FRHISamplerState* SamplerStateRHI)
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (Parameter.Type == EShaderParameterType::BindlessSamplerIndex)
	{
		const FRHIDescriptorHandle Handle = SamplerStateRHI ? SamplerStateRHI->GetBindlessHandle() : FRHIDescriptorHandle();
		SetBindlessParameter(RHICmdList, Shader, Parameter, Handle);
	}
	else
#endif
	{
		RHICmdList.SetShaderSampler(Shader, Parameter.BaseIndex, SamplerStateRHI);
	}
}

template<class RHICmdListType, class RHIShaderType>
void FMeshDrawShaderBindings::SetShaderBindings(
	RHICmdListType& RHICmdList,
	RHIShaderType Shader,
	const FReadOnlyMeshDrawSingleShaderBindings& RESTRICT SingleShaderBindings,
	FShaderBindingState& RESTRICT ShaderBindingState)
{
	FRHIUniformBuffer* const* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
	const FShaderUniformBufferParameterInfo* RESTRICT UniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.GetData();
	const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
	{
		FShaderUniformBufferParameterInfo Parameter = UniformBufferParameters[UniformBufferIndex];
		checkSlow(Parameter.BaseIndex < UE_ARRAY_COUNT(ShaderBindingState.UniformBuffers));
		FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];

		if (UniformBuffer != ShaderBindingState.UniformBuffers[Parameter.BaseIndex])
		{
			RHICmdList.SetShaderUniformBuffer(Shader, Parameter.BaseIndex, UniformBuffer);
			ShaderBindingState.UniformBuffers[Parameter.BaseIndex] = UniformBuffer;
			ShaderBindingState.MaxUniformBufferUsed = FMath::Max((int32)Parameter.BaseIndex, ShaderBindingState.MaxUniformBufferUsed);
		}
	}

	FRHISamplerState* const* RESTRICT SamplerBindings = SingleShaderBindings.GetSamplerStart();
	const FShaderResourceParameterInfo* RESTRICT TextureSamplerParameters = SingleShaderBindings.ParameterMapInfo.TextureSamplers.GetData();
	const int32 NumTextureSamplers = SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num();

	for (int32 SamplerIndex = 0; SamplerIndex < NumTextureSamplers; SamplerIndex++)
	{
		FShaderResourceParameterInfo Parameter = TextureSamplerParameters[SamplerIndex];
		FRHISamplerState* Sampler = (FRHISamplerState*)SamplerBindings[SamplerIndex];

		SetSamplerParameter(RHICmdList, Shader, ShaderBindingState, Parameter, Sampler);
	}

	const uint8* RESTRICT SRVType = SingleShaderBindings.GetSRVTypeStart();
	FRHIResource* const* RESTRICT SRVBindings = SingleShaderBindings.GetSRVStart();
	const FShaderResourceParameterInfo* RESTRICT SRVParameters = SingleShaderBindings.ParameterMapInfo.SRVs.GetData();
	const uint32 NumSRVs = SingleShaderBindings.ParameterMapInfo.SRVs.Num();

	for (uint32 SRVIndex = 0; SRVIndex < NumSRVs; SRVIndex++)
	{
		FShaderResourceParameterInfo Parameter = SRVParameters[SRVIndex];

		uint32 TypeByteIndex = SRVIndex / 8;
		uint32 TypeBitIndex = SRVIndex % 8;

		if (SRVType[TypeByteIndex] & (1 << TypeBitIndex))
		{
			FRHIShaderResourceView* SRV = (FRHIShaderResourceView*)SRVBindings[SRVIndex];
			SetSrvParameter(RHICmdList, Shader, ShaderBindingState, Parameter, SRV);
		}
		else
		{
			FRHITexture* Texture = (FRHITexture*)SRVBindings[SRVIndex];
			SetTextureParameter(RHICmdList, Shader, ShaderBindingState, Parameter, Texture);
		}
	}

	const uint8* LooseDataStart = SingleShaderBindings.GetLooseDataStart();

	for (const FShaderLooseParameterBufferInfo& LooseParameterBuffer : SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers)
	{
		for (FShaderLooseParameterInfo Parameter : LooseParameterBuffer.Parameters)
		{
			RHICmdList.SetShaderParameter(
				Shader,
				LooseParameterBuffer.BaseIndex,
				Parameter.BaseIndex,
				Parameter.Size,
				LooseDataStart
			);

			LooseDataStart += Parameter.Size;
		}
	}
}

template<class RHICmdListType, class RHIShaderType>
void FMeshDrawShaderBindings::SetShaderBindings(
	RHICmdListType& RHICmdList,
	RHIShaderType Shader,
	const FReadOnlyMeshDrawSingleShaderBindings& RESTRICT SingleShaderBindings)
{
	FRHIUniformBuffer* const* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
	const FShaderUniformBufferParameterInfo* RESTRICT UniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.GetData();
	const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
	{
		FShaderUniformBufferParameterInfo Parameter = UniformBufferParameters[UniformBufferIndex];
		FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];

		if (UniformBuffer)
		{
			RHICmdList.SetShaderUniformBuffer(Shader, Parameter.BaseIndex, UniformBuffer);
		}
	}

	FRHISamplerState* const* RESTRICT SamplerBindings = SingleShaderBindings.GetSamplerStart();
	const FShaderResourceParameterInfo* RESTRICT TextureSamplerParameters = SingleShaderBindings.ParameterMapInfo.TextureSamplers.GetData();
	const int32 NumTextureSamplers = SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num();

	for (int32 SamplerIndex = 0; SamplerIndex < NumTextureSamplers; SamplerIndex++)
	{
		FShaderResourceParameterInfo Parameter = TextureSamplerParameters[SamplerIndex];
		FRHISamplerState* Sampler = (FRHISamplerState*)SamplerBindings[SamplerIndex];

		if (Sampler)
		{
			SetSamplerParameter(RHICmdList, Shader, Parameter, Sampler);
		}
	}

	const uint8* RESTRICT SRVType = SingleShaderBindings.GetSRVTypeStart();
	FRHIResource* const* RESTRICT SRVBindings = SingleShaderBindings.GetSRVStart();
	const FShaderResourceParameterInfo* RESTRICT SRVParameters = SingleShaderBindings.ParameterMapInfo.SRVs.GetData();
	const uint32 NumSRVs = SingleShaderBindings.ParameterMapInfo.SRVs.Num();

	for (uint32 SRVIndex = 0; SRVIndex < NumSRVs; SRVIndex++)
	{
		FShaderResourceParameterInfo Parameter = SRVParameters[SRVIndex];

		uint32 TypeByteIndex = SRVIndex / 8;
		uint32 TypeBitIndex = SRVIndex % 8;

		if (SRVType[TypeByteIndex] & (1 << TypeBitIndex))
		{
			FRHIShaderResourceView* SRV = (FRHIShaderResourceView*)SRVBindings[SRVIndex];
			SetSrvParameter(RHICmdList, Shader, Parameter, SRV);
		}
		else
		{
			FRHITexture* Texture = (FRHITexture*)SRVBindings[SRVIndex];
			SetTextureParameter(RHICmdList, Shader, Parameter, Texture);
		}
	}
	
	const uint8* LooseDataStart = SingleShaderBindings.GetLooseDataStart();

	for (const FShaderLooseParameterBufferInfo& LooseParameterBuffer : SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers)
	{
		for (FShaderLooseParameterInfo Parameter : LooseParameterBuffer.Parameters)
		{
			RHICmdList.SetShaderParameter(
				Shader,
				LooseParameterBuffer.BaseIndex,
				Parameter.BaseIndex,
				Parameter.Size,
				LooseDataStart
			);

			LooseDataStart += Parameter.Size;
		}
	}
}

#if RHI_RAYTRACING

FRayTracingLocalShaderBindings* FMeshDrawShaderBindings::SetRayTracingShaderBindingsForHitGroup(
	FRayTracingLocalShaderBindingWriter* BindingWriter,
	uint32 InstanceIndex, 
	uint32 SegmentIndex,
	uint32 HitGroupIndexInPipeline,
	uint32 ShaderSlot) const
{
	check(ShaderLayouts.Num() == 1);

	FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[0], GetData());

	FRHIUniformBuffer* const* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
	const FShaderUniformBufferParameterInfo* RESTRICT UniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.GetData();
	const int32 NumUniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

	checkf(SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num() == 0, TEXT("Texture sampler parameters are not supported for ray tracing. UniformBuffers must be used for all resource binding."));
	checkf(SingleShaderBindings.ParameterMapInfo.SRVs.Num() == 0, TEXT("SRV parameters are not supported for ray tracing. UniformBuffers must be used for all resource binding."));

	// Measure parameter memory requirements

	int32 MaxUniformBufferIndex = -1;
	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBufferParameters; UniformBufferIndex++)
	{
		FShaderUniformBufferParameterInfo Parameter = UniformBufferParameters[UniformBufferIndex];
		const FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];
		MaxUniformBufferIndex = FMath::Max((int32)Parameter.BaseIndex, MaxUniformBufferIndex);
	}

	const uint32 NumUniformBuffersToSet = MaxUniformBufferIndex + 1;

	const TMemoryImageArray<FShaderLooseParameterBufferInfo>& LooseParameterBuffers = SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers;
	uint32 LooseParameterDataSize = 0;

	if (LooseParameterBuffers.Num())
	{
		check(LooseParameterBuffers.Num() <= 1);

		const FShaderLooseParameterBufferInfo& LooseParameterBuffer = SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers[0];
		check(LooseParameterBuffer.BaseIndex == 0);

		for (int32 LooseParameterIndex = 0; LooseParameterIndex < LooseParameterBuffer.Parameters.Num(); LooseParameterIndex++)
		{
			FShaderLooseParameterInfo LooseParameter = LooseParameterBuffer.Parameters[LooseParameterIndex];
			LooseParameterDataSize = FMath::Max<uint32>(LooseParameterDataSize, LooseParameter.BaseIndex + LooseParameter.Size);
		}
	}

	checkf(MaxUniformBufferIndex + 1 == NumUniformBufferParameters + LooseParameterBuffers.Num(),
		TEXT("Highest index of a uniform buffer was %d, but there were %d uniform buffer parameters and %d loose parameters"),
		MaxUniformBufferIndex,
		NumUniformBufferParameters,
		LooseParameterBuffers.Num());

	// Allocate and fill bindings

	const uint32 UserData = 0; // UserData could be used to store material ID or any other kind of per-material constant. This can be retrieved in hit shaders via GetHitGroupUserData().

	FRayTracingLocalShaderBindings& Bindings = BindingWriter->AddWithInlineParameters(NumUniformBuffersToSet, LooseParameterDataSize);

	Bindings.InstanceIndex = InstanceIndex;
	Bindings.SegmentIndex = SegmentIndex;
	Bindings.ShaderSlot = ShaderSlot;
	Bindings.ShaderIndexInPipeline = HitGroupIndexInPipeline;
	Bindings.UserData = UserData;

	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBufferParameters; UniformBufferIndex++)
	{
		FShaderUniformBufferParameterInfo Parameter = UniformBufferParameters[UniformBufferIndex];
		const FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];
		Bindings.UniformBuffers[Parameter.BaseIndex] = const_cast<FRHIUniformBuffer*>(UniformBuffer);
	}

	if (LooseParameterBuffers.Num())
	{
		const FShaderLooseParameterBufferInfo& LooseParameterBuffer = SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers[0];
		const uint8* LooseDataOffset = SingleShaderBindings.GetLooseDataStart();
		for (int32 LooseParameterIndex = 0; LooseParameterIndex < LooseParameterBuffer.Parameters.Num(); LooseParameterIndex++)
		{
			FShaderLooseParameterInfo LooseParameter = LooseParameterBuffer.Parameters[LooseParameterIndex];
			FMemory::Memcpy(Bindings.LooseParameterData + LooseParameter.BaseIndex, LooseDataOffset, LooseParameter.Size);
			LooseDataOffset += LooseParameter.Size;
		}
	}

	return &Bindings;
}

FRayTracingLocalShaderBindings* FMeshDrawShaderBindings::SetRayTracingShaderBindings(FRayTracingLocalShaderBindingWriter* BindingWriter, uint32 ShaderIndexInPipeline, uint32 ShaderSlot) const
{
	check(ShaderLayouts.Num() == 1);
	return SetRayTracingShaderBindingsForHitGroup(BindingWriter, 0, 0, ShaderIndexInPipeline, ShaderSlot);
}

void FMeshDrawShaderBindings::SetRayTracingShaderBindingsForMissShader(
	FRHICommandList& RHICmdList,
	FRHIRayTracingScene* Scene,
	FRayTracingPipelineState* PipelineState,
	uint32 ShaderIndexInPipeline,
	uint32 ShaderSlot) const
{
	check(ShaderLayouts.Num() == 1);

	FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[0], GetData());

	FShaderBindingState BindingState;

	const int32 MaxUniformBuffers = UE_ARRAY_COUNT(BindingState.UniformBuffers);

	FRHIUniformBuffer* const* RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
	const FShaderUniformBufferParameterInfo* RESTRICT UniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.GetData();
	const int32 NumUniformBufferParameters = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

	checkf(SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num() == 0, TEXT("Texture sampler parameters are not supported for ray tracing. UniformBuffers must be used for all resource binding."));
	checkf(SingleShaderBindings.ParameterMapInfo.SRVs.Num() == 0, TEXT("SRV parameters are not supported for ray tracing. UniformBuffers must be used for all resource binding."));

	// Measure parameter memory requirements

	int32 MaxUniformBufferUsed = -1;
	for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBufferParameters; UniformBufferIndex++)
	{
		FShaderUniformBufferParameterInfo Parameter = UniformBufferParameters[UniformBufferIndex];
		checkSlow(Parameter.BaseIndex < MaxUniformBuffers);
		FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];
		if (Parameter.BaseIndex < MaxUniformBuffers)
		{
			BindingState.UniformBuffers[Parameter.BaseIndex] = UniformBuffer;
			MaxUniformBufferUsed = FMath::Max((int32)Parameter.BaseIndex, MaxUniformBufferUsed);
		}
	}

	checkf(SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num() == 0, TEXT("Texture sampler parameters are not supported for ray tracing. UniformBuffers must be used for all resource binding."));
	checkf(SingleShaderBindings.ParameterMapInfo.SRVs.Num() == 0, TEXT("SRV parameters are not supported for ray tracing. UniformBuffers must be used for all resource binding."));
	checkf(SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers.Num() == 0, TEXT("Ray tracing miss shaders may not have loose parameters"));

	uint32 NumUniformBuffersToSet = MaxUniformBufferUsed + 1;
	const uint32 UserData = 0; // UserData could be used to store material ID or any other kind of per-material constant. This can be retrieved in hit shaders via GetHitGroupUserData().
	RHICmdList.SetRayTracingMissShader(Scene, ShaderSlot, PipelineState, ShaderIndexInPipeline,
		NumUniformBuffersToSet, BindingState.UniformBuffers,
		UserData);
}
#endif // RHI_RAYTRACING

FGraphicsMinimalPipelineStateId FGraphicsMinimalPipelineStateId::GetPersistentId(const FGraphicsMinimalPipelineStateInitializer& InPipelineState)
{
	Experimental::FHashElementId TableId;
	auto hash = PersistentIdTable.ComputeHash(InPipelineState);
	{
		FRWScopeLock Lock(PersistentIdTableLock, SLT_ReadOnly);

#if UE_BUILD_DEBUG
		FGraphicsMinimalPipelineStateInitializer PipelineStateDebug = FGraphicsMinimalPipelineStateInitializer(InPipelineState);
		check(GetTypeHash(PipelineStateDebug) == GetTypeHash(InPipelineState));
		check(PipelineStateDebug == InPipelineState);
#endif

		TableId = PersistentIdTable.FindIdByHash(hash, InPipelineState);


		if (!TableId.IsValid())
		{
			Lock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();

			TableId = PersistentIdTable.FindOrAddIdByHash(hash, InPipelineState, FRefCountedGraphicsMinimalPipelineState());
		}
		
		FRefCountedGraphicsMinimalPipelineState& Value = PersistentIdTable.GetByElementId(TableId).Value;

		if (Value.RefNum == 0 && !NeedsShaderInitialisation)
		{
			NeedsShaderInitialisation = true;
		}
		Value.RefNum++;
	}

	checkf(TableId.GetIndex() < (MAX_uint32 >> 2), TEXT("Persistent FGraphicsMinimalPipelineStateId table overflow!"));

	FGraphicsMinimalPipelineStateId Ret;
	Ret.bValid = 1;
	Ret.bComesFromLocalPipelineStateSet = 0;
	Ret.SetElementIndex = TableId.GetIndex();
	return Ret;
}


void FGraphicsMinimalPipelineStateId::InitializePersistentIds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(InitializePersistentMdcIds);

	FRWScopeLock WriteLock(PersistentIdTableLock, SLT_Write);
	if (NeedsShaderInitialisation)
	{
		for (TPair<const FGraphicsMinimalPipelineStateInitializer, FRefCountedGraphicsMinimalPipelineState>& Element : PersistentIdTable)
		{
			Element.Key.BoundShaderState.LazilyInitShaders();
		}
		NeedsShaderInitialisation = false;
	}
}

void FGraphicsMinimalPipelineStateId::RemovePersistentId(FGraphicsMinimalPipelineStateId Id)
{
	check(!Id.bComesFromLocalPipelineStateSet && Id.bValid);

	{
		FRWScopeLock WriteLock(PersistentIdTableLock, SLT_Write);
		FRefCountedGraphicsMinimalPipelineState& RefCountedStateInitializer = PersistentIdTable.GetByElementId(Id.SetElementIndex).Value;

		check(RefCountedStateInitializer.RefNum > 0);
		--RefCountedStateInitializer.RefNum;
		if (RefCountedStateInitializer.RefNum == 0)
		{
			PersistentIdTable.RemoveByElementId(Id.SetElementIndex);
		}
	}
}

FGraphicsMinimalPipelineStateId FGraphicsMinimalPipelineStateId::GetPipelineStateId(const FGraphicsMinimalPipelineStateInitializer& InPipelineState, FGraphicsMinimalPipelineStateSet& InOutPassSet, bool& InNeedsShaderInitialisation)
{
	FGraphicsMinimalPipelineStateId Ret;
	Ret.bValid = 1;
	Ret.bComesFromLocalPipelineStateSet = 1;
#if UE_BUILD_DEBUG
	FGraphicsMinimalPipelineStateInitializer PipelineStateDebug = FGraphicsMinimalPipelineStateInitializer(InPipelineState);
	check(GetTypeHash(PipelineStateDebug) == GetTypeHash(InPipelineState));
	check(PipelineStateDebug == InPipelineState);
#endif
	Experimental::FHashElementId TableIndex = InOutPassSet.FindOrAddId(InPipelineState);
#if UE_BUILD_DEBUG
	check(InOutPassSet.GetByElementId(TableIndex) == InPipelineState);
#endif
	InNeedsShaderInitialisation = InNeedsShaderInitialisation || InPipelineState.BoundShaderState.NeedsShaderInitialisation();

	checkf(TableIndex.GetIndex() < (MAX_uint32 >> 2), TEXT("One frame FGraphicsMinimalPipelineStateId table overflow!"));

	Ret.SetElementIndex = TableIndex.GetIndex();
	return Ret;
}

void FGraphicsMinimalPipelineStateId::ResetLocalPipelineIdTableSize()
{
#if MESH_DRAW_COMMAND_DEBUG_DATA
	int32 CapturedPipelineIdTableSize;
	do
	{
		CapturedPipelineIdTableSize = CurrentLocalPipelineIdTableSize;
	}while (!CurrentLocalPipelineIdTableSize.compare_exchange_strong(CapturedPipelineIdTableSize, 0));

	LocalPipelineIdTableSize = CapturedPipelineIdTableSize;
#endif //MESH_DRAW_COMMAND_DEBUG_DATA
}

void FGraphicsMinimalPipelineStateId::AddSizeToLocalPipelineIdTableSize(SIZE_T Size)
{
#if MESH_DRAW_COMMAND_DEBUG_DATA
	CurrentLocalPipelineIdTableSize += int32(Size);
#endif
}

FMeshDrawShaderBindings::~FMeshDrawShaderBindings()
{
	Release();
}

void FMeshDrawShaderBindings::Initialize(FMeshProcessorShaders Shaders)
{
	const int32 NumShaderFrequencies = 
		(Shaders.VertexShader.IsValid() ? 1 : 0) +
		(Shaders.PixelShader.IsValid() ? 1 : 0) +
		(Shaders.GeometryShader.IsValid() ? 1 : 0) +
		(Shaders.ComputeShader.IsValid() ? 1 : 0)
#if RHI_RAYTRACING
		+ (Shaders.RayTracingShader.IsValid() ? 1 : 0)
#endif
		;

	ShaderLayouts.Empty(NumShaderFrequencies);
	int32 ShaderBindingDataSize = 0;

	if (Shaders.VertexShader.IsValid())
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.VertexShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
		check(ShaderFrequencyBits < (1 << SF_Vertex));
		ShaderFrequencyBits |= (1 << SF_Vertex);
	}

	if (Shaders.PixelShader.IsValid())
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.PixelShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
		check(ShaderFrequencyBits < (1 << SF_Pixel));
		ShaderFrequencyBits |= (1 << SF_Pixel);
	}

	if (Shaders.GeometryShader.IsValid())
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.GeometryShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
		check(ShaderFrequencyBits < (1 << SF_Geometry));
		ShaderFrequencyBits |= (1 << SF_Geometry);
	}

	if (Shaders.ComputeShader.IsValid())
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.ComputeShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();
		check(ShaderFrequencyBits < (1 << SF_Compute));
		ShaderFrequencyBits |= (1 << SF_Compute);
	}

#if RHI_RAYTRACING
	if (Shaders.RayTracingShader.IsValid())
	{
		ShaderLayouts.Add(FMeshDrawShaderBindingsLayout(Shaders.RayTracingShader));
		ShaderBindingDataSize += ShaderLayouts.Last().GetDataSizeBytes();

		const EShaderFrequency Frequency = Shaders.RayTracingShader->GetFrequency();
		check(ShaderFrequencyBits < (1 << Frequency));
		ShaderFrequencyBits |= (1 << Frequency);
	}
#endif

	checkSlow(ShaderLayouts.Num() == NumShaderFrequencies);

	if (ShaderBindingDataSize > 0)
	{
		AllocateZeroed(ShaderBindingDataSize);
	}
}

void FMeshDrawShaderBindings::Finalize(const FMeshProcessorShaders* ShadersForDebugging)
{
#if VALIDATE_MESH_COMMAND_BINDINGS
	if (!ShadersForDebugging)
	{
		return;
	}

	const uint8* ShaderBindingDataPtr = GetData();
	uint32 ShaderFrequencyBitIndex = ~0;
	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		EShaderFrequency Frequency = SF_NumFrequencies;
		while (true)
		{
			ShaderFrequencyBitIndex++;
			if ((ShaderFrequencyBits & (1 << ShaderFrequencyBitIndex)) != 0)
			{
				Frequency = EShaderFrequency(ShaderFrequencyBitIndex);
				break;
			}
		}
		check(Frequency < SF_NumFrequencies);

		const FMeshDrawShaderBindingsLayout& ShaderLayout = ShaderLayouts[ShaderBindingsIndex];

		TShaderRef<FShader> Shader = ShadersForDebugging->GetShader(Frequency);
		check(Shader.IsValid());
		const FVertexFactoryType* VFType = Shader.GetVertexFactoryType();

		FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayout, ShaderBindingDataPtr);

		FRHIUniformBuffer* const* UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();

		for (int32 BindingIndex = 0; BindingIndex < ShaderLayout.ParameterMapInfo.UniformBuffers.Num(); BindingIndex++)
		{
			FShaderUniformBufferParameterInfo ParameterInfo = ShaderLayout.ParameterMapInfo.UniformBuffers[BindingIndex];

			FRHIUniformBuffer* UniformBufferValue = UniformBufferBindings[BindingIndex];

			if (!UniformBufferValue)
			{
				// Search the automatically bound uniform buffers for more context if available
				const FShaderParametersMetadata* AutomaticallyBoundUniformBufferStruct = Shader->FindAutomaticallyBoundUniformBufferStruct(ParameterInfo.BaseIndex);

				if (AutomaticallyBoundUniformBufferStruct)
				{
					ensureMsgf(
						UniformBufferValue || EnumHasAnyFlags(AutomaticallyBoundUniformBufferStruct->GetBindingFlags(), EUniformBufferBindingFlags::Static),
						TEXT("Shader %s with vertex factory %s never set automatically bound uniform buffer at BaseIndex %i.  Expected buffer of type %s.  This can cause GPU hangs, depending on how the shader uses it."),
						Shader.GetType()->GetName(), 
						VFType ? VFType->GetName() : TEXT("nullptr"),
						ParameterInfo.BaseIndex,
						AutomaticallyBoundUniformBufferStruct->GetStructTypeName());
				}
				else
				{
					ensureMsgf(UniformBufferValue, TEXT("Shader %s with vertex factory %s never set uniform buffer at BaseIndex %i.  This can cause GPU hangs, depending on how the shader uses it."), 
						VFType ? VFType->GetName() : TEXT("nullptr"),
						Shader.GetType()->GetName(), 
						ParameterInfo.BaseIndex);
				}
			}
		}

		FRHISamplerState* const* SamplerBindings = SingleShaderBindings.GetSamplerStart();

		for (int32 BindingIndex = 0; BindingIndex < ShaderLayout.ParameterMapInfo.TextureSamplers.Num(); BindingIndex++)
		{
			FShaderResourceParameterInfo ParameterInfo = ShaderLayout.ParameterMapInfo.TextureSamplers[BindingIndex];
			const FRHISamplerState* SamplerValue = SamplerBindings[BindingIndex];
			ensureMsgf(SamplerValue, TEXT("Shader %s with vertex factory %s never set sampler at BaseIndex %u.  This can cause GPU hangs, depending on how the shader uses it."), 
				Shader.GetType()->GetName(), 
				VFType ? VFType->GetName() : TEXT("nullptr"),
				ParameterInfo.BaseIndex);
		}

		const uint8* RESTRICT SRVType = SingleShaderBindings.GetSRVTypeStart();
		FRHIResource* const* RESTRICT SRVBindings = SingleShaderBindings.GetSRVStart();
		const FShaderResourceParameterInfo* RESTRICT SRVParameters = SingleShaderBindings.ParameterMapInfo.SRVs.GetData();
		const uint32 NumSRVs = SingleShaderBindings.ParameterMapInfo.SRVs.Num();

		for (uint32 SRVIndex = 0; SRVIndex < NumSRVs; SRVIndex++)
		{
			FShaderResourceParameterInfo Parameter = SRVParameters[SRVIndex];

			uint32 TypeByteIndex = SRVIndex / 8;
			uint32 TypeBitIndex = SRVIndex % 8;

			if (SRVType[TypeByteIndex] & (1 << TypeBitIndex))
			{
				FRHIShaderResourceView* SRV = (FRHIShaderResourceView*)SRVBindings[SRVIndex];

				ensureMsgf(SRV, TEXT("Shader %s with vertex factory %s never set SRV at BaseIndex %u.  This can cause GPU hangs, depending on how the shader uses it."), 
					Shader.GetType()->GetName(), 
					VFType ? VFType->GetName() : TEXT("nullptr"),
					Parameter.BaseIndex);
			}
			else
			{
				FRHITexture* Texture = (FRHITexture*)SRVBindings[SRVIndex];

				ensureMsgf(Texture, TEXT("Shader %s with vertex factory %s never set texture at BaseIndex %u.  This can cause GPU hangs, depending on how the shader uses it."), 
					Shader.GetType()->GetName(), 
					VFType ? VFType->GetName() : TEXT("nullptr"),
					Parameter.BaseIndex);
			}
		}

		ShaderBindingDataPtr += ShaderLayout.GetDataSizeBytes();
	}
#endif
}

void FMeshDrawShaderBindings::CopyFrom(const FMeshDrawShaderBindings& Other)
{
	Release();
	ShaderLayouts = Other.ShaderLayouts;
	ShaderFrequencyBits = Other.ShaderFrequencyBits;

	Allocate(Other.Size);

	if (Other.UsesInlineStorage())
	{
		Data = Other.Data;
	}
	else
	{
		FPlatformMemory::Memcpy(GetData(), Other.GetData(), Size);
	}

#if VALIDATE_UNIFORM_BUFFER_LIFETIME
	uint8* ShaderBindingDataPtr = GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		FMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);
		FRHIUniformBuffer** RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
		const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

		for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
		{
			FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];

			if (UniformBuffer)
			{
				UniformBuffer->NumMeshCommandReferencesForDebugging++;
			}
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}
#endif
}

void FMeshDrawShaderBindings::Release()
{
#if VALIDATE_UNIFORM_BUFFER_LIFETIME
	uint8* ShaderBindingDataPtr = GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		FMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);
		FRHIUniformBuffer** RESTRICT UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
		const int32 NumUniformBuffers = SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num();

		for (int32 UniformBufferIndex = 0; UniformBufferIndex < NumUniformBuffers; UniformBufferIndex++)
		{
			FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];

			if (UniformBuffer)
			{
				UniformBuffer->NumMeshCommandReferencesForDebugging--;
				check(UniformBuffer->NumMeshCommandReferencesForDebugging >= 0);
			}
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}
#endif

	if (Size > sizeof(FData))
	{
		delete[] Data.GetHeapData();
	}
	Size = 0;
	Data.SetHeapData(nullptr);
}

void FGraphicsMinimalPipelineStateInitializer::SetupBoundShaderState(FRHIVertexDeclaration* VertexDeclaration, const FMeshProcessorShaders& Shaders)
{
	BoundShaderState = FMinimalBoundShaderStateInput();
	BoundShaderState.VertexDeclarationRHI = VertexDeclaration;

	checkf(Shaders.VertexShader.IsValid(), TEXT("Can't render without a vertex shader"));

	if(Shaders.VertexShader.IsValid())
	{
		checkSlow(Shaders.VertexShader->GetFrequency() == SF_Vertex);
		BoundShaderState.VertexShaderResource = Shaders.VertexShader.GetResource();
		BoundShaderState.VertexShaderIndex = Shaders.VertexShader->GetResourceIndex();
		check(BoundShaderState.VertexShaderResource->IsValidShaderIndex(BoundShaderState.VertexShaderIndex));
	}
	if (Shaders.PixelShader.IsValid())
	{
		checkSlow(Shaders.PixelShader->GetFrequency() == SF_Pixel);
		BoundShaderState.PixelShaderResource = Shaders.PixelShader.GetResource();
		BoundShaderState.PixelShaderIndex = Shaders.PixelShader->GetResourceIndex();
		check(BoundShaderState.PixelShaderResource->IsValidShaderIndex(BoundShaderState.PixelShaderIndex));
	}
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	if (Shaders.GeometryShader.IsValid())
	{
		checkSlow(Shaders.GeometryShader->GetFrequency() == SF_Geometry);
		BoundShaderState.GeometryShaderResource = Shaders.GeometryShader.GetResource();
		BoundShaderState.GeometryShaderIndex = Shaders.GeometryShader->GetResourceIndex();
		check(BoundShaderState.GeometryShaderResource->IsValidShaderIndex(BoundShaderState.GeometryShaderIndex));
	}
#endif // PLATFORM_SUPPORTS_GEOMETRY_SHADERS
}

void FGraphicsMinimalPipelineStateInitializer::ComputePrecachePSOHash()
{
	struct FHashKey
	{
		uint32 VertexDeclaration;
		uint32 VertexShader;
		uint32 PixelShader;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		uint32 GeometryShader;
#endif // PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		uint32 BlendState;
		uint32 RasterizerState;
		uint32 DepthStencilState;
		uint32 ImmutableSamplerState;

		uint32 MultiViewCount : 8;
		uint32 DrawShadingRate : 8;
		uint32 PrimitiveType : 8;
		uint32 bDepthBounds : 1;
		uint32 bHasFragmentDensityAttachment : 1;
		uint32 Unused : 6;
	} HashKey;

	FMemory::Memzero(&HashKey, sizeof(FHashKey));

	HashKey.VertexDeclaration = BoundShaderState.VertexDeclarationRHI ? BoundShaderState.VertexDeclarationRHI->GetPrecachePSOHash() : 0;
	HashKey.VertexShader = HashCombine(GetTypeHash(BoundShaderState.VertexShaderIndex), GetTypeHash(BoundShaderState.VertexShaderResource));
	HashKey.PixelShader = HashCombine(GetTypeHash(BoundShaderState.PixelShaderIndex), GetTypeHash(BoundShaderState.PixelShaderResource));
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	HashKey.GeometryShader = HashCombine(GetTypeHash(BoundShaderState.GeometryShaderIndex), GetTypeHash(BoundShaderState.GeometryShaderResource));
#endif

	FBlendStateInitializerRHI BlendStateInitializerRHI;
	if (BlendState && BlendState->GetInitializer(BlendStateInitializerRHI))
	{
		HashKey.BlendState = GetTypeHash(BlendStateInitializerRHI);
	}
	FRasterizerStateInitializerRHI RasterizerStateInitializerRHI;
	if (RasterizerState && RasterizerState->GetInitializer(RasterizerStateInitializerRHI))
	{
		HashKey.RasterizerState = GetTypeHash(RasterizerStateInitializerRHI);
	}
	FDepthStencilStateInitializerRHI DepthStencilStateInitializerRHI;
	if (DepthStencilState && DepthStencilState->GetInitializer(DepthStencilStateInitializerRHI))
	{
		HashKey.DepthStencilState = GetTypeHash(DepthStencilStateInitializerRHI);
	}

	// Ignore immutable samplers for now
	//HashKey.ImmutableSamplerState = GetTypeHash(ImmutableSamplerState);
	
	HashKey.MultiViewCount = MultiViewCount;
	HashKey.DrawShadingRate = DrawShadingRate;
	HashKey.PrimitiveType = PrimitiveType;
	HashKey.bDepthBounds = bDepthBounds;
	HashKey.bHasFragmentDensityAttachment = bHasFragmentDensityAttachment;

	PrecachePSOHash = CityHash64((const char*)&HashKey, sizeof(FHashKey));
}

#if RHI_RAYTRACING

void FRayTracingMeshCommand::SetRayTracingShaderBindingsForHitGroup(
	FRayTracingLocalShaderBindingWriter* BindingWriter,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	FRHIUniformBuffer* NaniteUniformBuffer,
	uint32 InstanceIndex,
	uint32 SegmentIndex,
	uint32 HitGroupIndexInPipeline,
	uint32 ShaderSlot) const
{

	FRayTracingLocalShaderBindings* Bindings = ShaderBindings.SetRayTracingShaderBindingsForHitGroup(BindingWriter, InstanceIndex, SegmentIndex, HitGroupIndexInPipeline, ShaderSlot);

	if (ViewUniformBufferParameter.IsBound())
	{
		check(ViewUniformBuffer);
		Bindings->UniformBuffers[ViewUniformBufferParameter.GetBaseIndex()] = ViewUniformBuffer;
	}

	if (NaniteUniformBufferParameter.IsBound())
	{
		check(NaniteUniformBuffer);
		Bindings->UniformBuffers[NaniteUniformBufferParameter.GetBaseIndex()] = NaniteUniformBuffer;
	}
}

void FRayTracingMeshCommand::SetShaders(const FMeshProcessorShaders& Shaders)
{
	check(Shaders.RayTracingShader.IsValid())
	MaterialShaderIndex = Shaders.RayTracingShader.GetRayTracingHitGroupLibraryIndex();
	MaterialShader = Shaders.RayTracingShader.GetRayTracingShader();
	ViewUniformBufferParameter = Shaders.RayTracingShader->GetUniformBufferParameter<FViewUniformShaderParameters>();
	NaniteUniformBufferParameter = Shaders.RayTracingShader->GetUniformBufferParameter<FNaniteRayTracingUniformParameters>();
	ShaderBindings.Initialize(Shaders);
}

void FRayTracingShaderCommand::SetRayTracingShaderBindings(
	FRayTracingLocalShaderBindingWriter* BindingWriter,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	FRHIUniformBuffer* NaniteUniformBuffer,
	uint32 ShaderIndexInPipeline,
	uint32 ShaderSlot) const
{
	FRayTracingLocalShaderBindings* Bindings = ShaderBindings.SetRayTracingShaderBindings(BindingWriter, ShaderIndexInPipeline, ShaderSlot);

	if (ViewUniformBufferParameter.IsBound())
	{
		check(ViewUniformBuffer);
		Bindings->UniformBuffers[ViewUniformBufferParameter.GetBaseIndex()] = ViewUniformBuffer;
	}

	if (NaniteUniformBufferParameter.IsBound())
	{
		check(NaniteUniformBuffer);
		Bindings->UniformBuffers[NaniteUniformBufferParameter.GetBaseIndex()] = NaniteUniformBuffer;
	}
}

void FRayTracingShaderCommand::SetShader(const TShaderRef<FShader>& InShader)
{
	check(InShader->GetFrequency() == SF_RayCallable || InShader->GetFrequency() == SF_RayMiss);
	ShaderIndex = InShader.GetRayTracingCallableShaderLibraryIndex();
	Shader = InShader.GetRayTracingShader();
	ViewUniformBufferParameter = InShader->GetUniformBufferParameter<FViewUniformShaderParameters>();
	NaniteUniformBufferParameter = InShader->GetUniformBufferParameter<FNaniteRayTracingUniformParameters>();

	FMeshProcessorShaders Shaders;
	Shaders.RayTracingShader = InShader;

	ShaderBindings.Initialize(Shaders);
}
#endif // RHI_RAYTRACING

void FMeshDrawCommand::SetDrawParametersAndFinalize(
	const FMeshBatch& MeshBatch, 
	int32 BatchElementIndex,
	FGraphicsMinimalPipelineStateId PipelineId,
	const FMeshProcessorShaders* ShadersForDebugging)
{
	const FMeshBatchElement& BatchElement = MeshBatch.Elements[BatchElementIndex];

	check(!BatchElement.IndexBuffer || (BatchElement.IndexBuffer && BatchElement.IndexBuffer->IsInitialized() && BatchElement.IndexBuffer->IndexBufferRHI));
	IndexBuffer = BatchElement.IndexBuffer ? BatchElement.IndexBuffer->IndexBufferRHI.GetReference() : nullptr;
	FirstIndex = BatchElement.FirstIndex;
	NumPrimitives = BatchElement.NumPrimitives;
	NumInstances = BatchElement.NumInstances;

	// If the mesh batch has a valid dynamic index buffer, use it instead
	if (BatchElement.DynamicIndexBuffer.IsValid())
	{
		check(!BatchElement.DynamicIndexBuffer.IndexBuffer || (BatchElement.DynamicIndexBuffer.IndexBuffer && BatchElement.DynamicIndexBuffer.IndexBuffer->IsInitialized() && BatchElement.DynamicIndexBuffer.IndexBuffer->IndexBufferRHI));
		IndexBuffer = BatchElement.DynamicIndexBuffer.IndexBuffer ? BatchElement.DynamicIndexBuffer.IndexBuffer->IndexBufferRHI.GetReference() : nullptr;
		FirstIndex = BatchElement.DynamicIndexBuffer.FirstIndex;
		PrimitiveType = EPrimitiveType(BatchElement.DynamicIndexBuffer.PrimitiveType);
	}

	if (NumPrimitives > 0)
	{
		VertexParams.BaseVertexIndex = BatchElement.BaseVertexIndex;
		VertexParams.NumVertices = BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1;
		checkf(!BatchElement.IndirectArgsBuffer, TEXT("FMeshBatchElement::NumPrimitives must be set to 0 when a IndirectArgsBuffer is used"));
	}
	else
	{
		checkf(BatchElement.IndirectArgsBuffer, TEXT("It is only valid to set BatchElement.NumPrimitives == 0 when a IndirectArgsBuffer is used"));
		IndirectArgs.Buffer = BatchElement.IndirectArgsBuffer;
		IndirectArgs.Offset = BatchElement.IndirectArgsOffset;
	}

	Finalize(PipelineId, ShadersForDebugging);
}

void FMeshDrawShaderBindings::SetOnCommandList(FRHICommandList& RHICmdList, const FBoundShaderStateInput& Shaders, FShaderBindingState* StateCacheShaderBindings) const
{
	const uint8* ShaderBindingDataPtr = GetData();
	uint32 ShaderFrequencyBitIndex = ~0;
	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		EShaderFrequency Frequency = SF_NumFrequencies;
		while (true)
		{
			ShaderFrequencyBitIndex++;
			if ((ShaderFrequencyBits & (1 << ShaderFrequencyBitIndex)) != 0)
			{
				Frequency = EShaderFrequency(ShaderFrequencyBitIndex);
				break;
			}
		}
		check(Frequency < SF_NumFrequencies);

		FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);
		FShaderBindingState& ShaderBindingState = StateCacheShaderBindings[Frequency];

		if (Frequency == SF_Vertex)
		{
			SetShaderBindings(RHICmdList, Shaders.VertexShaderRHI, SingleShaderBindings, ShaderBindingState);
		} 
		else if (Frequency == SF_Pixel)
		{
			SetShaderBindings(RHICmdList, Shaders.PixelShaderRHI, SingleShaderBindings, ShaderBindingState);
		}
		else if (Frequency == SF_Geometry)
		{
			SetShaderBindings(RHICmdList, Shaders.GetGeometryShader(), SingleShaderBindings, ShaderBindingState);
		}
		else
		{
			checkf(0, TEXT("Unknown shader frequency"));
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}
}

void FMeshDrawShaderBindings::SetOnCommandList(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* Shader, FShaderBindingState* StateCacheShaderBindings) const
{
	check(ShaderLayouts.Num() == 1);
	FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[0], GetData());
	check(ShaderFrequencyBits & (1 << SF_Compute));

	if (StateCacheShaderBindings != nullptr)
	{
		SetShaderBindings(RHICmdList, Shader, SingleShaderBindings, *StateCacheShaderBindings);
	}
	else
	{
		SetShaderBindings(RHICmdList, Shader, SingleShaderBindings);
	}
}

bool FMeshDrawShaderBindings::MatchesForDynamicInstancing(const FMeshDrawShaderBindings& Rhs) const
{
	if (ShaderFrequencyBits != Rhs.ShaderFrequencyBits)
{
		return false;
	}

	if (ShaderLayouts.Num() != Rhs.ShaderLayouts.Num())
	{
		return false;
}

	for (int Index = 0; Index < ShaderLayouts.Num(); Index++)
{
		if (!(ShaderLayouts[Index] == Rhs.ShaderLayouts[Index]))
	{
		return false;
	}
	}

	const uint8* ShaderBindingDataPtr = GetData();
	const uint8* OtherShaderBindingDataPtr = Rhs.GetData();

	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);
		FReadOnlyMeshDrawSingleShaderBindings OtherSingleShaderBindings(Rhs.ShaderLayouts[ShaderBindingsIndex], OtherShaderBindingDataPtr);

		if (SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers.Num())
		{
			const uint8* LooseBindings = SingleShaderBindings.GetLooseDataStart();
			const uint8* OtherLooseBindings = OtherSingleShaderBindings.GetLooseDataStart();
			const uint32 LooseLength = SingleShaderBindings.GetLooseDataSizeBytes();
			const uint32 OtherLength = OtherSingleShaderBindings.GetLooseDataSizeBytes();

			if (LooseLength != OtherLength)
			{
				return false;
			}

			if (memcmp(LooseBindings, OtherLooseBindings, LooseLength) != 0)
			{
				return false;
			}
		}

		FRHISamplerState* const* SamplerBindings = SingleShaderBindings.GetSamplerStart();
		FRHISamplerState* const* OtherSamplerBindings = OtherSingleShaderBindings.GetSamplerStart();
		for (int32 SamplerIndex = 0; SamplerIndex < SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num(); SamplerIndex++)
		{
			const FRHIResource* Sampler = SamplerBindings[SamplerIndex];
			const FRHIResource* OtherSampler = OtherSamplerBindings[SamplerIndex];
			if (Sampler != OtherSampler)
			{
				return false;
			}
		}

		FRHIResource* const* SrvBindings = SingleShaderBindings.GetSRVStart();
		FRHIResource* const* OtherSrvBindings = OtherSingleShaderBindings.GetSRVStart();
		for (int32 SrvIndex = 0; SrvIndex < SingleShaderBindings.ParameterMapInfo.SRVs.Num(); SrvIndex++)
		{
			const FRHIResource* Srv = SrvBindings[SrvIndex];
			const FRHIResource* OtherSrv = OtherSrvBindings[SrvIndex];
			if (Srv != OtherSrv)
			{
				return false;
			}
		}

		FRHIUniformBuffer* const* UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
		FRHIUniformBuffer* const* OtherUniformBufferBindings = OtherSingleShaderBindings.GetUniformBufferStart();
		for (int32 UniformBufferIndex = 0; UniformBufferIndex < SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num(); UniformBufferIndex++)
		{
			const FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];
			const FRHIUniformBuffer* OtherUniformBuffer = OtherUniformBufferBindings[UniformBufferIndex];
			
			if (UniformBuffer != OtherUniformBuffer)
			{
				return false;
			}
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
		OtherShaderBindingDataPtr += Rhs.ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}

	return true;
}

uint32 FMeshDrawShaderBindings::GetDynamicInstancingHash() const
{
	//add and initialize any leftover padding within the struct to avoid unstable keys
	struct FHashKey
	{
		uint32 LooseParametersHash = 0;
		uint32 UniformBufferHash = 0;
		uint16 Size;
		uint16 Frequencies;
		static inline uint32 PointerHash(const void* Key)
		{
#if PLATFORM_64BITS
			// Ignoring the lower 4 bits since they are likely zero anyway.
			// Higher bits are more significant in 64 bit builds.
			return reinterpret_cast<UPTRINT>(Key) >> 4;
#else
			return reinterpret_cast<UPTRINT>(Key);
#endif
		};

		static inline uint32 HashCombine(uint32 A, uint32 B)
	{
			return A ^ (B + 0x9e3779b9 + (A << 6) + (A >> 2));
	}
	} HashKey;

	HashKey.Size = Size;
	HashKey.Frequencies = ShaderFrequencyBits;

	const uint8* ShaderBindingDataPtr = GetData();
	for (int32 ShaderBindingsIndex = 0; ShaderBindingsIndex < ShaderLayouts.Num(); ShaderBindingsIndex++)
	{
		FReadOnlyMeshDrawSingleShaderBindings SingleShaderBindings(ShaderLayouts[ShaderBindingsIndex], ShaderBindingDataPtr);

		if (SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers.Num())
		{
			const uint8* LooseBindings = SingleShaderBindings.GetLooseDataStart();
			uint32 Length = SingleShaderBindings.GetLooseDataSizeBytes();
			HashKey.LooseParametersHash = uint32(CityHash64((const char*)LooseBindings, Length));
		}

		FRHISamplerState* const* SamplerBindings = SingleShaderBindings.GetSamplerStart();
		for (int32 SamplerIndex = 0; SamplerIndex < SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num(); SamplerIndex++)
		{
			const FRHIResource* Sampler = SamplerBindings[SamplerIndex];
			HashKey.LooseParametersHash = FHashKey::HashCombine(FHashKey::PointerHash(Sampler), HashKey.LooseParametersHash);
		}

		FRHIResource* const* SrvBindings = SingleShaderBindings.GetSRVStart();
		for (int32 SrvIndex = 0; SrvIndex < SingleShaderBindings.ParameterMapInfo.SRVs.Num(); SrvIndex++)
		{
			const FRHIResource* Srv = SrvBindings[SrvIndex];
			HashKey.LooseParametersHash = FHashKey::HashCombine(FHashKey::PointerHash(Srv), HashKey.LooseParametersHash);
		}

		FRHIUniformBuffer* const* UniformBufferBindings = SingleShaderBindings.GetUniformBufferStart();
		for (int32 UniformBufferIndex = 0; UniformBufferIndex < SingleShaderBindings.ParameterMapInfo.UniformBuffers.Num(); UniformBufferIndex++)
		{
			const FRHIUniformBuffer* UniformBuffer = UniformBufferBindings[UniformBufferIndex];
			HashKey.UniformBufferHash = FHashKey::HashCombine(FHashKey::PointerHash(UniformBuffer), HashKey.UniformBufferHash);
		}

		ShaderBindingDataPtr += ShaderLayouts[ShaderBindingsIndex].GetDataSizeBytes();
	}

	return uint32(CityHash64((char*)&HashKey, sizeof(FHashKey)));
}

bool FMeshDrawCommand::SubmitDrawBegin(
	const FMeshDrawCommand& RESTRICT MeshDrawCommand, 
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	FRHIBuffer* ScenePrimitiveIdsBuffer,
	int32 PrimitiveIdOffset,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FMeshDrawCommandStateCache& RESTRICT StateCache)
{
	checkSlow(MeshDrawCommand.CachedPipelineId.IsValid());
	// GPUCULL_TODO: Can't do this check as the VFs are created with GMaxRHIFeatureLevel (so may support PrimitiveIdStreamIndex even for preview platforms)
	// Want to be sure that we supply GPU-scene instance data if required.
	// checkSlow(MeshDrawCommand.PrimitiveIdStreamIndex == -1 || ScenePrimitiveIdsBuffer != nullptr);
	
	const FGraphicsMinimalPipelineStateInitializer& MeshPipelineState = MeshDrawCommand.CachedPipelineId.GetPipelineState(GraphicsMinimalPipelineStateSet);

	if (MeshDrawCommand.CachedPipelineId.GetId() != StateCache.PipelineId)
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit = MeshPipelineState.AsGraphicsPipelineStateInitializer();
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		EPSOPrecacheResult PSOPrecacheResult = EPSOPrecacheResult::Unknown;
#if PSO_PRECACHING_VALIDATE
		// Retrieve state from cache - can be be cached on the MeshPipelineState to not retrieve it anymore after the final state is known
#if MESH_DRAW_COMMAND_DEBUG_DATA
		PSOPrecacheResult = PSOCollectorStats::CheckPipelineStateInCache(GraphicsPSOInit, MeshDrawCommand.DebugData.MeshPassType, MeshDrawCommand.DebugData.VertexFactoryType);
#else
		PSOPrecacheResult = PSOCollectorStats::CheckPipelineStateInCache(GraphicsPSOInit, EMeshPass::Num, nullptr);
#endif // MESH_DRAW_COMMAND_DEBUG_DATA
#endif // PSO_PRECACHING_VALIDATE

		// Check if skip draw is needed when the PSO is still precaching
		if (GSkipDrawOnPSOPrecaching && !MeshPipelineState.bPSOPrecached)
		{
			MeshPipelineState.bPSOPrecached = !PipelineStateCache::IsPrecaching(GraphicsPSOInit);

			// Try and skip draw if not precached yet
			if (!MeshPipelineState.bPSOPrecached)
			{
				return false;
			}
		}

		// We can set the new StencilRef here to avoid the set below
		bool bApplyAdditionalState = true;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, MeshDrawCommand.StencilRef, EApplyRendertargetOption::CheckApply, bApplyAdditionalState, PSOPrecacheResult);
		StateCache.SetPipelineState(MeshDrawCommand.CachedPipelineId.GetId());
		StateCache.StencilRef = MeshDrawCommand.StencilRef;
	}

	if (MeshDrawCommand.StencilRef != StateCache.StencilRef)
	{
		RHICmdList.SetStencilRef(MeshDrawCommand.StencilRef);
		StateCache.StencilRef = MeshDrawCommand.StencilRef;
	}

	for (int32 VertexBindingIndex = 0; VertexBindingIndex < MeshDrawCommand.VertexStreams.Num(); VertexBindingIndex++)
	{
		const FVertexInputStream& Stream = MeshDrawCommand.VertexStreams[VertexBindingIndex];

		if (MeshDrawCommand.PrimitiveIdStreamIndex != -1 && Stream.StreamIndex == MeshDrawCommand.PrimitiveIdStreamIndex)
		{
			RHICmdList.SetStreamSource(Stream.StreamIndex, ScenePrimitiveIdsBuffer, PrimitiveIdOffset);
			StateCache.VertexStreams[Stream.StreamIndex] = Stream;
		}
		else if (StateCache.VertexStreams[Stream.StreamIndex] != Stream)
		{
			RHICmdList.SetStreamSource(Stream.StreamIndex, Stream.VertexBuffer, Stream.Offset);
			StateCache.VertexStreams[Stream.StreamIndex] = Stream;
		}
	}

	MeshDrawCommand.ShaderBindings.SetOnCommandList(RHICmdList, MeshPipelineState.BoundShaderState.AsBoundShaderState(), StateCache.ShaderBindings);

	return true;
}

void FMeshDrawCommand::SubmitDrawEnd(const FMeshDrawCommand& MeshDrawCommand, uint32 InstanceFactor, FRHICommandList& RHICmdList,
	FRHIBuffer* IndirectArgsOverrideBuffer,
	uint32 IndirectArgsOverrideByteOffset)
{
	const bool bDoOverrideArgs = IndirectArgsOverrideBuffer != nullptr && MeshDrawCommand.PrimitiveIdStreamIndex >= 0;

	if (MeshDrawCommand.IndexBuffer)
	{
		if (MeshDrawCommand.NumPrimitives > 0 && !bDoOverrideArgs)
		{
			RHICmdList.DrawIndexedPrimitive(
				MeshDrawCommand.IndexBuffer,
				MeshDrawCommand.VertexParams.BaseVertexIndex,
				0,
				MeshDrawCommand.VertexParams.NumVertices,
				MeshDrawCommand.FirstIndex,
				MeshDrawCommand.NumPrimitives,
				MeshDrawCommand.NumInstances * InstanceFactor
			);
		}
		else
		{
			RHICmdList.DrawIndexedPrimitiveIndirect(
				MeshDrawCommand.IndexBuffer,
				bDoOverrideArgs ? IndirectArgsOverrideBuffer : MeshDrawCommand.IndirectArgs.Buffer,
				bDoOverrideArgs ? IndirectArgsOverrideByteOffset : MeshDrawCommand.IndirectArgs.Offset
			);
		}
	}
	else
	{
		if (MeshDrawCommand.NumPrimitives > 0 && !bDoOverrideArgs)
		{
			RHICmdList.DrawPrimitive(
				MeshDrawCommand.VertexParams.BaseVertexIndex + MeshDrawCommand.FirstIndex,
				MeshDrawCommand.NumPrimitives,
				MeshDrawCommand.NumInstances * InstanceFactor);
		}
		else
		{
			RHICmdList.DrawPrimitiveIndirect(
				bDoOverrideArgs ? IndirectArgsOverrideBuffer : MeshDrawCommand.IndirectArgs.Buffer,
				bDoOverrideArgs ? IndirectArgsOverrideByteOffset : MeshDrawCommand.IndirectArgs.Offset
			);
		}
	}
}

bool FMeshDrawCommand::SubmitDrawIndirectBegin(
	const FMeshDrawCommand& RESTRICT MeshDrawCommand,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	FRHIBuffer* ScenePrimitiveIdsBuffer,
	int32 PrimitiveIdOffset,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FMeshDrawCommandStateCache& RESTRICT StateCache)
{
	return SubmitDrawBegin(
		MeshDrawCommand,
		GraphicsMinimalPipelineStateSet,
		ScenePrimitiveIdsBuffer,
		PrimitiveIdOffset,
		InstanceFactor,
		RHICmdList,
		StateCache
	);
}

void FMeshDrawCommand::SubmitDrawIndirectEnd(
	const FMeshDrawCommand& MeshDrawCommand,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FRHIBuffer* IndirectArgsOverrideBuffer,
	uint32 IndirectArgsOverrideByteOffset)
{
	FRHIBuffer* IndirectArgsBuffer = nullptr;
	uint32		IndirectArgsOffset = 0;

	if (MeshDrawCommand.NumPrimitives == 0)
	{
		IndirectArgsBuffer = MeshDrawCommand.IndirectArgs.Buffer;
		IndirectArgsOffset = MeshDrawCommand.IndirectArgs.Offset;
	}

	if (IndirectArgsOverrideBuffer != nullptr)
	{
		IndirectArgsBuffer = IndirectArgsOverrideBuffer;
		IndirectArgsOffset = IndirectArgsOverrideByteOffset;
	}

	if (IndirectArgsBuffer != nullptr)
	{
		if (MeshDrawCommand.IndexBuffer)
		{
			RHICmdList.DrawIndexedPrimitiveIndirect(
				MeshDrawCommand.IndexBuffer,
				IndirectArgsBuffer,
				IndirectArgsOffset
			);
		}
		else
		{
			RHICmdList.DrawPrimitiveIndirect(
				IndirectArgsBuffer,
				IndirectArgsOffset
			);
		}
	}
	else if (MeshDrawCommand.NumPrimitives > 0)
	{
		if (MeshDrawCommand.IndexBuffer)
		{
			RHICmdList.DrawIndexedPrimitive(
				MeshDrawCommand.IndexBuffer,
				MeshDrawCommand.VertexParams.BaseVertexIndex,
				0,
				MeshDrawCommand.VertexParams.NumVertices,
				MeshDrawCommand.FirstIndex,
				MeshDrawCommand.NumPrimitives,
				MeshDrawCommand.NumInstances * InstanceFactor
			);
		}
		else
		{
			RHICmdList.DrawPrimitive(
				MeshDrawCommand.VertexParams.BaseVertexIndex + MeshDrawCommand.FirstIndex,
				MeshDrawCommand.NumPrimitives,
				MeshDrawCommand.NumInstances * InstanceFactor
			);
		}
	}
}

void FMeshDrawCommand::SubmitDraw(
	const FMeshDrawCommand& RESTRICT MeshDrawCommand,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	FRHIBuffer* ScenePrimitiveIdsBuffer,
	int32 PrimitiveIdOffset,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FMeshDrawCommandStateCache& RESTRICT StateCache,
	FRHIBuffer* IndirectArgsOverrideBuffer,
	uint32 IndirectArgsOverrideByteOffset)
{
#if MESH_DRAW_COMMAND_DEBUG_DATA && RHI_WANT_BREADCRUMB_EVENTS
	if (MeshDrawCommand.DebugData.ResourceName.IsValid())
	{
		TCHAR NameBuffer[FName::StringBufferSize];
		const uint32 NameLen = MeshDrawCommand.DebugData.ResourceName.ToString(NameBuffer);
		BREADCRUMB_EVENTF(RHICmdList, MeshDrawCommand, TEXT("%s %.*s"), *MeshDrawCommand.DebugData.MaterialName, NameLen, NameBuffer);
	}
	else
	{
		BREADCRUMB_EVENTF(RHICmdList, MeshDrawCommand, TEXT("%s"), *MeshDrawCommand.DebugData.MaterialName);
	}
#endif
#if WANTS_DRAW_MESH_EVENTS
	FMeshDrawEvent MeshEvent(MeshDrawCommand, InstanceFactor, RHICmdList);
#endif
	if (SubmitDrawBegin(MeshDrawCommand, GraphicsMinimalPipelineStateSet, ScenePrimitiveIdsBuffer, PrimitiveIdOffset, InstanceFactor, RHICmdList, StateCache))
	{
		SubmitDrawEnd(MeshDrawCommand, InstanceFactor, RHICmdList, IndirectArgsOverrideBuffer, IndirectArgsOverrideByteOffset);
	}
}

void ApplyTargetsInfo(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FGraphicsPipelineRenderTargetsInfo& RenderTargetsInfo)
{
	GraphicsPSOInit.RenderTargetsEnabled = RenderTargetsInfo.RenderTargetsEnabled;
	GraphicsPSOInit.RenderTargetFormats = RenderTargetsInfo.RenderTargetFormats;
	GraphicsPSOInit.RenderTargetFlags = RenderTargetsInfo.RenderTargetFlags;
	GraphicsPSOInit.NumSamples = RenderTargetsInfo.NumSamples;

	GraphicsPSOInit.DepthStencilTargetFormat = RenderTargetsInfo.DepthStencilTargetFormat;
	GraphicsPSOInit.DepthStencilTargetFlag = RenderTargetsInfo.DepthStencilTargetFlag;

	GraphicsPSOInit.DepthTargetLoadAction = RenderTargetsInfo.DepthTargetLoadAction;
	GraphicsPSOInit.DepthTargetStoreAction = RenderTargetsInfo.DepthTargetStoreAction;
	GraphicsPSOInit.StencilTargetLoadAction = RenderTargetsInfo.StencilTargetLoadAction;
	GraphicsPSOInit.StencilTargetStoreAction = RenderTargetsInfo.StencilTargetStoreAction;
	GraphicsPSOInit.DepthStencilAccess = RenderTargetsInfo.DepthStencilAccess;

	GraphicsPSOInit.MultiViewCount = RenderTargetsInfo.MultiViewCount;
	GraphicsPSOInit.bHasFragmentDensityAttachment = RenderTargetsInfo.bHasFragmentDensityAttachment;
}

uint64 FMeshDrawCommand::GetPipelineStateSortingKey(FRHICommandList& RHICmdList, const FGraphicsPipelineRenderTargetsInfo& RenderTargetsInfo) const
{
	// Default fallback sort key
	uint64 SortKey = CachedPipelineId.GetId();

	if (GRHISupportsPipelineStateSortKey)
	{
		FGraphicsMinimalPipelineStateSet PipelineStateSet;
		FGraphicsPipelineStateInitializer GraphicsPSOInit = CachedPipelineId.GetPipelineState(PipelineStateSet).AsGraphicsPipelineStateInitializer();
		ApplyTargetsInfo(GraphicsPSOInit, RenderTargetsInfo);

		// PSO is retrieved here already. This is currently only used by Nanite::DrawLumenMeshCards - can this also used the version without command list?
		const FGraphicsPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateGraphicsPipelineState(RHICmdList, GraphicsPSOInit, EApplyRendertargetOption::DoNothing, EPSOPrecacheResult::Unknown);
		if (PipelineState)
		{
			const uint64 StateSortKey = PipelineStateCache::RetrieveGraphicsPipelineStateSortKey(PipelineState);
			if (StateSortKey != 0) // 0 on the first occurrence (prior to caching), so these commands will fall back on shader id for sorting.
			{
				SortKey = StateSortKey;
			}
		}
	}

	return SortKey;
}

uint64 FMeshDrawCommand::GetPipelineStateSortingKey(const FGraphicsPipelineRenderTargetsInfo& RenderTargetsInfo) const
{
	// Default fallback sort key
	uint64 SortKey = CachedPipelineId.GetId();

	if (GRHISupportsPipelineStateSortKey)
	{
		FGraphicsMinimalPipelineStateSet PipelineStateSet;
		FGraphicsPipelineStateInitializer GraphicsPSOInit = CachedPipelineId.GetPipelineState(PipelineStateSet).AsGraphicsPipelineStateInitializer();
		ApplyTargetsInfo(GraphicsPSOInit, RenderTargetsInfo);

		const FGraphicsPipelineState* PipelineState = PipelineStateCache::FindGraphicsPipelineState(GraphicsPSOInit);
		if (PipelineState)
		{
			const uint64 StateSortKey = PipelineStateCache::RetrieveGraphicsPipelineStateSortKey(PipelineState);
			if (StateSortKey != 0) // 0 on the first occurrence (prior to caching), so these commands will fall back on shader id for sorting.
			{
				SortKey = StateSortKey;
			}
		}
	}

	return SortKey;
}

#if MESH_DRAW_COMMAND_DEBUG_DATA
void FMeshDrawCommand::SetDebugData(const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterial* Material, const FMaterialRenderProxy* MaterialRenderProxy, const FMeshProcessorShaders& UntypedShaders, const FVertexFactory* VertexFactory, uint32 MeshPassType)
{
	DebugData.PrimitiveSceneProxyIfNotUsingStateBuckets = PrimitiveSceneProxy;
	DebugData.MaterialRenderProxy = MaterialRenderProxy;
	DebugData.VertexShader = UntypedShaders.VertexShader;
	DebugData.PixelShader = UntypedShaders.PixelShader;
	DebugData.VertexFactory = VertexFactory;
	DebugData.VertexFactoryType = VertexFactory->GetType();
	DebugData.MeshPassType = MeshPassType;
	DebugData.ResourceName =  PrimitiveSceneProxy ? PrimitiveSceneProxy->GetResourceName() : FName();
	DebugData.MaterialName = Material->GetAssetName();
}
#endif

void SubmitMeshDrawCommands(
	const FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet, 
	FRHIBuffer* PrimitiveIdsBuffer,
	uint32 PrimitiveIdBufferStride,
	int32 BasePrimitiveIdsOffset,
	bool bDynamicInstancing,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList)
{
	SubmitMeshDrawCommandsRange(VisibleMeshDrawCommands, GraphicsMinimalPipelineStateSet, PrimitiveIdsBuffer, PrimitiveIdBufferStride, BasePrimitiveIdsOffset, bDynamicInstancing, 0, VisibleMeshDrawCommands.Num(), InstanceFactor, RHICmdList);
}

void SubmitMeshDrawCommandsRange(
	const FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	FRHIBuffer* PrimitiveIdsBuffer,
	uint32 PrimitiveIdBufferStride,
	int32 BasePrimitiveIdsOffset,
	bool bDynamicInstancing,
	int32 StartIndex,
	int32 NumMeshDrawCommands,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList)
{
	// GPUCULL_TODO: workaround for the fact that DrawDynamicMeshPassPrivate et al. don't work with GPU-Scene instancing
	//               we don't support dynamic instancing for this path since we require one primitive per draw command
	//               This is because the stride on the instance data buffer is set to 0 so only the first will ever be fetched.
	checkSlow(!bDynamicInstancing);
	bDynamicInstancing = false;

	FMeshDrawCommandStateCache StateCache;
	INC_DWORD_STAT_BY(STAT_MeshDrawCalls, NumMeshDrawCommands);

	for (int32 DrawCommandIndex = StartIndex; DrawCommandIndex < StartIndex + NumMeshDrawCommands; DrawCommandIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, MeshEvent, GEmitMeshDrawEvent != 0, TEXT("Mesh Draw"));

		const FVisibleMeshDrawCommand& VisibleMeshDrawCommand = VisibleMeshDrawCommands[DrawCommandIndex];
		const int32 PrimitiveIdBufferOffset = BasePrimitiveIdsOffset + (bDynamicInstancing ? VisibleMeshDrawCommand.PrimitiveIdBufferOffset : DrawCommandIndex) * PrimitiveIdBufferStride;
		checkSlow(!bDynamicInstancing || VisibleMeshDrawCommand.PrimitiveIdBufferOffset >= 0);
		FMeshDrawCommand::SubmitDraw(*VisibleMeshDrawCommand.MeshDrawCommand, GraphicsMinimalPipelineStateSet, PrimitiveIdsBuffer, PrimitiveIdBufferOffset, InstanceFactor, RHICmdList, StateCache);
	}
}

void ApplyViewOverridesToMeshDrawCommands(const FSceneView& View, FMeshCommandOneFrameArray& VisibleMeshDrawCommands, FDynamicMeshDrawCommandStorage& DynamicMeshDrawCommandStorage, FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet, bool& InNeedsShaderInitialisation)
{
	if (View.bReverseCulling || View.bRenderSceneTwoSided)
	{
		const FMeshCommandOneFrameArray& PassVisibleMeshDrawCommands = VisibleMeshDrawCommands;

		FMeshCommandOneFrameArray ViewOverriddenMeshCommands;
		ViewOverriddenMeshCommands.Empty(PassVisibleMeshDrawCommands.Num());

		for (int32 MeshCommandIndex = 0; MeshCommandIndex < PassVisibleMeshDrawCommands.Num(); MeshCommandIndex++)
		{
			DynamicMeshDrawCommandStorage.MeshDrawCommands.Add(1);
			FMeshDrawCommand& NewMeshCommand = DynamicMeshDrawCommandStorage.MeshDrawCommands[DynamicMeshDrawCommandStorage.MeshDrawCommands.Num() - 1];

			const FVisibleMeshDrawCommand& VisibleMeshDrawCommand = PassVisibleMeshDrawCommands[MeshCommandIndex];
			const FMeshDrawCommand& MeshCommand = *VisibleMeshDrawCommand.MeshDrawCommand;
			NewMeshCommand = MeshCommand;

			const ERasterizerCullMode LocalCullMode = View.bRenderSceneTwoSided ? CM_None : View.bReverseCulling ? FMeshPassProcessor::InverseCullMode(VisibleMeshDrawCommand.MeshCullMode) : VisibleMeshDrawCommand.MeshCullMode;

			FGraphicsMinimalPipelineStateInitializer PipelineState = MeshCommand.CachedPipelineId.GetPipelineState(GraphicsMinimalPipelineStateSet);
			PipelineState.RasterizerState = GetStaticRasterizerState<true>(VisibleMeshDrawCommand.MeshFillMode, LocalCullMode);

			const FGraphicsMinimalPipelineStateId PipelineId = FGraphicsMinimalPipelineStateId::GetPipelineStateId(PipelineState, GraphicsMinimalPipelineStateSet, InNeedsShaderInitialisation);
			NewMeshCommand.Finalize(PipelineId, nullptr);

			FVisibleMeshDrawCommand NewVisibleMeshDrawCommand;

			NewVisibleMeshDrawCommand.Setup(
				&NewMeshCommand,
				VisibleMeshDrawCommand.PrimitiveIdInfo,
				VisibleMeshDrawCommand.StateBucketId,
				VisibleMeshDrawCommand.MeshFillMode,
				VisibleMeshDrawCommand.MeshCullMode,
				VisibleMeshDrawCommand.Flags,
				VisibleMeshDrawCommand.SortKey,
				VisibleMeshDrawCommand.RunArray,
				VisibleMeshDrawCommand.NumRuns);

			ViewOverriddenMeshCommands.Add(NewVisibleMeshDrawCommand);
		}

		// Replace VisibleMeshDrawCommands
		FMemory::Memswap(&VisibleMeshDrawCommands, &ViewOverriddenMeshCommands, sizeof(ViewOverriddenMeshCommands));
	}
}

void DrawDynamicMeshPassPrivate(
	const FSceneView& View,
	FRHICommandList& RHICmdList,
	FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FDynamicMeshDrawCommandStorage& DynamicMeshDrawCommandStorage,
	FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	bool& InNeedsShaderInitialisation,
	uint32 InstanceFactor)
{
	if (VisibleMeshDrawCommands.Num() > 0)
	{
		// GPUCULL_TODO: workaround for the fact that DrawDynamicMeshPassPrivate et al. don't work with GPU-Scene instancing
		//               we don't support dynamic instancing for this path since we require one primitive per draw command
		//               This is because the stride on the instance data buffer is set to 0 so only the first will ever be fetched.
		const bool bDynamicInstancing = false;

		FRHIBuffer* PrimitiveIdVertexBuffer = nullptr;
		const uint32 PrimitiveIdBufferStride = FInstanceCullingContext::GetInstanceIdBufferStride(View.GetFeatureLevel());

		ApplyViewOverridesToMeshDrawCommands(View, VisibleMeshDrawCommands, DynamicMeshDrawCommandStorage, GraphicsMinimalPipelineStateSet, InNeedsShaderInitialisation);

		check(View.bIsViewInfo);
		const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(&View);
#if DO_GUARD_SLOW
		if (UseGPUScene(View.GetShaderPlatform(), View.GetFeatureLevel()))
		{
			bool bNeedsGPUSceneData = false;
			for (const auto& VisibleMeshDrawCommand : VisibleMeshDrawCommands)
			{
				bNeedsGPUSceneData = bNeedsGPUSceneData || EnumHasAnyFlags(VisibleMeshDrawCommand.Flags, EFVisibleMeshDrawCommandFlags::HasPrimitiveIdStreamIndex);
			}
			ensure(!bNeedsGPUSceneData || ViewInfo->CachedViewUniformShaderParameters->PrimitiveSceneData != GIdentityPrimitiveBuffer.PrimitiveSceneDataBufferSRV);
			ensure(!bNeedsGPUSceneData || ViewInfo->CachedViewUniformShaderParameters->InstanceSceneData != GIdentityPrimitiveBuffer.InstanceSceneDataBufferSRV);
			ensure(!bNeedsGPUSceneData || ViewInfo->CachedViewUniformShaderParameters->InstancePayloadData != GIdentityPrimitiveBuffer.InstancePayloadDataBufferSRV);
		}
#endif // DO_GUARD_SLOW
		SortAndMergeDynamicPassMeshDrawCommands(View, RHICmdList, VisibleMeshDrawCommands, DynamicMeshDrawCommandStorage, PrimitiveIdVertexBuffer, InstanceFactor, &ViewInfo->DynamicPrimitiveCollector);

		SubmitMeshDrawCommandsRange(VisibleMeshDrawCommands, GraphicsMinimalPipelineStateSet, PrimitiveIdVertexBuffer, PrimitiveIdBufferStride, 0, bDynamicInstancing, 0, VisibleMeshDrawCommands.Num(), InstanceFactor, RHICmdList);
	}
}


FMeshDrawCommandSortKey CalculateMeshStaticSortKey(const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader)
{
	FMeshDrawCommandSortKey SortKey;
	SortKey.Generic.VertexShaderHash = VertexShader ? VertexShader->GetSortKey() : 0;
	SortKey.Generic.PixelShaderHash = PixelShader ? PixelShader->GetSortKey() : 0;

	return SortKey;
}

FMeshPassProcessor::FMeshPassProcessor(EMeshPass::Type InMeshPassType, const FScene* InScene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
	: MeshPassType(InMeshPassType)
	, Scene(InScene)
	, FeatureLevel(InFeatureLevel)
	, ViewIfDynamicMeshCommand(InViewIfDynamicMeshCommand)
	, DrawListContext(InDrawListContext)
{
}

FMeshPassProcessor::FMeshDrawingPolicyOverrideSettings FMeshPassProcessor::ComputeMeshOverrideSettings(const FPSOPrecacheParams& PrecachePSOParams)
{
	FMeshDrawingPolicyOverrideSettings OverrideSettings;
	OverrideSettings.MeshPrimitiveType = PT_TriangleList;
		
	OverrideSettings.MeshOverrideFlags |= PrecachePSOParams.bDisableBackFaceCulling ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= PrecachePSOParams.bReverseCulling ? EDrawingPolicyOverrideFlags::ReverseCullMode : EDrawingPolicyOverrideFlags::None;
	return OverrideSettings;
}

FMeshPassProcessor::FMeshDrawingPolicyOverrideSettings FMeshPassProcessor::ComputeMeshOverrideSettings(const FMeshBatch& Mesh)
{
	FMeshDrawingPolicyOverrideSettings OverrideSettings;
	OverrideSettings.MeshPrimitiveType = (EPrimitiveType)Mesh.Type;

	OverrideSettings.MeshOverrideFlags |= Mesh.bDisableBackfaceCulling ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.bDitheredLODTransition ? EDrawingPolicyOverrideFlags::DitheredLODTransition : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.bWireframe ? EDrawingPolicyOverrideFlags::Wireframe : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.ReverseCulling ? EDrawingPolicyOverrideFlags::ReverseCullMode : EDrawingPolicyOverrideFlags::None;
	return OverrideSettings;
}

ERasterizerFillMode FMeshPassProcessor::ComputeMeshFillMode(const FMaterial& InMaterialResource, const FMeshDrawingPolicyOverrideSettings& InOverrideSettings)
{
	const bool bMaterialResourceIsTwoSided = InMaterialResource.IsTwoSided();
	const bool bIsWireframeMaterial = InMaterialResource.IsWireframe() || !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::Wireframe);
	return bIsWireframeMaterial ? FM_Wireframe : FM_Solid;
}

ERasterizerCullMode FMeshPassProcessor::ComputeMeshCullMode(const FMaterial& InMaterialResource, const FMeshDrawingPolicyOverrideSettings& InOverrideSettings)
{
	const bool bMaterialResourceIsTwoSided = InMaterialResource.IsTwoSided();
	const bool bInTwoSidedOverride = !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::TwoSided);
	const bool bInReverseCullModeOverride = !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::ReverseCullMode);
	const bool bIsTwoSided = (bMaterialResourceIsTwoSided || bInTwoSidedOverride);
	const bool bMeshRenderTwoSided = bIsTwoSided || bInTwoSidedOverride;
	return bMeshRenderTwoSided ? CM_None : (bInReverseCullModeOverride ? CM_CCW : CM_CW);
}

void FMeshPassProcessor::GetDrawCommandPrimitiveId(
	const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo,
	const FMeshBatchElement& BatchElement,
	int32& DrawPrimitiveId,
	int32& ScenePrimitiveId) const
{
	FMeshDrawCommandPrimitiveIdInfo PrimitiveIdInfo = GetDrawCommandPrimitiveId(PrimitiveSceneInfo, BatchElement);
	DrawPrimitiveId = PrimitiveIdInfo.DrawPrimitiveId;
	ScenePrimitiveId = PrimitiveIdInfo.ScenePrimitiveId;
}

FMeshDrawCommandPrimitiveIdInfo FMeshPassProcessor::GetDrawCommandPrimitiveId(
	const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo,
	const FMeshBatchElement& BatchElement) const
{
	FMeshDrawCommandPrimitiveIdInfo PrimitiveIdInfo = FMeshDrawCommandPrimitiveIdInfo(0, -1);

	if (UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
	{
		if (BatchElement.PrimitiveIdMode == PrimID_FromPrimitiveSceneInfo)
		{
			ensureMsgf(BatchElement.PrimitiveUniformBufferResource == nullptr, TEXT("PrimitiveUniformBufferResource should not be setup when PrimitiveIdMode == PrimID_FromPrimitiveSceneInfo"));
			check(PrimitiveSceneInfo);
			PrimitiveIdInfo.DrawPrimitiveId = PrimitiveSceneInfo->GetIndex();
			PrimitiveIdInfo.InstanceSceneDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();
			PrimitiveIdInfo.bIsDynamicPrimitive = 0U;
		}
		else if (BatchElement.PrimitiveIdMode == PrimID_DynamicPrimitiveShaderData && ViewIfDynamicMeshCommand != nullptr)
		{
			// Mark using GPrimIDDynamicFlag (top bit) as we defer this to later.
			PrimitiveIdInfo.DrawPrimitiveId = BatchElement.DynamicPrimitiveIndex | GPrimIDDynamicFlag;
			PrimitiveIdInfo.InstanceSceneDataOffset = BatchElement.DynamicPrimitiveInstanceSceneDataOffset;
			PrimitiveIdInfo.bIsDynamicPrimitive = 1U;
		}
		else
		{
			check(BatchElement.PrimitiveIdMode == PrimID_ForceZero);
		}
	}

	PrimitiveIdInfo.ScenePrimitiveId = PrimitiveSceneInfo ? PrimitiveSceneInfo->GetIndex() : -1;

	return PrimitiveIdInfo;
}

bool FMeshPassProcessor::ShouldSkipMeshDrawCommand(const FMeshBatch& RESTRICT MeshBatch, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy) const
{
	bool bSkipMeshDrawCommand = false;

#if WITH_EDITORONLY_DATA
	// Support debug mode to render only non-Nanite proxies that incorrectly reference coarse mesh static mesh assets.
	if (GNaniteIsolateInvalidCoarseMesh != 0)
	{
		// Skip everything by default
		bSkipMeshDrawCommand = true;

		const bool bNaniteProxy = PrimitiveSceneProxy != nullptr && PrimitiveSceneProxy->IsNaniteMesh();
		if (!bNaniteProxy && MeshBatch.VertexFactory != nullptr)
		{
			// Only skip if the referenced static mesh is not a generated Nanite coarse mesh
			const bool bIsCoarseProxy = MeshBatch.VertexFactory->IsCoarseProxyMesh();
			if (bIsCoarseProxy)
			{
				bSkipMeshDrawCommand = false;
			}
		}
	}
#endif

	return bSkipMeshDrawCommand;
}

FCachedPassMeshDrawListContext::FCachedPassMeshDrawListContext(FScene& InScene)
	: Scene(InScene)
	, bUseGPUScene(UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel))
	, bUseStateBucketsAuxData(InScene.GetShadingPath() == EShadingPath::Mobile)
{
}

FMeshDrawCommand& FCachedPassMeshDrawListContext::AddCommand(FMeshDrawCommand& Initializer, uint32 NumElements)
{
	checkf(CurrMeshPass < EMeshPass::Num, TEXT("BeginMeshPass() must be called before adding commands to this context"));
	ensureMsgf(CommandInfo.CommandIndex == -1 && CommandInfo.StateBucketId == -1, TEXT("GetCommandInfoAndReset() wasn't called since the last command was added"));

	if (NumElements == 1)
	{
		return Initializer;
	}
	else
	{
		MeshDrawCommandForStateBucketing = Initializer;
		return MeshDrawCommandForStateBucketing;
	}
}

void FCachedPassMeshDrawListContext::BeginMeshPass(EMeshPass::Type MeshPass)
{
	checkf(CurrMeshPass == EMeshPass::Num, TEXT("BeginMeshPass() was called without a matching EndMeshPass()"));
	check(MeshPass < EMeshPass::Num);
	CurrMeshPass = MeshPass;
}

void FCachedPassMeshDrawListContext::EndMeshPass()
{
	checkf(CurrMeshPass < EMeshPass::Num, TEXT("EndMeshPass() was called without matching BeginMeshPass()"));
	CurrMeshPass = EMeshPass::Num;
}

FCachedMeshDrawCommandInfo FCachedPassMeshDrawListContext::GetCommandInfoAndReset()
{
	FCachedMeshDrawCommandInfo Ret = CommandInfo;
	CommandInfo.CommandIndex = -1;
	CommandInfo.StateBucketId = -1;
	return Ret;
}

void FCachedPassMeshDrawListContext::FinalizeCommandCommon(
	const FMeshBatch& MeshBatch, 
	int32 BatchElementIndex,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	FMeshDrawCommandSortKey SortKey,
	EFVisibleMeshDrawCommandFlags Flags,
	const FGraphicsMinimalPipelineStateInitializer& PipelineState,
	const FMeshProcessorShaders* ShadersForDebugging,
	FMeshDrawCommand& MeshDrawCommand)
{
	FGraphicsMinimalPipelineStateId PipelineId = FGraphicsMinimalPipelineStateId::GetPersistentId(PipelineState);

	MeshDrawCommand.SetDrawParametersAndFinalize(MeshBatch, BatchElementIndex, PipelineId, ShadersForDebugging);

	CommandInfo = FCachedMeshDrawCommandInfo(CurrMeshPass);
	CommandInfo.SortKey = SortKey;
	CommandInfo.MeshFillMode = MeshFillMode;
	CommandInfo.MeshCullMode = MeshCullMode;
	CommandInfo.Flags = Flags;

#if MESH_DRAW_COMMAND_DEBUG_DATA
	if (bUseGPUScene)
	{
		MeshDrawCommand.ClearDebugPrimitiveSceneProxy(); //When using State Buckets multiple PrimitiveSceneProxies use the same MeshDrawCommand, so The PrimitiveSceneProxy pointer can't be stored.
	}
#endif
#if DO_GUARD_SLOW
	if (bUseGPUScene)
	{
		FMeshDrawCommand MeshDrawCommandDebug = FMeshDrawCommand(MeshDrawCommand);
		check(MeshDrawCommandDebug.ShaderBindings.GetDynamicInstancingHash() == MeshDrawCommand.ShaderBindings.GetDynamicInstancingHash());
		check(MeshDrawCommandDebug.GetDynamicInstancingHash() == MeshDrawCommand.GetDynamicInstancingHash());
	}
	if (Scene.GetShadingPath() == EShadingPath::Deferred)
	{
		ensureMsgf(MeshDrawCommand.VertexStreams.GetAllocatedSize() == 0, TEXT("Cached Mesh Draw command overflows VertexStreams. VertexStream inline size should be tweaked."));

		if (CurrMeshPass == EMeshPass::BasePass || CurrMeshPass == EMeshPass::DepthPass || CurrMeshPass == EMeshPass::CSMShadowDepth || CurrMeshPass == EMeshPass::VSMShadowDepth)
		{
			TArray<EShaderFrequency, TInlineAllocator<SF_NumFrequencies>> ShaderFrequencies;
			MeshDrawCommand.ShaderBindings.GetShaderFrequencies(ShaderFrequencies);

			int32 DataOffset = 0;
			for (int32 i = 0; i < ShaderFrequencies.Num(); i++)
			{
				FMeshDrawSingleShaderBindings SingleShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(ShaderFrequencies[i], DataOffset);
				if (SingleShaderBindings.GetParameterMapInfo().LooseParameterBuffers.Num() != 0)
				{
					bAnyLooseParameterBuffers = true;
				}
				ensureMsgf(SingleShaderBindings.GetParameterMapInfo().SRVs.Num() == 0, TEXT("Cached Mesh Draw command uses individual SRVs.  This will break dynamic instancing in performance critical pass.  Use Uniform Buffers instead."));
				ensureMsgf(SingleShaderBindings.GetParameterMapInfo().TextureSamplers.Num() == 0, TEXT("Cached Mesh Draw command uses individual Texture Samplers.  This will break dynamic instancing in performance critical pass.  Use Uniform Buffers instead."));
			}
		}
	}
#endif
}

void FCachedPassMeshDrawListContextImmediate::FinalizeCommand(
		const FMeshBatch& MeshBatch, 
		int32 BatchElementIndex,
		const FMeshDrawCommandPrimitiveIdInfo& IdInfo,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		FMeshDrawCommandSortKey SortKey,
		EFVisibleMeshDrawCommandFlags Flags,
		const FGraphicsMinimalPipelineStateInitializer& PipelineState,
		const FMeshProcessorShaders* ShadersForDebugging,
		FMeshDrawCommand& MeshDrawCommand)
{
	// disabling this by default as it incurs a high cost in perf captures due to sheer volume.  Recommendation is to re-enable locally if you need to profile this particular code.
	// QUICK_SCOPE_CYCLE_COUNTER(STAT_FinalizeCachedMeshDrawCommand);

	FinalizeCommandCommon(
		MeshBatch, 
		BatchElementIndex,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		Flags,
		PipelineState,
		ShadersForDebugging,
		MeshDrawCommand
	);

	if (bUseGPUScene)
	{
		Experimental::FHashElementId SetId;
		FStateBucketMap& BucketMap = Scene.CachedMeshDrawCommandStateBuckets[CurrMeshPass];
		auto hash = BucketMap.ComputeHash(MeshDrawCommand);
		{
			SetId = BucketMap.FindOrAddIdByHash(hash, MeshDrawCommand, FMeshDrawCommandCount());
			FMeshDrawCommandCount& DrawCount = BucketMap.GetByElementId(SetId).Value;
			DrawCount.Num++;
		}

		CommandInfo.StateBucketId = SetId.GetIndex();

		if (bUseStateBucketsAuxData)
		{
			// grow MDC AuxData array in sync with MDC StateBuckets
			Scene.CachedStateBucketsAuxData[CurrMeshPass].SetNum(BucketMap.GetMaxIndex() + 1, false);
			Scene.CachedStateBucketsAuxData[CurrMeshPass][CommandInfo.StateBucketId] = FStateBucketAuxData(MeshBatch);
		}
	}
	else
	{
		// Only one FMeshDrawCommand supported per FStaticMesh in a pass
		// Allocate at lowest free index so that 'r.DoLazyStaticMeshUpdate' can shrink the TSparseArray more effectively
		FCachedPassMeshDrawList& CachedDrawLists = Scene.CachedDrawLists[CurrMeshPass];
		CommandInfo.CommandIndex = CachedDrawLists.MeshDrawCommands.EmplaceAtLowestFreeIndex(CachedDrawLists.LowestFreeIndexSearchStart, MeshDrawCommand);
	}	
}

void FCachedPassMeshDrawListContextDeferred::FinalizeCommand(
		const FMeshBatch& MeshBatch, 
		int32 BatchElementIndex,
		const FMeshDrawCommandPrimitiveIdInfo& IdInfo,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		FMeshDrawCommandSortKey SortKey,
		EFVisibleMeshDrawCommandFlags Flags,
		const FGraphicsMinimalPipelineStateInitializer& PipelineState,
		const FMeshProcessorShaders* ShadersForDebugging,
		FMeshDrawCommand& MeshDrawCommand)
{
	// disabling this by default as it incurs a high cost in perf captures due to sheer volume.  Recommendation is to re-enable locally if you need to profile this particular code.
	// QUICK_SCOPE_CYCLE_COUNTER(STAT_FinalizeCachedMeshDrawCommand);

	FinalizeCommandCommon(
		MeshBatch, 
		BatchElementIndex,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		Flags,
		PipelineState,
		ShadersForDebugging,
		MeshDrawCommand
	);

	const int32 Index = DeferredCommands.Add(MeshDrawCommand);

	if (bUseGPUScene)
	{
		// Cache the hash here to make the deferred finalize less expensive
		DeferredCommandHashes.Add(FStateBucketMap::ComputeHash(MeshDrawCommand));

		CommandInfo.StateBucketId = Index;

		if (bUseStateBucketsAuxData)
		{
			DeferredStateBucketsAuxData.Add(FStateBucketAuxData(MeshBatch));
		}
	}
	else
	{
		CommandInfo.CommandIndex = Index;
	}
}

void FCachedPassMeshDrawListContextDeferred::DeferredFinalizeMeshDrawCommands(const TArrayView<FPrimitiveSceneInfo*>& SceneInfos, int32 Start, int32 End)
{
	if (bUseGPUScene)
	{
		for (int32 SceneInfoIndex = Start; SceneInfoIndex < End; ++SceneInfoIndex)
		{
			FPrimitiveSceneInfo* SceneInfo = SceneInfos[SceneInfoIndex];
			for (auto& CmdInfo : SceneInfo->StaticMeshCommandInfos)
			{				
				check(CmdInfo.MeshPass < EMeshPass::Num);
				FStateBucketMap& BucketMap = Scene.CachedMeshDrawCommandStateBuckets[CmdInfo.MeshPass];
				int32 DeferredIndex = CmdInfo.StateBucketId;

				check(DeferredIndex >= 0 && DeferredIndex < DeferredCommands.Num());
				check(CmdInfo.CommandIndex == -1);
				FMeshDrawCommand& Command = DeferredCommands[DeferredIndex];
				const Experimental::FHashType CommandHash = DeferredCommandHashes[DeferredIndex];

				Experimental::FHashElementId SetId = BucketMap.FindOrAddIdByHash(CommandHash, MoveTemp(Command), FMeshDrawCommandCount());
				FMeshDrawCommandCount& DrawCount = BucketMap.GetByElementId(SetId).Value;
				DrawCount.Num++;

				CmdInfo.StateBucketId = SetId.GetIndex();

				if (bUseStateBucketsAuxData)
				{
					// grow MDC AuxData array in sync with MDC StateBuckets
					Scene.CachedStateBucketsAuxData[CmdInfo.MeshPass].SetNum(BucketMap.GetMaxIndex() + 1, false);
					Scene.CachedStateBucketsAuxData[CmdInfo.MeshPass][CmdInfo.StateBucketId] = DeferredStateBucketsAuxData[DeferredIndex];
				}
			}
		}
	}
	else
	{
		for (int32 SceneInfoIndex = Start; SceneInfoIndex < End; ++SceneInfoIndex)
		{
			FPrimitiveSceneInfo* SceneInfo = SceneInfos[SceneInfoIndex];
			for (auto& CmdInfo : SceneInfo->StaticMeshCommandInfos)
			{				
				check(CmdInfo.MeshPass < EMeshPass::Num);
				FCachedPassMeshDrawList& CachedDrawLists = Scene.CachedDrawLists[CmdInfo.MeshPass];

				check(CmdInfo.CommandIndex >= 0 && CmdInfo.CommandIndex < DeferredCommands.Num());
				check(CmdInfo.StateBucketId == -1);
				FMeshDrawCommand& Command = DeferredCommands[CmdInfo.CommandIndex];
				
				CmdInfo.CommandIndex = CachedDrawLists.MeshDrawCommands.EmplaceAtLowestFreeIndex(CachedDrawLists.LowestFreeIndexSearchStart, MoveTemp(Command));
			}
		}
	}

	DeferredCommands.Reset();
	DeferredCommandHashes.Reset();
	DeferredStateBucketsAuxData.Reset();
}

PassProcessorCreateFunction FPassProcessorManager::JumpTable[(int32)EShadingPath::Num][EMeshPass::Num] = {};
DeprecatedPassProcessorCreateFunction FPassProcessorManager::DeprecatedJumpTable[(int32)EShadingPath::Num][EMeshPass::Num] = {};
EMeshPassFlags FPassProcessorManager::Flags[(int32)EShadingPath::Num][EMeshPass::Num] = {};

static_assert(EMeshPass::Num <= FPSOCollectorCreateManager::MaxPSOCollectorCount);

void FPassProcessorManager::SetPassFlags(EShadingPath ShadingPath, EMeshPass::Type PassType, EMeshPassFlags NewFlags)
{
	check(IsInGameThread());
	FGlobalComponentRecreateRenderStateContext Context;
	if (JumpTable[(uint32)ShadingPath][PassType])
	{
		Flags[(uint32)ShadingPath][PassType] = NewFlags;
	}
}



#if WANTS_DRAW_MESH_EVENTS
FMeshDrawCommand::FMeshDrawEvent::FMeshDrawEvent(const FMeshDrawCommand& MeshDrawCommand, const uint32 InstanceFactor, FRHICommandList& RHICmdList)
{
	if (GShowMaterialDrawEvents)
	{
		const FString& MaterialName = MeshDrawCommand.DebugData.MaterialName;
		FName ResourceName = MeshDrawCommand.DebugData.ResourceName;

		FString DrawEventName = FString::Printf(
			TEXT("%s %s"),
			// Note: this is the parent's material name, not the material instance
			*MaterialName,
			ResourceName.IsValid() ? *ResourceName.ToString() : TEXT(""));

		const uint32 Instances = MeshDrawCommand.NumInstances * InstanceFactor;
		if (Instances > 1)
		{
			BEGIN_DRAW_EVENTF(
				RHICmdList,
				MaterialEvent,
				*this,
				TEXT("%s %u instances"),
				*DrawEventName,
				Instances);
		}
		else
		{
			BEGIN_DRAW_EVENTF(RHICmdList, MaterialEvent, *this, *DrawEventName);
		}
	}
}
#endif

void AddRenderTargetInfo(
	EPixelFormat PixelFormat,
	ETextureCreateFlags CreateFlags,
	FGraphicsPipelineRenderTargetsInfo& RenderTargetsInfo)
{
	RenderTargetsInfo.RenderTargetFormats[RenderTargetsInfo.RenderTargetsEnabled] = PixelFormat;
	RenderTargetsInfo.RenderTargetFlags[RenderTargetsInfo.RenderTargetsEnabled] = CreateFlags;
	RenderTargetsInfo.RenderTargetsEnabled++;
}

void SetupDepthStencilInfo(
	EPixelFormat DepthStencilFormat,
	ETextureCreateFlags DepthStencilCreateFlags,
	ERenderTargetLoadAction DepthTargetLoadAction,
	ERenderTargetLoadAction StencilTargetLoadAction,
	FExclusiveDepthStencil DepthStencilAccess,
	FGraphicsPipelineRenderTargetsInfo& RenderTargetsInfo)
{
	// Setup depth stencil state
	RenderTargetsInfo.DepthStencilTargetFormat = DepthStencilFormat;
	RenderTargetsInfo.DepthStencilTargetFlag = DepthStencilCreateFlags;

	RenderTargetsInfo.DepthTargetLoadAction = DepthTargetLoadAction;
	RenderTargetsInfo.StencilTargetLoadAction = StencilTargetLoadAction;
	RenderTargetsInfo.DepthStencilAccess = DepthStencilAccess;

	const ERenderTargetStoreAction StoreAction = EnumHasAnyFlags(RenderTargetsInfo.DepthStencilTargetFlag, TexCreate_Memoryless) ? ERenderTargetStoreAction::ENoAction : ERenderTargetStoreAction::EStore;
	RenderTargetsInfo.DepthTargetStoreAction = RenderTargetsInfo.DepthStencilAccess.IsUsingDepth() ? StoreAction : ERenderTargetStoreAction::ENoAction;
	RenderTargetsInfo.StencilTargetStoreAction = RenderTargetsInfo.DepthStencilAccess.IsUsingStencil() ? StoreAction : ERenderTargetStoreAction::ENoAction;
}

void SetupGBufferRenderTargetInfo(const FSceneTexturesConfig& SceneTexturesConfig, FGraphicsPipelineRenderTargetsInfo& RenderTargetsInfo, bool bSetupDepthStencil)
{
	SceneTexturesConfig.GetGBufferRenderTargetsInfo(RenderTargetsInfo);
	if (bSetupDepthStencil)
	{
		SetupDepthStencilInfo(PF_DepthStencil, SceneTexturesConfig.DepthCreateFlags, ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite, RenderTargetsInfo);
	}
}

#if PSO_PRECACHING_VALIDATE

// Stats for PSO shaders only (no state, only VS/PS/GS usage)
static FCriticalSection PSOShadersPrecacheLock;
static PSOCollectorStats::FPrecacheStats PSOShadersPrecacheStats;
static Experimental::TRobinHoodHashMap<FGraphicsMinimalPipelineStateInitializer, PSOCollectorStats::FShaderStateUsage> PSOShadersPrecacheMap;

// Stats for minimal PSO initializer data during MeshDrawCommand building (doesn't contain render target info)
static FCriticalSection MinimalPSOPrecacheLock;
static PSOCollectorStats::FPrecacheStats MinimalPSOPrecacheStats;
static Experimental::TRobinHoodHashMap<uint64, PSOCollectorStats::FShaderStateUsage> MinimalPSOPrecacheMap;

// Helper function to remove certain members of the PSO initializer to help track down issues with PSO precache misses
// eg Remove BlendState and check if the misses are gone for example
static FGraphicsMinimalPipelineStateInitializer PatchMinimalPipelineStateToCheck(const FGraphicsMinimalPipelineStateInitializer& MinimalPSO)
{
	FGraphicsMinimalPipelineStateInitializer PSOInitializeToCheck = MinimalPSO;
	//PSOInitializeToCheck.DepthStencilState = nullptr;
	//PSOInitializeToCheck.RasterizerState = nullptr;
	//PSOInitializeToCheck.BlendState = nullptr;
	//PSOInitializeToCheck.PrimitiveType = PT_TriangleList;
	//PSOInitializeToCheck.ImmutableSamplerState = FImmutableSamplerState();
	//PSOInitializeToCheck.BoundShaderState.VertexDeclarationRHI = nullptr;
	//PSOInitializeToCheck.UniqueEntry = false;
	
	// Recompute the hash when disabling certain states for checks
	//PSOInitializeToCheck.ComputePrecachePSOHash();

	return PSOInitializeToCheck;
}

void PSOCollectorStats::AddMinimalPipelineStateToCache(const FGraphicsMinimalPipelineStateInitializer& PSOInitialize, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType)
{
	if (!IsPrecachingValidationEnabled())
	{
		return;
	}

	check(VertexFactoryType->SupportsPSOPrecaching());

	static bool bFirstTime = true;
	if (bFirstTime)
	{
		PSOShadersPrecacheStats.Empty();
		MinimalPSOPrecacheStats.Empty();
		bFirstTime = false;
	}

	// Update the Shader only stats ignoring all state 
	{
		FScopeLock Lock(&PSOShadersPrecacheLock);
		FGraphicsMinimalPipelineStateInitializer PSOInitializeToCheck;
		PSOInitializeToCheck.BoundShaderState = PSOInitialize.BoundShaderState;
		PSOInitializeToCheck.BoundShaderState.VertexDeclarationRHI = nullptr;
		PSOInitializeToCheck.ComputePrecachePSOHash();

		Experimental::FHashElementId TableId = PSOShadersPrecacheMap.FindId(PSOInitializeToCheck);
		if (!TableId.IsValid())
		{
			TableId = PSOShadersPrecacheMap.FindOrAddId(PSOInitializeToCheck, FShaderStateUsage());

		}

		FShaderStateUsage& Value = PSOShadersPrecacheMap.GetByElementId(TableId).Value;
		if (!Value.bPrecached)
		{
			check(!Value.bUsed);
			PSOShadersPrecacheStats.PrecacheData.UpdateStats(MeshPassType, VertexFactoryType);
			Value.bPrecached = true;
		}
	}

	// Update the minimal PSO stats - optionally removing certain fields via PatchMinimalPipelineStateToCheck
	{
		FScopeLock Lock(&MinimalPSOPrecacheLock);
		FGraphicsMinimalPipelineStateInitializer PSOInitializeToCheck  = PatchMinimalPipelineStateToCheck(PSOInitialize);
		check(PSOInitializeToCheck.PrecachePSOHash != 0);

		Experimental::FHashElementId TableId = MinimalPSOPrecacheMap.FindId(PSOInitializeToCheck.PrecachePSOHash);
		if (!TableId.IsValid())
		{
			TableId = MinimalPSOPrecacheMap.FindOrAddId(PSOInitializeToCheck.PrecachePSOHash, FShaderStateUsage());

		}

		FShaderStateUsage& Value = MinimalPSOPrecacheMap.GetByElementId(TableId).Value;
		if (!Value.bPrecached)
		{
			check(!Value.bUsed);
			MinimalPSOPrecacheStats.PrecacheData.UpdateStats(MeshPassType, VertexFactoryType);
			Value.bPrecached = true;
		}
	}
}

void PSOCollectorStats::CheckMinimalPipelineStateInCache(const FGraphicsMinimalPipelineStateInitializer& PSOInitialize, uint32 MeshPassType, const FVertexFactoryType* VertexFactoryType)
{
	if (!IsPrecachingValidationEnabled())
	{
		return;
	}

	const EShadingPath ShadingPath = FSceneInterface::GetShadingPath(GMaxRHIFeatureLevel);
	bool bCollectPSOs = FPSOCollectorCreateManager::GetCreateFunction(ShadingPath, MeshPassType) != nullptr;
	bool bTracked = bCollectPSOs && VertexFactoryType->SupportsPSOPrecaching();

	// Enable this when we want to check the actual RHI shader resources during this function - slightly slower and don't need to be retrieved here yet
	//PSOInitialize.BoundShaderState.AsBoundShaderState();
	
	// Update the Shader only stats ignoring all state
	{
		FScopeLock Lock(&PSOShadersPrecacheLock);
		FGraphicsMinimalPipelineStateInitializer PSOInitializeToCheck;
		PSOInitializeToCheck.BoundShaderState = PSOInitialize.BoundShaderState;
		PSOInitializeToCheck.BoundShaderState.VertexDeclarationRHI = nullptr;
		PSOInitializeToCheck.ComputePrecachePSOHash();

		Experimental::FHashElementId TableId = PSOShadersPrecacheMap.FindId(PSOInitializeToCheck);
		if (!TableId.IsValid())
		{
			TableId = PSOShadersPrecacheMap.FindOrAddId(PSOInitializeToCheck, FShaderStateUsage());
		}

		FShaderStateUsage& Value = PSOShadersPrecacheMap.GetByElementId(TableId).Value;
		if (!Value.bUsed)
		{
			PSOShadersPrecacheStats.UsageData.UpdateStats(MeshPassType, VertexFactoryType);
			if (!bTracked)
			{
				PSOShadersPrecacheStats.UntrackedData.UpdateStats(MeshPassType, VertexFactoryType);
			}
			else if (!Value.bPrecached)
			{
				PSOShadersPrecacheStats.MissData.UpdateStats(MeshPassType, VertexFactoryType);
			}
			else
			{
				PSOShadersPrecacheStats.HitData.UpdateStats(MeshPassType, VertexFactoryType);
			}

			Value.bUsed = true;
		}
	}

	// Update the minimal PSO stats - optionally removing certain fields via PatchMinimalPipelineStateToCheck
	{
		FScopeLock Lock(&MinimalPSOPrecacheLock);
		FGraphicsMinimalPipelineStateInitializer PSOInitializeToCheck = PatchMinimalPipelineStateToCheck(PSOInitialize);
		check(PSOInitializeToCheck.PrecachePSOHash != 0);

		Experimental::FHashElementId TableId = MinimalPSOPrecacheMap.FindId(PSOInitializeToCheck.PrecachePSOHash);
		if (!TableId.IsValid())
		{
			TableId = MinimalPSOPrecacheMap.FindOrAddId(PSOInitializeToCheck.PrecachePSOHash, FShaderStateUsage());
		}

		FShaderStateUsage& Value = MinimalPSOPrecacheMap.GetByElementId(TableId).Value;
		if (!Value.bUsed)
		{
			MinimalPSOPrecacheStats.UsageData.UpdateStats(MeshPassType, VertexFactoryType);
			if (!bTracked)
			{
				MinimalPSOPrecacheStats.UntrackedData.UpdateStats(MeshPassType, VertexFactoryType);
			}
			else if (!Value.bPrecached)
			{
				MinimalPSOPrecacheStats.MissData.UpdateStats(MeshPassType, VertexFactoryType);
			}
			else
			{
				MinimalPSOPrecacheStats.HitData.UpdateStats(MeshPassType, VertexFactoryType);
			}

			Value.bUsed = true;
		}
	}
}

#endif // PSO_PRECACHING_VALIDATE

