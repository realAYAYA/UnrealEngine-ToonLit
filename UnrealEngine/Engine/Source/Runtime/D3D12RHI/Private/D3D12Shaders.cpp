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
	OutShader.ShaderName = InShaderCode.FindOptionalData(FShaderCodeName::Key);

	int32 UniformBufferTableSize = 0;
	const uint8* UniformBufferData = InShaderCode.FindOptionalDataAndSize(FShaderCodeUniformBuffers::Key, UniformBufferTableSize);
	if (UniformBufferData && UniformBufferTableSize > 0)
	{
		FBufferReader UBReader((void*)UniformBufferData, UniformBufferTableSize, false);
		TArray<FString> Names;
		UBReader << Names;
		check(OutShader.UniformBuffers.Num() == 0);
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			OutShader.UniformBuffers.Add(FName(*Names[Index]));
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

	return true;
}

static bool ValidateShaderIsUsable(FD3D12ShaderData* InShader, EShaderFrequency InFrequency)
{
#if D3D12RHI_NEEDS_SHADER_FEATURE_CHECKS
	if ((InFrequency == SF_Mesh || InFrequency == SF_Amplification) && !GRHISupportsMeshShadersTier0)
	{
		return false;
	}

	if (EnumHasAnyFlags(InShader->Features, EShaderCodeFeatures::WaveOps) && !GRHISupportsWaveOperations)
	{
		return false;
	}

	if (EnumHasAnyFlags(InShader->Features, EShaderCodeFeatures::BindlessResources | EShaderCodeFeatures::BindlessSamplers) && !GRHISupportsBindless)
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
		delete Shader;
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
		FD3D12Adapter& Adapter = GetAdapter();

#if USE_STATIC_ROOT_SIGNATURE
		Shader->RootSignature = Adapter.GetStaticComputeRootSignature();
#else
		const FD3D12QuantizedBoundShaderState QBSS = QuantizeBoundComputeShaderState(Adapter, Shader);
		Shader->RootSignature = Adapter.GetRootSignature(QBSS);
#endif
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
		delete Shader;
		return nullptr;
	}

	UE::RHICore::InitStaticUniformBufferSlots(Shader->StaticSlots, Shader->ShaderResourceTable);

	FD3D12Adapter& Adapter = GetAdapter();

#if USE_STATIC_ROOT_SIGNATURE
	switch (ShaderFrequency)
	{
	case SF_RayGen:
		Shader->pRootSignature = Adapter.GetStaticRayTracingGlobalRootSignature();
		break;
	case SF_RayHitGroup:
	case SF_RayCallable:
	case SF_RayMiss:
		Shader->pRootSignature = Adapter.GetStaticRayTracingLocalRootSignature();
		break;
	default:
		checkNoEntry(); // Unexpected shader target frequency
	}
#else // USE_STATIC_ROOT_SIGNATURE
	const FD3D12QuantizedBoundShaderState QBSS = QuantizeBoundRayTracingShaderState(Adapter, ShaderFrequency, Shader);
	Shader->pRootSignature = Adapter.GetRootSignature(QBSS);
#endif // USE_STATIC_ROOT_SIGNATURE

	return Shader;
}

#endif // D3D12_RHI_RAYTRACING

void FD3D12CommandContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{
	// Structures are chosen to be directly mappable
	StateCache.SetViewports(Count, reinterpret_cast<const D3D12_VIEWPORT*>(Data));
}

FD3D12BoundShaderState::FD3D12BoundShaderState(
	FRHIVertexDeclaration* InVertexDeclarationRHI,
	FRHIVertexShader* InVertexShaderRHI,
	FRHIPixelShader* InPixelShaderRHI,
	FRHIGeometryShader* InGeometryShaderRHI,
	FD3D12Adapter* InAdapter
	) :
	CacheLink(InVertexDeclarationRHI, InVertexShaderRHI, InPixelShaderRHI, InGeometryShaderRHI, this)
{
	INC_DWORD_STAT(STAT_D3D12NumBoundShaderState);

#if USE_STATIC_ROOT_SIGNATURE
	pRootSignature = InAdapter->GetStaticGraphicsRootSignature();
#else
	const FD3D12QuantizedBoundShaderState QuantizedBoundShaderState = QuantizeBoundGraphicsShaderState(*InAdapter, this);
	pRootSignature = InAdapter->GetRootSignature(QuantizedBoundShaderState);
#endif

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	CacheLink.AddToCache();
#endif
}

