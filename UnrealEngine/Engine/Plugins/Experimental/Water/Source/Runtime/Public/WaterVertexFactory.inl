// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "MeshMaterialShader.h"
#include "WaterInstanceDataBuffer.h"


// ----------------------------------------------------------------------------------

template <bool bWithWaterSelectionSupport>
TWaterVertexFactory<bWithWaterSelectionSupport>::TWaterVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, int32 InNumQuadsPerSide, float InLODScale)
	: FVertexFactory(InFeatureLevel)
	, NumQuadsPerSide(InNumQuadsPerSide)
	, LODScale(InLODScale)
{
	VertexBuffer = new FWaterMeshVertexBuffer(NumQuadsPerSide);
	IndexBuffer = new FWaterMeshIndexBuffer(NumQuadsPerSide);
}

template <bool bWithWaterSelectionSupport>
TWaterVertexFactory<bWithWaterSelectionSupport>::~TWaterVertexFactory()
{
	delete VertexBuffer;
	delete IndexBuffer;
}

template <bool bWithWaterSelectionSupport>
void TWaterVertexFactory<bWithWaterSelectionSupport>::InitRHI()
{
	Super::InitRHI();

	// Setup the uniform data:
	SetupUniformDataForGroup(EWaterMeshRenderGroupType::RG_RenderWaterTiles);

#if WITH_WATER_SELECTION_SUPPORT
	SetupUniformDataForGroup(EWaterMeshRenderGroupType::RG_RenderSelectedWaterTilesOnly);
	SetupUniformDataForGroup(EWaterMeshRenderGroupType::RG_RenderUnselectedWaterTilesOnly);
#endif // WITH_WATER_SELECTION_SUPPORT

	VertexBuffer->InitResource();
	IndexBuffer->InitResource();

	// No streams should currently exist.
	check(Streams.Num() == 0);

	// Position stream for per vertex local position 
	FVertexStream PositionVertexStream;
	PositionVertexStream.VertexBuffer = VertexBuffer;
	PositionVertexStream.Stride = sizeof(FVector4f);
	PositionVertexStream.Offset = 0;
	PositionVertexStream.VertexStreamUsage = EVertexStreamUsage::Default;

	// Simple instancing vertex stream with nullptr vertex buffer to be set at binding time
	FVertexStream InstanceDataVertexStream;
	InstanceDataVertexStream.VertexBuffer = nullptr;
	InstanceDataVertexStream.Stride = sizeof(FVector4f);
	InstanceDataVertexStream.Offset = 0;
	InstanceDataVertexStream.VertexStreamUsage = EVertexStreamUsage::Instancing;

	FVertexElement VertexPositionElement(Streams.Add(PositionVertexStream), 0, VET_Float4, 0, PositionVertexStream.Stride, false);

	// Vertex declaration
	FVertexDeclarationElementList Elements;
	Elements.Add(VertexPositionElement);

	// Adds all streams
	for (int32 StreamIdx = 0; StreamIdx < NumAdditionalVertexStreams; ++StreamIdx)
	{
		FVertexElement InstanceElement(Streams.Add(InstanceDataVertexStream), 0, VET_Float4, 8 + StreamIdx, InstanceDataVertexStream.Stride, true);
		Elements.Add(InstanceElement);
	}

#if 0
	const bool bCanUseGPUScene = UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel);

	// Add support for primitiveID
	const uint8 Index = static_cast<uint8>(EVertexInputStreamType::Default);
	PrimitiveIdStreamIndex[Index] = -1;
	if (GetType()->SupportsPrimitiveIdStream() && bCanUseGPUScene)
	{
		// When the VF is used for rendering in normal mesh passes, this vertex buffer and offset will be overridden
		Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummy, 0, 0, sizeof(uint32), VET_UInt, EVertexStreamUsage::Instancing), 13));
		PrimitiveIdStreamIndex[Index] = Elements.Last().StreamIndex;
	}
#endif

	InitDeclaration(Elements);
}

template <bool bWithWaterSelectionSupport>
void TWaterVertexFactory<bWithWaterSelectionSupport>::ReleaseRHI()
{
	for (auto& UniformBuffer : UniformBuffers)
	{
		UniformBuffer.SafeRelease();
	}

	if (VertexBuffer)
	{
		VertexBuffer->ReleaseResource();
	}

	if (IndexBuffer)
	{
		IndexBuffer->ReleaseResource();
	}

	Super::ReleaseRHI();
}

