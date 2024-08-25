// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Shaders.cpp: D3D shader RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RHICoreShader.h"

template <typename TShaderType>
static inline bool ReadShaderOptionalData(FShaderCodeReader& InShaderCode, TShaderType& OutShader)
{
	const FShaderCodePackedResourceCounts* PackedResourceCounts = InShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	if (!PackedResourceCounts)
	{
		return false;
	}
	OutShader.ResourceCounts = *PackedResourceCounts;

#if RHI_INCLUDE_SHADER_DEBUG_DATA
	OutShader.Debug.ShaderName = InShaderCode.FindOptionalData(FShaderCodeName::Key);

	int32 UniformBufferTableSize = 0;
	const uint8* UniformBufferData = InShaderCode.FindOptionalDataAndSize(FShaderCodeUniformBuffers::Key, UniformBufferTableSize);
	if (UniformBufferData && UniformBufferTableSize > 0)
	{
		FBufferReader UBReader((void*)UniformBufferData, UniformBufferTableSize, false);
		TArray<FString> Names;
		UBReader << Names;
		check(OutShader.Debug.UniformBufferNames.Num() == 0);
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			OutShader.Debug.UniformBufferNames.Add(FName(*Names[Index]));
		}
	}
#endif

#if D3D12RHI_NEEDS_VENDOR_EXTENSIONS
	int32 VendorExtensionTableSize = 0;
	auto* VendorExtensionData = InShaderCode.FindOptionalDataAndSize(FShaderCodeVendorExtension::Key, VendorExtensionTableSize);
	if (VendorExtensionData && VendorExtensionTableSize > 0)
	{
		FBufferReader Ar((void*)VendorExtensionData, VendorExtensionTableSize, false);
		Ar << OutShader.VendorExtensions;
	}
#endif

#if D3D12RHI_NEEDS_SHADER_FEATURE_CHECKS
	if (const FShaderCodeFeatures* CodeFeatures = InShaderCode.FindOptionalData<FShaderCodeFeatures>())
	{
		OutShader.Features = CodeFeatures->CodeFeatures;
	}
#endif

	UE::RHICore::SetupShaderCodeValidationData(&OutShader, InShaderCode);
	UE::RHICore::SetupShaderDiagnosticData(&OutShader, InShaderCode);

	return true;
}

static bool ValidateShaderIsUsable(FD3D12ShaderData* InShader, EShaderFrequency InFrequency)
{
#if D3D12RHI_NEEDS_SHADER_FEATURE_CHECKS
	if ((InFrequency == SF_Mesh || InFrequency == SF_Amplification) && !GRHISupportsMeshShadersTier0)
	{
		return false;
	}

	// When we're using the SM5 shader library plus raytracing, GRHISupportsWaveOperations is false because DXBC shaders can't do wave ops,
	// but RT shaders are compiled to DXIL and can use wave ops (HW support is guaranteed too).
	if (EnumHasAnyFlags(InShader->Features, EShaderCodeFeatures::WaveOps) && !GRHISupportsWaveOperations && !IsRayTracingShaderFrequency(InFrequency))
	{
		return false;
	}

	if (EnumHasAnyFlags(InShader->Features, EShaderCodeFeatures::BindlessResources | EShaderCodeFeatures::BindlessSamplers))
	{
		if (GRHIBindlessSupport == ERHIBindlessSupport::Unsupported ||
			(GRHIBindlessSupport == ERHIBindlessSupport::RayTracingOnly && !IsRayTracingShaderFrequency(InFrequency)))
		{
			return false;
		}
	}

	if (InFrequency == SF_Pixel && EnumHasAnyFlags(InShader->Features, EShaderCodeFeatures::StencilRef) && !GRHISupportsStencilRefFromPixelShader)
	{
		return false;
	}

	if (EnumHasAnyFlags(InShader->Features, EShaderCodeFeatures::BarycentricsSemantic) && !GRHIGlobals.SupportsBarycentricsSemantic)
	{
		return false;
	}
#endif

	return true;
}

template <typename TShaderType>
bool InitShaderCommon(FShaderCodeReader& ShaderCode, int32 Offset, TShaderType* InShader)
{
	if (!ReadShaderOptionalData(ShaderCode, *InShader))
	{
		return false;
	}

	if (!ValidateShaderIsUsable(InShader, InShader->GetFrequency()))
	{
		return false;
	}

	// Copy the native shader data only, skipping any of our own headers.
	InShader->Code = ShaderCode.GetOffsetShaderCode(Offset);
	InShader->SetShaderBundleUsage(EnumHasAnyFlags(InShader->ResourceCounts.UsageFlags, EShaderResourceUsageFlags::ShaderBundle));

	return true;
}

template <typename TShaderType>
TShaderType* CreateStandardShader(TArrayView<const uint8> InCode)
{
	FShaderCodeReader ShaderCode(InCode);
	TShaderType* Shader = new TShaderType();

	FMemoryReaderView Ar(InCode, true);
	Ar << Shader->ShaderResourceTable;

	const int32 Offset = Ar.Tell();

	if (!InitShaderCommon(ShaderCode, Offset, Shader))
	{
		Shader->AddRef();
		Shader->Release();
		return nullptr;
	}

	UE::RHICore::InitStaticUniformBufferSlots(Shader->StaticSlots, Shader->ShaderResourceTable);
	return Shader;
}

