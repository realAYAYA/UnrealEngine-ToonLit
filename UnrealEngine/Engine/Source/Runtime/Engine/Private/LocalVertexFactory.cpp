// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LocalVertexFactory.cpp: Local vertex factory implementation
=============================================================================*/

#include "LocalVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "SpeedTreeWind.h"
#include "ShaderParameterUtils.h"
#include "Rendering/ColorVertexBuffer.h"
#include "MeshMaterialShader.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "GPUSkinCache.h"
#include "GPUSkinVertexFactory.h"

IMPLEMENT_TYPE_LAYOUT(FLocalVertexFactoryShaderParametersBase);
IMPLEMENT_TYPE_LAYOUT(FLocalVertexFactoryShaderParameters);

class FSpeedTreeWindNullUniformBuffer : public TUniformBuffer<FSpeedTreeUniformParameters>
{
	typedef TUniformBuffer< FSpeedTreeUniformParameters > Super;
public:
	virtual void InitDynamicRHI() override;
};

void FSpeedTreeWindNullUniformBuffer::InitDynamicRHI()
{
	FSpeedTreeUniformParameters Parameters;
	FMemory::Memzero(Parameters);
	SetContentsNoUpdate(Parameters);
	
	Super::InitDynamicRHI();
}

static TGlobalResource< FSpeedTreeWindNullUniformBuffer > GSpeedTreeWindNullUniformBuffer;

void FLocalVertexFactoryShaderParametersBase::Bind(const FShaderParameterMap& ParameterMap)
{
	LODParameter.Bind(ParameterMap, TEXT("SpeedTreeLODInfo"));
	bAnySpeedTreeParamIsBound = LODParameter.IsBound() || ParameterMap.ContainsParameterAllocation(TEXT("SpeedTreeData"));
}

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FLocalVertexFactoryUniformShaderParameters, "LocalVF");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FLocalVertexFactoryLooseParameters, "LocalVFLooseParameters");

TUniformBufferRef<FLocalVertexFactoryUniformShaderParameters> CreateLocalVFUniformBuffer(
	const FLocalVertexFactory* LocalVertexFactory, 
	uint32 LODLightmapDataIndex, 
	FColorVertexBuffer* OverrideColorVertexBuffer, 
	int32 BaseVertexIndex,
	int32 PreSkinBaseVertexIndex
	)
{
	FLocalVertexFactoryUniformShaderParameters UniformParameters;

	UniformParameters.LODLightmapDataIndex = LODLightmapDataIndex;
	int32 ColorIndexMask = 0;

	if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
	{
		UniformParameters.VertexFetch_PositionBuffer = LocalVertexFactory->GetPositionsSRV();
		UniformParameters.VertexFetch_PreSkinPositionBuffer = LocalVertexFactory->GetPreSkinPositionSRV();

		UniformParameters.VertexFetch_PackedTangentsBuffer = LocalVertexFactory->GetTangentsSRV();
		UniformParameters.VertexFetch_TexCoordBuffer = LocalVertexFactory->GetTextureCoordinatesSRV();

		if (OverrideColorVertexBuffer)
		{
			UniformParameters.VertexFetch_ColorComponentsBuffer = OverrideColorVertexBuffer->GetColorComponentsSRV();
			ColorIndexMask = OverrideColorVertexBuffer->GetNumVertices() > 1 ? ~0 : 0;
		}
		else
		{
			UniformParameters.VertexFetch_ColorComponentsBuffer = LocalVertexFactory->GetColorComponentsSRV();
			ColorIndexMask = (int32)LocalVertexFactory->GetColorIndexMask();
		}
	}
	else
	{
        UniformParameters.VertexFetch_PositionBuffer = GNullColorVertexBuffer.VertexBufferSRV;
		UniformParameters.VertexFetch_PreSkinPositionBuffer = GNullColorVertexBuffer.VertexBufferSRV;
		UniformParameters.VertexFetch_PackedTangentsBuffer = GNullColorVertexBuffer.VertexBufferSRV;
		UniformParameters.VertexFetch_TexCoordBuffer = GNullColorVertexBuffer.VertexBufferSRV;
	}

	if (!UniformParameters.VertexFetch_ColorComponentsBuffer)
	{
		UniformParameters.VertexFetch_ColorComponentsBuffer = GNullColorVertexBuffer.VertexBufferSRV;
	}

	const int32 NumTexCoords = LocalVertexFactory->GetNumTexcoords();
	const int32 LightMapCoordinateIndex = LocalVertexFactory->GetLightMapCoordinateIndex();
	const int32 EffectiveBaseVertexIndex = RHISupportsAbsoluteVertexID(GMaxRHIShaderPlatform) ? 0 : BaseVertexIndex;
	const int32 EffectivePreSkinBaseVertexIndex = RHISupportsAbsoluteVertexID(GMaxRHIShaderPlatform) ? 0 : PreSkinBaseVertexIndex;

	UniformParameters.VertexFetch_Parameters = {ColorIndexMask, NumTexCoords, LightMapCoordinateIndex, EffectiveBaseVertexIndex};
	UniformParameters.PreSkinBaseVertexIndex = EffectivePreSkinBaseVertexIndex;

	return TUniformBufferRef<FLocalVertexFactoryUniformShaderParameters>::CreateUniformBufferImmediate(UniformParameters, UniformBuffer_MultiFrame);
}

