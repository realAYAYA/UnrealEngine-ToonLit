// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHICoreShader.h"
#include "PipelineStateCache.h"

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

void SetupShaderCodeValidationData(FRHIShader* RHIShader, FShaderCodeReader& ShaderCodeReader)
{
#if RHI_INCLUDE_SHADER_DEBUG_DATA && ENABLE_RHI_VALIDATION
	if (GRHIValidationEnabled && RHIShader)
	{
		int32 ShaderCodeValidationExtensionSize = 0;
		const uint8* ShaderCodeValidationExtensionData = ShaderCodeReader.FindOptionalDataAndSize(FShaderCodeValidationExtension::Key, ShaderCodeValidationExtensionSize);
		if (ShaderCodeValidationExtensionData && ShaderCodeValidationExtensionSize > 0)
		{
			FBufferReader ArValidationExtensionData((void*)ShaderCodeValidationExtensionData, ShaderCodeValidationExtensionSize, false);
			FShaderCodeValidationExtension ShaderCodeValidationExtension;
			ArValidationExtensionData << ShaderCodeValidationExtension;
			RHIShader->DebugStrideValidationData.Append(ShaderCodeValidationExtension.ShaderCodeValidationStride);
			RHIShader->DebugSRVTypeValidationData.Append(ShaderCodeValidationExtension.ShaderCodeValidationSRVType);
			RHIShader->DebugUAVTypeValidationData.Append(ShaderCodeValidationExtension.ShaderCodeValidationUAVType);
			RHIShader->DebugUBSizeValidationData.Append(ShaderCodeValidationExtension.ShaderCodeValidationUBSize);
		}
	}
#endif
}

void DispatchShaderBundleEmulation(
	FRHIComputeCommandList& InRHICmdList,
	FRHIShaderBundle* ShaderBundle,
	FRHIBuffer* ArgumentBuffer,
	TConstArrayView<FRHIShaderBundleDispatch> Dispatches)
{
	for (const FRHIShaderBundleDispatch& Dispatch : Dispatches)
	{
		if (Dispatch.Shader == nullptr)
		{
			continue;
		}

		checkf(Dispatch.Shader->HasShaderBundleUsage(), TEXT("All shaders in a bundle must specify CFLAG_ShaderBundle"));

		SetComputePipelineState(InRHICmdList, Dispatch.Shader);

		if (Dispatch.Parameters.HasParameters())
		{
			InRHICmdList.SetShaderParameters(
				Dispatch.Shader,
				Dispatch.Parameters.ParametersData,
				Dispatch.Parameters.Parameters,
				Dispatch.Parameters.ResourceParameters,
				Dispatch.Parameters.BindlessParameters
			);
		}

		if (GRHISupportsShaderRootConstants)
		{
			InRHICmdList.SetShaderRootConstants(Dispatch.Constants);
		}

		const uint32 IndirectOffset = (Dispatch.RecordIndex * FRHIShaderBundle::ArgumentByteStride);
		InRHICmdList.DispatchIndirectComputeShader(ArgumentBuffer, IndirectOffset);
	}
}

const bool GRHIShaderDiagnosticEnabled = true;
void SetupShaderDiagnosticData(FRHIShader* RHIShader, FShaderCodeReader& ShaderCodeReader)
{
	if (RHIShader && GRHIShaderDiagnosticEnabled)
	{
		int32 ShaderDiagnosticExtensionSize = 0;
		const uint8* ShaderDiagnosticExtensionData = ShaderCodeReader.FindOptionalDataAndSize(FShaderDiagnosticExtension::Key, ShaderDiagnosticExtensionSize);
		if (ShaderDiagnosticExtensionData && ShaderDiagnosticExtensionSize > 0)
		{
			FBufferReader ArValidationExtensionData((void*)ShaderDiagnosticExtensionData, ShaderDiagnosticExtensionSize, false);
			FShaderDiagnosticExtension ShaderDiagnosticExtension;
			ArValidationExtensionData << ShaderDiagnosticExtension;
			RegisterDiagnosticMessages(ShaderDiagnosticExtension.ShaderDiagnosticDatas);
		}
	}
}

TArray<FShaderDiagnosticData> GShaderDiagnosticDatas;
void RegisterDiagnosticMessages(const TArray<FShaderDiagnosticData>& In)
{
	// Not thread safe
	GShaderDiagnosticDatas.Append(In);
}

const FString* GetDiagnosticMessage(uint32 MessageID)
{
	// Not thread safe
	if (const FShaderDiagnosticData* Found = GShaderDiagnosticDatas.FindByPredicate([MessageID](const FShaderDiagnosticData& In) { return In.Hash == MessageID; }))
	{
		return &Found->Message;
	}
	return nullptr;
}

} //! RHICore
} //! UE