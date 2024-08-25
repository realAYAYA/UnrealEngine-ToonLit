// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollectionRendering.h"
#include "GlobalRenderResources.h"
#include "MaterialDomain.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Misc/DelayedAutoRegister.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "PrimitiveUniformShaderParameters.h"
#include "RenderUtils.h"

IMPLEMENT_TYPE_LAYOUT(FGeometryCollectionVertexFactoryShaderParameters);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGeometryCollectionVertexFactoryUniformShaderParameters, "GeometryCollectionVF");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGCBoneLooseParameters, "GCBoneLooseParameters");

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGeometryCollectionVertexFactory, SF_Vertex, FGeometryCollectionVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FGeometryCollectionVertexFactory, "/Engine/Private/GeometryCollectionVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsStaticLighting
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPositionOnly
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsManualVertexFetch
	| EVertexFactoryFlags::SupportsPSOPrecaching
);

bool FGeometryCollectionVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	if (!Parameters.MaterialParameters.bIsUsedWithGeometryCollections && !Parameters.MaterialParameters.bIsSpecialEngineMaterial)
	{
		return false;
	}

	// Only compile this permutation inside the editor - it's not applicable in games, but occasionally the editor needs it.
	if (Parameters.MaterialParameters.MaterialDomain == MD_UI)
	{
		return !!WITH_EDITOR;
	}

	return true;
}

//
// Modify compile environment to enable instancing
// @param OutEnvironment - shader compile environment to modify
//
void FGeometryCollectionVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	const FStaticFeatureLevel MaxSupportedFeatureLevel = GetMaxSupportedFeatureLevel(Parameters.Platform);
	// TODO: support GPUScene on mobile
	const bool bUseGPUScene = UseGPUScene(Parameters.Platform, MaxSupportedFeatureLevel);
	const bool bSupportsPrimitiveIdStream = Parameters.VertexFactoryType->SupportsPrimitiveIdStream();

	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bSupportsPrimitiveIdStream && bUseGPUScene);

	if (RHISupportsManualVertexFetch(Parameters.Platform))
	{
		OutEnvironment.SetDefineIfUnset(TEXT("MANUAL_VERTEX_FETCH"), TEXT("1"));
	}

	// Geometry collections use a custom hit proxy per bone
	OutEnvironment.SetDefine(TEXT("USE_PER_VERTEX_HITPROXY_ID"), 1);

	OutEnvironment.SetDefine(TEXT("RAY_TRACING_DYNAMIC_MESH_IN_LOCAL_SPACE"), TEXT("1"));
}

void FGeometryCollectionVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
	if (Type->SupportsPrimitiveIdStream()
		&& UseGPUScene(Platform, GetMaxSupportedFeatureLevel(Platform))
		&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName()))
	{
		OutErrors.AddUnique(*FString::Printf(TEXT("Shader attempted to bind the Primitive uniform buffer even though Vertex Factory %s computes a PrimitiveId per-instance.  This will break auto-instancing.  Shaders should use GetPrimitiveData(Parameters).Member instead of Primitive.Member."), Type->GetName()));
	}
}

void FGeometryCollectionVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	Elements.Add(FVertexElement(0, 0, VET_Float3, 0, sizeof(float)*3u, false));

	if (VertexInputStreamType == EVertexInputStreamType::PositionAndNormalOnly)
	{
		// 2-axis TangentBasis components in a single buffer, hence *2u
		Elements.Add(FVertexElement(1, 0, VET_PackedNormal, 2, sizeof(FPackedNormal)*2u, false));
	}

	if (UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel)
		&& !PlatformGPUSceneUsesUniformBufferView(GMaxRHIShaderPlatform))
	{
		switch (VertexInputStreamType)
		{
		case EVertexInputStreamType::Default:
		{
			Elements.Add(FVertexElement(1, 0, VET_UInt, 13, sizeof(uint32), true));
			break;
		}
		case EVertexInputStreamType::PositionOnly:
		{
			Elements.Add(FVertexElement(1, 0, VET_UInt, 1, sizeof(uint32), true));
			break;
		}
		case EVertexInputStreamType::PositionAndNormalOnly:
		{
			Elements.Add(FVertexElement(2, 0, VET_UInt, 1, sizeof(uint32), true));
			break;
		}
		default:
			checkNoEntry();
		}
	}
}

void FGeometryCollectionVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	SCOPED_LOADTIMER(FGeometryCollectionVertexFactory_InitRHI);

	// We create different streams based on feature level
	check(HasValidFeatureLevel());

	// VertexFactory needs to be able to support max possible shader platform and feature level
	// in case if we switch feature level at runtime.
	const bool bCanUseGPUScene = UseGPUScene(GMaxRHIShaderPlatform, GetFeatureLevel());
	const bool bUseManualVertexFetch = SupportsManualVertexFetch(GetFeatureLevel());

	// If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
	// then initialize PositionStream and PositionDeclaration.
	if (Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
	{
		auto AddDeclaration = [this, bCanUseGPUScene](EVertexInputStreamType InputStreamType, bool bAddNormal)
		{
			FVertexDeclarationElementList StreamElements;
			StreamElements.Add(AccessStreamComponent(Data.PositionComponent, 0, InputStreamType));

			bAddNormal = bAddNormal && Data.TangentBasisComponents[1].VertexBuffer != NULL;
			if (bAddNormal)
			{
				StreamElements.Add(AccessStreamComponent(Data.TangentBasisComponents[1], 2, InputStreamType));
			}

			AddPrimitiveIdStreamElement(InputStreamType, StreamElements, 1, 1);

			InitDeclaration(StreamElements, InputStreamType);
		};

		AddDeclaration(EVertexInputStreamType::PositionOnly, false);
		AddDeclaration(EVertexInputStreamType::PositionAndNormalOnly, true);
	}

	FVertexDeclarationElementList Elements;
	if (Data.PositionComponent.VertexBuffer != nullptr)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
	}

	AddPrimitiveIdStreamElement(EVertexInputStreamType::Default, Elements, 13, 13);

	// Only the tangent and normal are used by the stream; the bitangent is derived in the shader.
	uint8 TangentBasisAttributes[2] = { 1, 2 };
	for (int32 AxisIndex = 0; AxisIndex < 2; AxisIndex++)
	{
		if (Data.TangentBasisComponents[AxisIndex].VertexBuffer != nullptr)
		{
			Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex]));
		}
	}

	if (Data.ColorComponentsSRV == nullptr)
	{
		Data.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
		Data.ColorIndexMask = 0;
	}

	ColorStreamIndex = INDEX_NONE;
	if (Data.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.ColorComponent, 3));
		ColorStreamIndex = Elements.Last().StreamIndex;
	}
	else
	{
		// If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		// This wastes 4 bytes per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color, EVertexStreamUsage::ManualFetch);
		Elements.Add(AccessStreamComponent(NullColorComponent, 3));
		ColorStreamIndex = Elements.Last().StreamIndex;
	}

	if (Data.TextureCoordinates.Num())
	{
		const int32 BaseTexCoordAttribute = 4;
		for (int32 CoordinateIndex = 0; CoordinateIndex < Data.TextureCoordinates.Num(); ++CoordinateIndex)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}

		for (int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < MAX_STATIC_TEXCOORDS / 2; ++CoordinateIndex)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}
	}

	if (Data.LightMapCoordinateComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.LightMapCoordinateComponent, 15));
	}
	else if (Data.TextureCoordinates.Num())
	{
		Elements.Add(AccessStreamComponent(Data.TextureCoordinates[0], 15));
	}

	check(Streams.Num() > 0);

	InitDeclaration(Elements);
	check(IsValidRef(GetDeclaration()));

	const int32 DefaultBaseVertexIndex = 0;
	const int32 DefaultPreSkinBaseVertexIndex = 0;

	if (bUseManualVertexFetch || bCanUseGPUScene)
	{
		SCOPED_LOADTIMER(FGeometryCollectionVertexFactory_InitRHI_CreateLocalVFUniformBuffer);

		FGeometryCollectionVertexFactoryUniformShaderParameters UniformParameters;

		UniformParameters.LODLightmapDataIndex = Data.LODLightmapDataIndex;
		int32 ColorIndexMask = 0;

		if (bUseManualVertexFetch)
		{
			UniformParameters.VertexFetch_PositionBuffer = GetPositionsSRV();
			UniformParameters.VertexFetch_PackedTangentsBuffer = GetTangentsSRV();
			UniformParameters.VertexFetch_TexCoordBuffer = GetTextureCoordinatesSRV();

			UniformParameters.VertexFetch_ColorComponentsBuffer = GetColorComponentsSRV();
			ColorIndexMask = (int32)GetColorIndexMask();
		}
		else
		{
			UniformParameters.VertexFetch_PositionBuffer = GNullColorVertexBuffer.VertexBufferSRV;
			UniformParameters.VertexFetch_PackedTangentsBuffer = GNullColorVertexBuffer.VertexBufferSRV;
			UniformParameters.VertexFetch_TexCoordBuffer = GNullColorVertexBuffer.VertexBufferSRV;
		}

		if (!UniformParameters.VertexFetch_ColorComponentsBuffer)
		{
			UniformParameters.VertexFetch_ColorComponentsBuffer = GNullColorVertexBuffer.VertexBufferSRV;
		}

		const int32 NumTexCoords = GetNumTexcoords();
		const int32 LightMapCoordinateIndex = GetLightMapCoordinateIndex();

		UniformParameters.VertexFetch_Parameters = { ColorIndexMask, NumTexCoords, LightMapCoordinateIndex, 0 };

		EUniformBufferUsage UniformBufferUsage = EnableLooseParameter ? UniformBuffer_SingleFrame : UniformBuffer_MultiFrame;

		UniformBuffer = TUniformBufferRef<FGeometryCollectionVertexFactoryUniformShaderParameters>::CreateUniformBufferImmediate(UniformParameters, UniformBufferUsage);
		
		FGCBoneLooseParameters LooseParameters;

		LooseParameters.VertexFetch_BoneTransformBuffer = GetBoneTransformSRV();
		LooseParameters.VertexFetch_BonePrevTransformBuffer = GetBonePrevTransformSRV();
		LooseParameters.VertexFetch_BoneMapBuffer = GetBoneMapSRV();

		LooseParameterUniformBuffer = FGCBoneLooseParametersRef::CreateUniformBufferImmediate(LooseParameters, UniformBufferUsage);
	}

	check(IsValidRef(GetDeclaration()));
}

void FGeometryCollectionVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	LooseParameterUniformBuffer.SafeRelease();
	FVertexFactory::ReleaseRHI();
}

void FGeometryCollectionVertexFactoryShaderParameters::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	class FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const
{
	check(VertexFactory->GetType() == &FGeometryCollectionVertexFactory::StaticType);
	const auto* TypedVertexFactory = static_cast<const FGeometryCollectionVertexFactory*>(VertexFactory);

	const bool bSupportsManualFetch = TypedVertexFactory->SupportsManualVertexFetch(FeatureLevel);

	FRHIUniformBuffer* VertexFactoryUniformBuffer = TypedVertexFactory->GetUniformBuffer();

	if (bSupportsManualFetch || UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
	{
		check(VertexFactoryUniformBuffer != nullptr);
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FGeometryCollectionVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);
	}

	// We only want to set the SRV parameters if we support manual vertex fetch.
	if (bSupportsManualFetch)
	{
		FUniformBufferRHIRef LooseParameterBuffer = TypedVertexFactory->GetLooseParameterBuffer();
		check(LooseParameterBuffer != nullptr);
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FGCBoneLooseParameters>(), LooseParameterBuffer);
	}
}

#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGeometryCollectionVertexFactory, SF_RayHitGroup, FGeometryCollectionVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGeometryCollectionVertexFactory, SF_Compute, FGeometryCollectionVertexFactoryShaderParameters);
#endif // RHI_RAYTRACING