template <bool bWithWaterSelectionSupport>
void TWaterVertexFactory<bWithWaterSelectionSupport>::SetupUniformDataForGroup(EWaterMeshRenderGroupType InRenderGroupType)
{
	FWaterVertexFactoryParameters UniformParams;
	UniformParams.NumQuadsPerTileSide = NumQuadsPerSide;
	UniformParams.LODScale = LODScale;
	UniformParams.bRenderSelected = true;
	UniformParams.bRenderUnselected = true;

#if WITH_WATER_SELECTION_SUPPORT
	UniformParams.bRenderSelected = (InRenderGroupType != EWaterMeshRenderGroupType::RG_RenderUnselectedWaterTilesOnly);
	UniformParams.bRenderUnselected = (InRenderGroupType != EWaterMeshRenderGroupType::RG_RenderSelectedWaterTilesOnly);
#endif // WITH_WATER_SELECTION_SUPPORT

	UniformBuffers[(int32)InRenderGroupType] = FWaterVertexFactoryBufferRef::CreateUniformBufferImmediate(UniformParams, UniformBuffer_MultiFrame);
}

template <bool bWithWaterSelectionSupport>
bool TWaterVertexFactory<bWithWaterSelectionSupport>::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	const bool bIsCompatibleWithWater = ((Parameters.MaterialParameters.MaterialDomain == MD_Surface) && Parameters.MaterialParameters.bIsUsedWithWater) || Parameters.MaterialParameters.bIsSpecialEngineMaterial;
	if (bIsCompatibleWithWater)
	{
		// Only let the PC platform compile the permutations supporting selection : 
		return (!bWithWaterSelectionSupport || IsPCPlatform(Parameters.Platform));
	}
	return false;
}

template <bool bWithWaterSelectionSupport>
void TWaterVertexFactory<bWithWaterSelectionSupport>::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("WATER_MESH_FACTORY"), 1);
#if 0
	const bool bUseGPUSceneAndPrimitiveIdStream = 
		Type->SupportsPrimitiveIdStream() 
		&& UseGPUScene(Parameters.Platform, GetMaxSupportedFeatureLevel(Parameters.Platform))
		// TODO: support GPUScene on mobile
		&& !IsMobilePlatform(Parameters.Platform);

	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bUseGPUSceneAndPrimitiveIdStream);
#endif

	if (bWithWaterSelectionSupport)
	{
		OutEnvironment.SetDefine(TEXT("USE_VERTEXFACTORY_HITPROXY_ID"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("WITH_WATER_SELECTION_SUPPORT_VF"), TEXT("1"));
	}

	OutEnvironment.SetDefine(TEXT("RAY_TRACING_DYNAMIC_MESH_IN_LOCAL_SPACE"), TEXT("1"));
}

template <bool bWithWaterSelectionSupport>
void TWaterVertexFactory<bWithWaterSelectionSupport>::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
#if 0
	if (Type->SupportsPrimitiveIdStream()
		&& UseGPUScene(Platform, GetMaxSupportedFeatureLevel(Platform))
		&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::StaticStructMetadata.GetShaderVariableName()))
	{
		OutErrors.AddUnique(*FString::Printf(TEXT("Shader attempted to bind the Primitive uniform buffer even though Vertex Factory %s computes a PrimitiveId per-instance.  This will break auto-instancing.  Shaders should use GetPrimitiveData(Parameters).Member instead of Primitive.Member."), Type->GetName()));
	}
#endif
}

template <bool bWithWaterSelectionSupport>
void TWaterVertexFactory<bWithWaterSelectionSupport>::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	// Add position stream
	Elements.Add(FVertexElement(0, 0, VET_Float4, 0, 0, false));

	// Add all the additional streams
	for (int32 StreamIdx = 0; StreamIdx < NumAdditionalVertexStreams; ++StreamIdx)
	{
		Elements.Add(FVertexElement(1 + StreamIdx, 0, VET_Float4, 8 + StreamIdx, 0, true));
	}
}