void FLocalVertexFactoryShaderParametersBase::GetElementShaderBindingsBase(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader, 
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory, 
	const FMeshBatchElement& BatchElement,
	FRHIUniformBuffer* VertexFactoryUniformBuffer,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams
	) const
{
	const auto* LocalVertexFactory = static_cast<const FLocalVertexFactory*>(VertexFactory);
	
	if (LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel) || UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
	{
		if (!VertexFactoryUniformBuffer)
		{
			// No batch element override
			VertexFactoryUniformBuffer = LocalVertexFactory->GetUniformBuffer();
		}

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), VertexFactoryUniformBuffer);
	}

	//@todo - allow FMeshBatch to supply vertex streams (instead of requiring that they come from the vertex factory), and this userdata hack will no longer be needed for override vertex color
	if (BatchElement.bUserDataIsColorVertexBuffer)
	{
		FColorVertexBuffer* OverrideColorVertexBuffer = (FColorVertexBuffer*)BatchElement.UserData;
		check(OverrideColorVertexBuffer);

		if (!LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel))
		{
			LocalVertexFactory->GetColorOverrideStream(OverrideColorVertexBuffer, VertexStreams);
		}	
	}

	if (bAnySpeedTreeParamIsBound)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FLocalVertexFactoryShaderParameters_SetMesh_SpeedTree);
		FRHIUniformBuffer* SpeedTreeUniformBuffer = Scene? Scene->GetSpeedTreeUniformBuffer(VertexFactory) : nullptr;
		if (SpeedTreeUniformBuffer == nullptr)
		{
			SpeedTreeUniformBuffer = GSpeedTreeWindNullUniformBuffer.GetUniformBufferRHI();
		}
		check(SpeedTreeUniformBuffer != nullptr);

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FSpeedTreeUniformParameters>(), SpeedTreeUniformBuffer);

		if (LODParameter.IsBound())
		{
			FVector3f LODData(BatchElement.MinScreenSize, BatchElement.MaxScreenSize, BatchElement.MaxScreenSize - BatchElement.MinScreenSize);
			ShaderBindings.Add(LODParameter, LODData);
		}
	}
}

void FLocalVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	FLocalVertexFactoryShaderParametersBase::Bind(ParameterMap);
	GPUSkinCachePositionBuffer.Bind(ParameterMap, TEXT("GPUSkinCachePositionBuffer"));
	IsGPUSkinPassThrough.Bind(ParameterMap, TEXT("bIsGPUSkinPassThrough"));
}