FVertexShaderRHIRef FD3D12DynamicRHI::RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateStandardShader<FD3D12VertexShader>(Code);
}

FMeshShaderRHIRef FD3D12DynamicRHI::RHICreateMeshShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateStandardShader<FD3D12MeshShader>(Code);
}

FAmplificationShaderRHIRef FD3D12DynamicRHI::RHICreateAmplificationShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateStandardShader<FD3D12AmplificationShader>(Code);
}

FPixelShaderRHIRef FD3D12DynamicRHI::RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateStandardShader<FD3D12PixelShader>(Code);
}

FGeometryShaderRHIRef FD3D12DynamicRHI::RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return CreateStandardShader<FD3D12GeometryShader>(Code);
}

FComputeShaderRHIRef FD3D12DynamicRHI::RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	FD3D12ComputeShader* Shader = CreateStandardShader<FD3D12ComputeShader>(Code);
	if (Shader)
	{
		Shader->RootSignature = GetAdapter().GetRootSignature(Shader);
		Shader->SetNoDerivativeOps(EnumHasAnyFlags(Shader->ResourceCounts.UsageFlags, EShaderResourceUsageFlags::NoDerivativeOps));
	}

	return Shader;
}

#if D3D12_RHI_RAYTRACING

FRayTracingShaderRHIRef FD3D12DynamicRHI::RHICreateRayTracingShader(TArrayView<const uint8> Code, const FSHAHash& Hash, EShaderFrequency ShaderFrequency)
{
	checkf(GRHISupportsRayTracing && GRHISupportsRayTracingShaders, TEXT("Tried to create RayTracing shader but RHI doesn't support it!"));

	FShaderCodeReader ShaderCode(Code);
	FD3D12RayTracingShader* Shader = new FD3D12RayTracingShader(ShaderFrequency);

	FMemoryReaderView Ar(Code, true);
	Ar << Shader->ShaderResourceTable;
	Ar << Shader->EntryPoint;
	Ar << Shader->AnyHitEntryPoint;
	Ar << Shader->IntersectionEntryPoint;
	Ar << Shader->RayTracingPayloadType;
	Ar << Shader->RayTracingPayloadSize;

	checkf(Shader->RayTracingPayloadType != 0, TEXT("Ray Tracing Shader must not have an empty payload type!"));
	checkf(	(FMath::CountBits(Shader->RayTracingPayloadType) == 1 && (ShaderFrequency == SF_RayHitGroup || ShaderFrequency == SF_RayMiss || ShaderFrequency == SF_RayCallable)) ||
			(FMath::CountBits(Shader->RayTracingPayloadType) >= 1 && (ShaderFrequency == SF_RayGen)),
			TEXT("Ray Tracing Shader has %d bits set, which is not the expected count for shader frequency %d"), FMath::CountBits(Shader->RayTracingPayloadType), int(ShaderFrequency)
	);

	int32 Offset = Ar.Tell();

	int32 PrecompiledKey = 0;
	Ar << PrecompiledKey;
	if (PrecompiledKey == RayTracingPrecompiledPSOKey)
	{
		Offset += sizeof(PrecompiledKey); // Skip the precompiled PSO marker if it's present
		Shader->bPrecompiledPSO = true;
	}

	if (!InitShaderCommon(ShaderCode, Offset, Shader))
	{
		// We can't just call delete on the shader since it's an FRHIResource, so it must use the deletion queue mechanism.
		// However, since it starts with refcount 0, we must first AddRef it in order to be able to call Release.
		Shader->AddRef();
		Shader->Release();
		return nullptr;
	}

	UE::RHICore::InitStaticUniformBufferSlots(Shader->StaticSlots, Shader->ShaderResourceTable);

	Shader->pRootSignature = GetAdapter().GetRootSignature(Shader);

	return Shader;
}

#endif // D3D12_RHI_RAYTRACING

FShaderBundleRHIRef FD3D12DynamicRHI::RHICreateShaderBundle(uint32 NumRecords)
{
	FD3D12ShaderBundle* ShaderBundle = new FD3D12ShaderBundle(GetRHIDevice(0), NumRecords);
	return ShaderBundle;
}

void FD3D12CommandContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{
	// Structures are chosen to be directly mappable
	StateCache.SetViewports(Count, reinterpret_cast<const D3D12_VIEWPORT*>(Data));
}

FBoundShaderStateRHIRef FD3D12DynamicRHI::RHICreateBoundShaderState(
	FRHIVertexDeclaration* VertexDeclarationRHI,
	FRHIVertexShader* VertexShaderRHI,
	FRHIPixelShader* PixelShaderRHI,
	FRHIGeometryShader* GeometryShaderRHI
	)
{
	checkNoEntry();
	return nullptr;
}
