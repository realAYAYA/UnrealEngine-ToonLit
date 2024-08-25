// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/App.h"
#include "RHICore.h"
#include "ShaderCore.h"
#include "ShaderParameterMacros.h"

struct FRHIShaderBundleDispatch;

namespace UE
{
namespace RHICore
{

/** Validates that the uniform buffer at the requested static slot. */
extern RHICORE_API void ValidateStaticUniformBuffer(FRHIUniformBuffer* UniformBuffer, FUniformBufferStaticSlot Slot, uint32 ExpectedHash);
extern RHICORE_API void SetupShaderCodeValidationData(FRHIShader* RHIShader, class FShaderCodeReader& ShaderCodeReader);
extern RHICORE_API void SetupShaderDiagnosticData(FRHIShader* RHIShader, class FShaderCodeReader& ShaderCodeReader);
extern RHICORE_API void RegisterDiagnosticMessages(const TArray<FShaderDiagnosticData>& In);
extern RHICORE_API const FString* GetDiagnosticMessage(uint32 MessageID);

/** Common implementation of dispatch shader bundle emulation shared by RHIs */
extern RHICORE_API void DispatchShaderBundleEmulation(
	FRHIComputeCommandList& InRHICmdList,
	FRHIShaderBundle* ShaderBundle,
	FRHIBuffer* ArgumentBuffer,
	TConstArrayView<FRHIShaderBundleDispatch> Dispatches
);

inline void InitStaticUniformBufferSlots(TArray<FUniformBufferStaticSlot>& StaticSlots, const FShaderResourceTable& ShaderResourceTable)
{
	StaticSlots.Reserve(ShaderResourceTable.ResourceTableLayoutHashes.Num());

	for (uint32 LayoutHash : ShaderResourceTable.ResourceTableLayoutHashes)
	{
		if (const FShaderParametersMetadata* Metadata = FindUniformBufferStructByLayoutHash(LayoutHash))
		{
			StaticSlots.Add(Metadata->GetLayout().StaticSlot);
		}
		else
		{
			StaticSlots.Add(MAX_UNIFORM_BUFFER_STATIC_SLOTS);
		}
	}
}

template <typename TApplyFunction>
void ApplyStaticUniformBuffers(
	FRHIShader* Shader,
	const TArray<FUniformBufferStaticSlot>& Slots,
	const TArray<uint32>& LayoutHashes,
	const TArray<FRHIUniformBuffer*>& UniformBuffers,
	TApplyFunction&& ApplyFunction
)
{
	checkf(LayoutHashes.Num() == Slots.Num(), TEXT("Shader %s, LayoutHashes %d, Slots %d"),
		Shader->GetShaderName(), LayoutHashes.Num(), Slots.Num());

	for (int32 BufferIndex = 0; BufferIndex < Slots.Num(); ++BufferIndex)
	{
		const FUniformBufferStaticSlot Slot = Slots[BufferIndex];

		if (IsUniformBufferStaticSlotValid(Slot))
		{
			FRHIUniformBuffer* Buffer = UniformBuffers[Slot];
			ValidateStaticUniformBuffer(Buffer, Slot, LayoutHashes[BufferIndex]);

			if (Buffer)
			{
				ApplyFunction(BufferIndex, Buffer);
			}
		}
	}
}

template <typename TRHIContext, typename TRHIShader>
void ApplyStaticUniformBuffers(
	TRHIContext* CommandContext,
	TRHIShader* Shader,
	const TArray<FUniformBufferStaticSlot>& Slots,
	const TArray<uint32>& LayoutHashes,
	const TArray<FRHIUniformBuffer*>& UniformBuffers)
{
	ApplyStaticUniformBuffers(Shader, Slots, LayoutHashes, UniformBuffers,
		[CommandContext, Shader](int32 BufferIndex, FRHIUniformBuffer* Buffer)
		{
			CommandContext->RHISetShaderUniformBuffer(Shader, BufferIndex, Buffer);
		});
}

template <typename TResourceType> struct TResourceTypeStr {};
template<> struct TResourceTypeStr<FRHISamplerState       > { static constexpr TCHAR String[] = TEXT("Sampler State"); };
template<> struct TResourceTypeStr<FRHITexture            > { static constexpr TCHAR String[] = TEXT("Texture"); };
template<> struct TResourceTypeStr<FRHIShaderResourceView > { static constexpr TCHAR String[] = TEXT("Shader Resource View"); };
template<> struct TResourceTypeStr<FRHIUnorderedAccessView> { static constexpr TCHAR String[] = TEXT("Unordered Access View"); };

template <typename TResourceType, typename TCallback>
inline void EnumerateUniformBufferResources(FRHIUniformBuffer* RESTRICT Buffer, int32 BufferIndex, const uint32* RESTRICT ResourceMap, TCallback&& Callback)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->GetResourceTable().GetData();

	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);

			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8  BindIndex     = FRHIResourceTableEntry::GetBindIndex    (ResourceInfo);

			TResourceType* Resource = static_cast<TResourceType*>(Resources[ResourceIndex].GetReference());
			checkf(Resource
				, TEXT("Null %s (resource %d bind %d) on UB Layout %s")
				, TResourceTypeStr<TResourceType>::String
				, ResourceIndex
				, BindIndex
				, *Buffer->GetLayout().GetDebugName()
			);

			Callback(Resource, BindIndex);

			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
}