void FLocalVertexFactoryShaderParameters::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams
) const
{
	FLocalVertexFactory const* LocalVertexFactory = static_cast<FLocalVertexFactory const*>(VertexFactory);
	ShaderBindings.Add(IsGPUSkinPassThrough, (uint32)(LocalVertexFactory->bGPUSkinPassThrough ? 1 : 0));
	if (LocalVertexFactory->bGPUSkinPassThrough)
	{
		GetElementShaderBindingsGPUSkinPassThrough(
			Scene,
			View,
			Shader,
			InputStreamType,
			FeatureLevel,
			VertexFactory,
			BatchElement,
			ShaderBindings,
			VertexStreams);
	}
	else
	{
		// Decode VertexFactoryUserData as VertexFactoryUniformBuffer
		FRHIUniformBuffer* VertexFactoryUniformBuffer = static_cast<FRHIUniformBuffer*>(BatchElement.VertexFactoryUserData);

		FLocalVertexFactoryShaderParametersBase::GetElementShaderBindingsBase(
			Scene,
			View,
			Shader,
			InputStreamType,
			FeatureLevel,
			VertexFactory,
			BatchElement,
			VertexFactoryUniformBuffer,
			ShaderBindings,
			VertexStreams);
	}

	ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryLooseParameters>(), LocalVertexFactory->LooseParametersUniformBuffer);
}

void FLocalVertexFactoryShaderParameters::GetElementShaderBindingsGPUSkinPassThrough(
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
	// #dxr_todo do we need this call to the base?
	FLocalVertexFactoryShaderParametersBase::GetElementShaderBindingsBase(Scene, View, Shader, InputStreamType, FeatureLevel, VertexFactory, BatchElement, nullptr, ShaderBindings, VertexStreams);

	// todo: Add more context into VertexFactoryUserData about whether this is skin cache/mesh deformer/ray tracing etc.
	// For now it just holds a skin cache pointer which is null if using other mesh deformers.
	FGPUSkinBatchElementUserData* BatchUserData = (FGPUSkinBatchElementUserData*)BatchElement.VertexFactoryUserData;
	const bool bUsesSkinCache = BatchUserData != nullptr;

	check(VertexFactory->GetType() == &FGPUSkinPassthroughVertexFactory::StaticType);
	FGPUSkinPassthroughVertexFactory const* PassthroughVertexFactory = static_cast<FGPUSkinPassthroughVertexFactory const*>(VertexFactory);
	if (bUsesSkinCache)
	{
		GetElementShaderBindingsSkinCache(PassthroughVertexFactory, BatchUserData, ShaderBindings, VertexStreams);
	}
	else
	{
		GetElementShaderBindingsMeshDeformer(PassthroughVertexFactory, ShaderBindings, VertexStreams);
	}
}

void FLocalVertexFactoryShaderParameters::GetElementShaderBindingsSkinCache(
	FGPUSkinPassthroughVertexFactory const* PassthroughVertexFactory,
	FGPUSkinBatchElementUserData* BatchUserData,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const
{
	FGPUSkinCache::GetShaderBindings(
		BatchUserData->Entry, BatchUserData->Section,
		PassthroughVertexFactory,
		GPUSkinCachePositionBuffer,
		ShaderBindings, VertexStreams);
}

void FLocalVertexFactoryShaderParameters::GetElementShaderBindingsMeshDeformer(
	FGPUSkinPassthroughVertexFactory const* PassthroughVertexFactory,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const
{
	if (PassthroughVertexFactory->PositionRDG.IsValid())
	{
		VertexStreams.Add(FVertexInputStream(PassthroughVertexFactory->GetPositionStreamIndex(), 0, PassthroughVertexFactory->PositionRDG->GetRHI()));
		ShaderBindings.Add(GPUSkinCachePositionBuffer, PassthroughVertexFactory->GetPositionsSRV());
	}
	if (PassthroughVertexFactory->TangentRDG.IsValid() && PassthroughVertexFactory->GetTangentStreamIndex() > -1)
	{
		VertexStreams.Add(FVertexInputStream(PassthroughVertexFactory->GetTangentStreamIndex(), 0, PassthroughVertexFactory->TangentRDG->GetRHI()));
	}
}

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
bool FLocalVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	// Only compile this permutation inside the editor - it's not applicable in games, but occasionally the editor needs it.
	if (Parameters.MaterialParameters.MaterialDomain == MD_UI)
	{
		return !!WITH_EDITOR;
	}

	return true; 
}

void FLocalVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	// Don't override e.g. SplineMesh's opt-out
	if (!OutEnvironment.GetDefinitions().Contains("VF_SUPPORTS_SPEEDTREE_WIND"))
	{
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_SPEEDTREE_WIND"), TEXT("1"));
	}

	const bool ContainsManualVertexFetch = OutEnvironment.GetDefinitions().Contains("MANUAL_VERTEX_FETCH");
	if (!ContainsManualVertexFetch && RHISupportsManualVertexFetch(Parameters.Platform))
	{
		OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("1"));
	}

	const bool bVFSupportsPrimtiveSceneData = Parameters.VertexFactoryType->SupportsPrimitiveIdStream() && UseGPUScene(Parameters.Platform, GetMaxSupportedFeatureLevel(Parameters.Platform));
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bVFSupportsPrimtiveSceneData);

	// When combining ray tracing and WPO, leave the mesh in local space for consistency with how shading normals are calculated.
	// See UE-139634 for the case that lead to this.
	OutEnvironment.SetDefine(TEXT("RAY_TRACING_DYNAMIC_MESH_IN_LOCAL_SPACE"), TEXT("1"));

	if (Parameters.VertexFactoryType->SupportsGPUSkinPassThrough())
	{
		OutEnvironment.SetDefine(TEXT("SUPPORT_GPUSKIN_PASSTHROUGH"), IsGPUSkinCacheAvailable(Parameters.Platform));
	}
}

void FLocalVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
	if (Type->SupportsPrimitiveIdStream() 
		&& UseGPUScene(Platform, GetMaxSupportedFeatureLevel(Platform)) 
		&& !IsMobilePlatform(Platform) // On mobile VS may use PrimtiveUB while GPUScene is enabled
		&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::StaticStructMetadata.GetShaderVariableName()))
	{
		OutErrors.AddUnique(*FString::Printf(TEXT("Shader attempted to bind the Primitive uniform buffer even though Vertex Factory %s computes a PrimitiveId per-instance.  This will break auto-instancing.  Shaders should use GetPrimitiveData(Parameters).Member instead of Primitive.Member."), Type->GetName()));
	}
}

/**
* Return the vertex elements used when manual vertex fetch is used
*/
void FLocalVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	Elements.Add(FVertexElement(0, 0, VET_Float3, 0, 12, false));

	switch (VertexInputStreamType)
	{
	case EVertexInputStreamType::Default:
	{
		Elements.Add(FVertexElement(1, 0, VET_UInt, 13, 0, true));
		break;
	}
	case EVertexInputStreamType::PositionOnly:
	{
		Elements.Add(FVertexElement(1, 0, VET_UInt, 1, 0, true));
		break;
	}
	case EVertexInputStreamType::PositionAndNormalOnly:
	{
		Elements.Add(FVertexElement(1, 4, VET_PackedNormal, 2, 0, false));
		Elements.Add(FVertexElement(2, 0, VET_UInt, 1, 0, true));
		break;
	}
	default:
		checkNoEntry();
	}
}

void FLocalVertexFactory::SetData(const FDataType& InData)
{
	check(IsInRenderingThread());

	{
		//const int NumTexCoords = InData.NumTexCoords;
		//const int LightMapCoordinateIndex = InData.LightMapCoordinateIndex;
		//check(NumTexCoords > 0);
		//check(LightMapCoordinateIndex < NumTexCoords && LightMapCoordinateIndex >= 0);
		//check(InData.PositionComponentSRV);
		//check(InData.TangentsSRV);
		//check(InData.TextureCoordinatesSRV);
		//check(InData.ColorComponentsSRV);
	}

	// The shader code makes assumptions that the color component is a FColor, performing swizzles on ES3 and Metal platforms as necessary
	// If the color is sent down as anything other than VET_Color then you'll get an undesired swizzle on those platforms
	check((InData.ColorComponent.Type == VET_None) || (InData.ColorComponent.Type == VET_Color));

	Data = InData;
	UpdateRHI();
}

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FLocalVertexFactory::Copy(const FLocalVertexFactory& Other)
{
	FLocalVertexFactory* VertexFactory = this;
	const FDataType* DataCopy = &Other.Data;
	ENQUEUE_RENDER_COMMAND(FLocalVertexFactoryCopyData)(
		[VertexFactory, DataCopy](FRHICommandListImmediate& RHICmdList)
		{
			VertexFactory->Data = *DataCopy;
		});
	BeginUpdateResourceRHI(this);
}

