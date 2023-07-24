// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHICoreShader.h"

#define RHI_VALIDATE_STATIC_UNIFORM_BUFFERS (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

namespace UE
{
namespace RHICore
{

void ValidateStaticUniformBuffer(FRHIUniformBuffer* UniformBuffer, FUniformBufferStaticSlot Slot, uint32 ExpectedHash)
{
#if RHI_VALIDATE_STATIC_UNIFORM_BUFFERS
	FUniformBufferStaticSlotRegistry& SlotRegistry = FUniformBufferStaticSlotRegistry::Get();

	if (!UniformBuffer)
	{
		const FShaderParametersMetadata* ExpectedStructMetadata = FindUniformBufferStructByLayoutHash(ExpectedHash);

		checkf(
			ExpectedStructMetadata,
			TEXT("Shader is requesting a uniform buffer at slot %s with hash '%u', but a reverse lookup of the hash can't find it. The shader cache may be out of date."),
			*SlotRegistry.GetDebugDescription(Slot), ExpectedHash);

		const EUniformBufferBindingFlags BindingFlags = ExpectedStructMetadata->GetBindingFlags();

		checkf(EnumHasAnyFlags(BindingFlags, EUniformBufferBindingFlags::Static),
			TEXT("Shader requested a global uniform buffer of type '%s' at static slot '%s', but it is not registered with the Global binding flag. The shader cache may be out of date."),
			ExpectedStructMetadata->GetShaderVariableName(), *SlotRegistry.GetDebugDescription(Slot));

		// Structs can be bound both globally or per-shader, effectively leaving it up to the user to choose which to bind.
		// But that also means we can't validate existence at the global level.
		if (!EnumHasAnyFlags(BindingFlags, EUniformBufferBindingFlags::Shader))
		{
			UE_LOG(LogRHICore, Fatal,
				TEXT("Shader requested a global uniform buffer of type '%s' at static slot '%s', but it was null. The uniform buffer should ")
				TEXT("be bound using RHICmdList.SetStaticUniformBuffers() or passed into an RDG pass using SHADER_PARAMETER_STRUCT_REF() or ")
				TEXT("SHADER_PARAMETER_RDG_UNIFORM_BUFFER()."),
				ExpectedStructMetadata->GetShaderVariableName(), *SlotRegistry.GetDebugDescription(Slot));
		}
	}
	else
	{
		const FRHIUniformBufferLayout& Layout = UniformBuffer->GetLayout();

		if (Layout.GetHash() != ExpectedHash)
		{
			const FShaderParametersMetadata* ExpectedStructMetadata = FindUniformBufferStructByLayoutHash(ExpectedHash);

			checkf(
				ExpectedStructMetadata,
				TEXT("Shader is requesting uniform buffer '%s' at slot %s with hash '%u', but a reverse lookup of the hash can't find it. The shader cache may be out of date."),
				*Layout.GetDebugName(), *SlotRegistry.GetDebugDescription(Slot), ExpectedHash);

			checkf(
				false,
				TEXT("Shader attempted to bind uniform buffer '%s' at slot %s with hash '%u', but the shader expected '%s' with hash '%u'."),
				*Layout.GetDebugName(), *SlotRegistry.GetDebugDescription(Slot), ExpectedHash, ExpectedStructMetadata->GetShaderVariableName(), Layout.GetHash());
		}
	}
#endif
}

} //! RHICore
} //! UE