template <typename TBinder, typename TUniformBufferArrayType, typename TBitMaskType>
void SetResourcesFromTables(TBinder&& Binder, FRHIShader const& Shader, FShaderResourceTable const& SRT, TBitMaskType& DirtyUniformBuffers, TUniformBufferArrayType const& BoundUniformBuffers
#if ENABLE_RHI_VALIDATION
	, RHIValidation::FTracker* Tracker
#endif
)
{
	float CurrentTimeForTextureTimes = FApp::GetCurrentTime();

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = SRT.ResourceTableBits & DirtyUniformBuffers;
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits) & (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::CountTrailingZeros(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;

		check(BufferIndex < SRT.ResourceTableLayoutHashes.Num());

		FRHIUniformBuffer* Buffer = BoundUniformBuffers[BufferIndex];

#if DO_CHECK

		if (!Buffer)
		{
			UE_LOG(LogRHICore, Fatal, TEXT("Shader expected a uniform buffer at slot %u but got null instead (Shader='%s' UB='%s'). Rendering code needs to set a valid uniform buffer for this slot.")
				, BufferIndex
				, Shader.GetShaderName()
				, *Shader.GetUniformBufferName(BufferIndex)
			);
		}
		else if (Buffer->GetLayout().GetHash() != SRT.ResourceTableLayoutHashes[BufferIndex])
		{
			FRHIUniformBufferLayout const& BufferLayout = Buffer->GetLayout();

			FString ResourcesString;
			for (FRHIUniformBufferResource const& Resource : BufferLayout.Resources)
			{
				ResourcesString += FString::Printf(TEXT("%s%d")
					, ResourcesString.Len() ? TEXT(" ") : TEXT("")
					, Resource.MemberType
				);
			}

			// This might mean you are accessing a data you haven't bound e.g. GBuffer
			UE_LOG(LogRHICore, Fatal,
				TEXT("Uniform buffer bound to slot %u is not what the shader expected:\n")
				TEXT("\tBound                : Uniform Buffer[%s] with Hash[0x%08x]\n")
				TEXT("\tExpected             : Uniform Buffer[%s] with Hash[0x%08x]\n")
				TEXT("\tShader Name          : %s\n")
				TEXT("\tLayout CB Size       : %d\n")
				TEXT("\tLayout Num Resources : %d\n")
				TEXT("\tResource Types       : %s\n")
				, BufferIndex
				, *BufferLayout.GetDebugName(), BufferLayout.GetHash()
				, *Shader.GetUniformBufferName(BufferIndex), SRT.ResourceTableLayoutHashes[BufferIndex]
				, Shader.GetShaderName()
				, BufferLayout.ConstantBufferSize
				, BufferLayout.Resources.Num()
				, *ResourcesString
			);
		}

#endif // DO_CHECK

		// Textures
		EnumerateUniformBufferResources<FRHITexture>(Buffer, BufferIndex, SRT.TextureMap.GetData(),
			[&](FRHITexture* Texture, uint8 Index)
			{
#if ENABLE_RHI_VALIDATION
				if (Tracker)
				{
					ERHIAccess Access = IsComputeShaderFrequency(Shader.GetFrequency())
						? ERHIAccess::SRVCompute
						: ERHIAccess::SRVGraphics;

					// Textures bound here only have their "common" plane accessible. Stencil etc is ignored.
					// (i.e. only access the color plane of a color texture, or depth plane of a depth texture)
					Tracker->Assert(Texture->GetViewIdentity(0, 0, 0, 0, uint32(RHIValidation::EResourcePlane::Common), 1), Access);
				}
#endif
				Texture->SetLastRenderTime(CurrentTimeForTextureTimes);
				Binder.SetTexture(Texture, Index);
			});

		// SRVs
		EnumerateUniformBufferResources<FRHIShaderResourceView>(Buffer, BufferIndex, SRT.ShaderResourceViewMap.GetData(),
			[&](FRHIShaderResourceView* SRV, uint8 Index)
			{
#if ENABLE_RHI_VALIDATION
				if (Tracker)
				{
					ERHIAccess Access = IsComputeShaderFrequency(Shader.GetFrequency())
						? ERHIAccess::SRVCompute
						: ERHIAccess::SRVGraphics;

					Tracker->Assert(SRV->GetViewIdentity(), Access);
				}
				if (GRHIValidationEnabled)
				{
					RHIValidation::ValidateShaderResourceView(&Shader, Index, SRV);
				}
#endif
				Binder.SetSRV(SRV, Index);
			});

		// Samplers
		EnumerateUniformBufferResources<FRHISamplerState>(Buffer, BufferIndex, SRT.SamplerMap.GetData(),
			[&](FRHISamplerState* Sampler, uint8 Index)
			{
				Binder.SetSampler(Sampler, Index);
			});

		// UAVs
		EnumerateUniformBufferResources<FRHIUnorderedAccessView>(Buffer, BufferIndex, SRT.UnorderedAccessViewMap.GetData(),
			[&](FRHIUnorderedAccessView* UAV, uint8 Index)
			{
#if ENABLE_RHI_VALIDATION
				if (Tracker)
				{
					ERHIAccess Access = IsComputeShaderFrequency(Shader.GetFrequency())
						? ERHIAccess::UAVCompute
						: ERHIAccess::UAVGraphics;

					Tracker->AssertUAV(UAV, Access, Index);
				}
#endif
				Binder.SetUAV(UAV, Index);
			});
	}

	DirtyUniformBuffers = TBitMaskType(0);
}

} //! RHICore
} //! UE
