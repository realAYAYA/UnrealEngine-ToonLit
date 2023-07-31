// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHICore.h"
#include "ShaderCore.h"

namespace UE
{
namespace RHICore
{

/** Validates that the uniform buffer at the requested static slot. */
extern RHICORE_API void ValidateStaticUniformBuffer(FRHIUniformBuffer* UniformBuffer, FUniformBufferStaticSlot Slot, uint32 ExpectedHash);

inline void InitStaticUniformBufferSlots(TArray<FUniformBufferStaticSlot>& StaticSlots, const FBaseShaderResourceTable& ShaderResourceTable)
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

template <typename TRHIContext, typename TRHIShader>
void ApplyStaticUniformBuffers(
	TRHIContext* CommandContext,
	TRHIShader* Shader,
	const TArray<FUniformBufferStaticSlot>& Slots,
	const TArray<uint32>& LayoutHashes,
	const TArray<FRHIUniformBuffer*>& UniformBuffers)
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
				CommandContext->RHISetShaderUniformBuffer(Shader, BufferIndex, Buffer);
			}
		}
	}
}

} //! RHICore
} //! UE