void FLocalVertexFactory::InitRHI()
{
	SCOPED_LOADTIMER(FLocalVertexFactory_InitRHI);

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
		auto AddDeclaration = [this](EVertexInputStreamType InputStreamType, bool bAddNormal)
		{
			FVertexDeclarationElementList StreamElements;
			StreamElements.Add(AccessStreamComponent(Data.PositionComponent, 0, InputStreamType));

			bAddNormal = bAddNormal && Data.TangentBasisComponents[1].VertexBuffer != NULL;
			if (bAddNormal)
			{
				StreamElements.Add(AccessStreamComponent(Data.TangentBasisComponents[1], 2, InputStreamType));
			}

			AddPrimitiveIdStreamElement(InputStreamType, StreamElements, 1, 8);

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

	AddPrimitiveIdStreamElement(EVertexInputStreamType::Default, Elements, 13, 8);

#if !WITH_EDITOR
	// Can't rely on manual vertex fetch in the editor to not add the unused elements because vertex factories created
	// with manual vertex fetch support can somehow still be used when booting up in for example ES3.1 preview mode
	// The vertex factories are then used during mobile rendering and will cause PSO creation failure.
	// First need to fix invalid usage of these vertex factories before this can be enabled again. (UE-165187)
	if (!bUseManualVertexFetch)
#endif // WITH_EDITOR
	{
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

		ColorStreamIndex = -1;
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

		// Fill PreSkinPosition slot for GPUSkinPassThrough vertex factory, or else use a dummy buffer.
		FVertexStreamComponent NullComponent(&GNullVertexBuffer, 0, 0, VET_Float4);
		Elements.Add(AccessStreamComponent(Data.PreSkinPositionComponent.VertexBuffer ? Data.PreSkinPositionComponent : NullComponent, 14));

		if (Data.LightMapCoordinateComponent.VertexBuffer)
		{
			Elements.Add(AccessStreamComponent(Data.LightMapCoordinateComponent, 15));
		}
		else if (Data.TextureCoordinates.Num())
		{
			Elements.Add(AccessStreamComponent(Data.TextureCoordinates[0], 15));
		}
	}

	check(Streams.Num() > 0);

	InitDeclaration(Elements);
	check(IsValidRef(GetDeclaration()));

	const int32 DefaultBaseVertexIndex = 0;
	const int32 DefaultPreSkinBaseVertexIndex = 0;

	if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || bCanUseGPUScene)
	{
		SCOPED_LOADTIMER(FLocalVertexFactory_InitRHI_CreateLocalVFUniformBuffer);
		UniformBuffer = CreateLocalVFUniformBuffer(this, Data.LODLightmapDataIndex, nullptr, DefaultBaseVertexIndex, DefaultPreSkinBaseVertexIndex);
	}

	FLocalVertexFactoryLooseParameters LooseParameters;
	LooseParameters.GPUSkinPassThroughPreviousPositionBuffer = GNullVertexBuffer.VertexBufferSRV;
	LooseParametersUniformBuffer = TUniformBufferRef<FLocalVertexFactoryLooseParameters>::CreateUniformBufferImmediate(LooseParameters, UniformBuffer_MultiFrame);

	check(IsValidRef(GetDeclaration()));
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FLocalVertexFactory, SF_Vertex, FLocalVertexFactoryShaderParameters);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FLocalVertexFactory, SF_RayHitGroup, FLocalVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FLocalVertexFactory, SF_Compute, FLocalVertexFactoryShaderParameters);
#endif // RHI_RAYTRACING

IMPLEMENT_VERTEX_FACTORY_TYPE(FLocalVertexFactory,"/Engine/Private/LocalVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsStaticLighting
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPositionOnly
	| EVertexFactoryFlags::SupportsCachingMeshDrawCommands
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsLightmapBaking
	| EVertexFactoryFlags::SupportsManualVertexFetch
	| EVertexFactoryFlags::SupportsPSOPrecaching
	| EVertexFactoryFlags::SupportsGPUSkinPassThrough
);