#if PLATFORM_SUPPORTS_MESH_SHADERS
FD3D12BoundShaderState::FD3D12BoundShaderState(
	FRHIMeshShader* InMeshShaderRHI,
	FRHIAmplificationShader* InAmplificationShaderRHI,
	FRHIPixelShader* InPixelShaderRHI,
	FD3D12Adapter* InAdapter
) :
	CacheLink(InMeshShaderRHI, InAmplificationShaderRHI, InPixelShaderRHI, this)
{
	INC_DWORD_STAT(STAT_D3D12NumBoundShaderState);

#if USE_STATIC_ROOT_SIGNATURE
	pRootSignature = InAdapter->GetStaticGraphicsRootSignature();
#else
	const FD3D12QuantizedBoundShaderState QuantizedBoundShaderState = QuantizeBoundGraphicsShaderState(*InAdapter, this);
	pRootSignature = InAdapter->GetRootSignature(QuantizedBoundShaderState);
#endif

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	CacheLink.AddToCache();
#endif
}
#endif // PLATFORM_SUPPORTS_MESH_SHADERS

FD3D12BoundShaderState::~FD3D12BoundShaderState()
{
	DEC_DWORD_STAT(STAT_D3D12NumBoundShaderState);
#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	CacheLink.RemoveFromCache();
#endif
}

FBoundShaderStateRHIRef FD3D12DynamicRHI::DX12CreateBoundShaderState(const FBoundShaderStateInput& BoundShaderStateInput)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CreateBoundShaderStateTime);

	checkf(GIsRHIInitialized, (TEXT("Bound shader state RHI resource was created without initializing Direct3D first")));

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	// Check for an existing bound shader state which matches the parameters
	FBoundShaderStateRHIRef CachedBoundShaderState = GetCachedBoundShaderState_Threadsafe(
		BoundShaderStateInput.VertexDeclarationRHI,
		BoundShaderStateInput.VertexShaderRHI,
		BoundShaderStateInput.PixelShaderRHI,
		BoundShaderStateInput.GetGeometryShader(),
		BoundShaderStateInput.GetMeshShader(),
		BoundShaderStateInput.GetAmplificationShader()
	);
	if (CachedBoundShaderState.GetReference())
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderState;
	}
#else
	check(IsInRenderingThread() || IsInRHIThread());
	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		BoundShaderStateInput.VertexDeclarationRHI,
		BoundShaderStateInput.VertexShaderRHI,
		BoundShaderStateInput.PixelShaderRHI,
		BoundShaderStateInput.GetGeometryShader(),
		BoundShaderStateInput.GetMeshShader(),
		BoundShaderStateInput.GetAmplificationShader()
	);
	if (CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
#endif
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_D3D12NewBoundShaderStateTime);

#if PLATFORM_SUPPORTS_MESH_SHADERS
		if (BoundShaderStateInput.GetMeshShader())
		{
			return new FD3D12BoundShaderState(
				BoundShaderStateInput.GetMeshShader(),
				BoundShaderStateInput.GetAmplificationShader(),
				BoundShaderStateInput.PixelShaderRHI,
				&GetAdapter());
		}
		else
#endif // PLATFORM_SUPPORTS_MESH_SHADERS
		{
			return new FD3D12BoundShaderState(
				BoundShaderStateInput.VertexDeclarationRHI,
				BoundShaderStateInput.VertexShaderRHI,
				BoundShaderStateInput.PixelShaderRHI,
				BoundShaderStateInput.GetGeometryShader(),
				&GetAdapter());
		}
	}
}

/**
* Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
* @param VertexDeclaration - existing vertex decl
* @param StreamStrides - optional stream strides
* @param VertexShader - existing vertex shader
* @param PixelShader - existing pixel shader
* @param GeometryShader - existing geometry shader
*/
FBoundShaderStateRHIRef FD3D12DynamicRHI::RHICreateBoundShaderState(
	FRHIVertexDeclaration* VertexDeclarationRHI,
	FRHIVertexShader* VertexShaderRHI,
	FRHIPixelShader* PixelShaderRHI,
	FRHIGeometryShader* GeometryShaderRHI
	)
{
	FBoundShaderStateInput Inputs(VertexDeclarationRHI, VertexShaderRHI, PixelShaderRHI
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		, GeometryShaderRHI
#endif
	);
	return DX12CreateBoundShaderState(Inputs);
}
