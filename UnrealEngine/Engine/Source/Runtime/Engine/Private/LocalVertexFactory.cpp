// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LocalVertexFactory.cpp: Local vertex factory implementation
=============================================================================*/

#include "LocalVertexFactory.h"
#include "Animation/MeshDeformerGeometry.h"
#include "MeshBatch.h"
#include "MeshDrawShaderBindings.h"
#include "SkeletalRenderPublic.h"
#include "SpeedTreeWind.h"
#include "Misc/DelayedAutoRegister.h"
#include "Rendering/ColorVertexBuffer.h"
#include "MaterialDomain.h"
#include "MeshMaterialShader.h"
#include "PrimitiveUniformShaderParameters.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "GPUSkinCache.h"
#include "GPUSkinVertexFactory.h"
#include "RenderUtils.h"
#include "SceneInterface.h"

IMPLEMENT_TYPE_LAYOUT(FLocalVertexFactoryShaderParametersBase);
IMPLEMENT_TYPE_LAYOUT(FLocalVertexFactoryShaderParameters);

class FSpeedTreeWindNullUniformBuffer : public TUniformBuffer<FSpeedTreeUniformParameters>
{
	typedef TUniformBuffer< FSpeedTreeUniformParameters > Super;
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
};

void FSpeedTreeWindNullUniformBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	FSpeedTreeUniformParameters Parameters;
	FMemory::Memzero(Parameters);
	SetContentsNoUpdate(Parameters);
	
	Super::InitRHI(RHICmdList);
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
	IsGPUSkinPassThrough.Bind(ParameterMap, TEXT("bIsGPUSkinPassThrough"));
}

static void GetMeshDeformerVertexStreams(FMeshDeformerGeometry const& InDeformerGeometry, FGPUSkinPassthroughVertexFactory const* InVertexFactory, FVertexInputStreamArray& InOutVertexStreams)
{
	const int32 PositionStreamIndex = InVertexFactory->GetAttributeStreamIndex(FGPUSkinPassthroughVertexFactory::VertexPosition);
	if (PositionStreamIndex > -1 && InDeformerGeometry.Position.IsValid())
	{
		InOutVertexStreams.Add(FVertexInputStream(PositionStreamIndex, 0, InDeformerGeometry.Position->GetRHI()));
	}

	const int32 TangentStreamIndex = InVertexFactory->GetAttributeStreamIndex(FGPUSkinPassthroughVertexFactory::VertexTangent);
	if (TangentStreamIndex > -1 && InDeformerGeometry.Tangent.IsValid())
	{
		InOutVertexStreams.Add(FVertexInputStream(TangentStreamIndex, 0, InDeformerGeometry.Tangent->GetRHI()));
	}

	const int32 ColorStreamIndex = InVertexFactory->GetAttributeStreamIndex(FGPUSkinPassthroughVertexFactory::VertexColor);
	if (ColorStreamIndex > -1 && InDeformerGeometry.Color.IsValid())
	{
		InOutVertexStreams.Add(FVertexInputStream(ColorStreamIndex, 0, InDeformerGeometry.Color->GetRHI()));
	}
}

static void GetElementShaderBindingsGPUSkinPassThrough(
	const FMeshMaterialShader* Shader,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	class FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams)
{
	// Bind vertex streams.
	FSkinBatchVertexFactoryUserData* BatchUserData = (FSkinBatchVertexFactoryUserData*)BatchElement.VertexFactoryUserData;
	FGPUSkinPassthroughVertexFactory const* PassthroughVertexFactory = static_cast<FGPUSkinPassthroughVertexFactory const*>(VertexFactory);

	if (BatchUserData != nullptr && BatchUserData->SkinCacheEntry != nullptr)
	{
		// Using Skin Cache.
		FGPUSkinCache::GetShaderVertexStreams(BatchUserData->SkinCacheEntry, BatchUserData->SectionIndex, PassthroughVertexFactory, VertexStreams);
	}
	else if (BatchUserData != nullptr && BatchUserData->DeformerGeometry != nullptr)
	{
		// Using Mesh Deformers.
		GetMeshDeformerVertexStreams(*BatchUserData->DeformerGeometry, PassthroughVertexFactory, VertexStreams);
	}

	// Bind the vertex factory uniform buffer.
	if (PassthroughVertexFactory->SupportsManualVertexFetch(FeatureLevel) || UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
	{
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), PassthroughVertexFactory->GetUniformBuffer());
	}
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
			Shader,
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
	FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	// Don't override e.g. SplineMesh's opt-out
	OutEnvironment.SetDefineIfUnset(TEXT("VF_SUPPORTS_SPEEDTREE_WIND"), TEXT("1"));

	if (RHISupportsManualVertexFetch(Parameters.Platform))
	{
		OutEnvironment.SetDefineIfUnset(TEXT("MANUAL_VERTEX_FETCH"), TEXT("1"));
	}

	const bool bVFSupportsPrimtiveSceneData = Parameters.VertexFactoryType->SupportsPrimitiveIdStream() && UseGPUScene(Parameters.Platform, GetMaxSupportedFeatureLevel(Parameters.Platform));
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bVFSupportsPrimtiveSceneData);

	// When combining ray tracing and WPO, leave the mesh in local space for consistency with how shading normals are calculated.
	// See UE-139634 for the case that lead to this.
	OutEnvironment.SetDefine(TEXT("RAY_TRACING_DYNAMIC_MESH_IN_LOCAL_SPACE"), TEXT("1"));

	if (Parameters.VertexFactoryType->SupportsGPUSkinPassThrough())
	{
		OutEnvironment.SetDefine(TEXT("SUPPORT_GPUSKIN_PASSTHROUGH"), IsGPUSkinPassThroughSupported(Parameters.Platform));
	}

	OutEnvironment.SetDefine(TEXT("ALWAYS_EVALUATE_WORLD_POSITION_OFFSET"),
		Parameters.MaterialParameters.bAlwaysEvaluateWorldPositionOffset ? 1 : 0);
}

void FLocalVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
	if (Type->SupportsPrimitiveIdStream() 
		&& UseGPUScene(Platform, GetMaxSupportedFeatureLevel(Platform)) 
		&& !IsMobilePlatform(Platform) // On mobile VS may use PrimtiveUB while GPUScene is enabled
		&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName()))
	{
		OutErrors.AddUnique(*FString::Printf(TEXT("Shader attempted to bind the Primitive uniform buffer even though Vertex Factory %s computes a PrimitiveId per-instance.  This will break auto-instancing.  Shaders should use GetPrimitiveData(Parameters).Member instead of Primitive.Member."), Type->GetName()));
	}
}

/**
* Return the vertex elements used when manual vertex fetch is used
*/
void FLocalVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	Elements.Add(FVertexElement(0, 0, VET_Float3, 0, sizeof(float)*3u, false));
	
	if (VertexInputStreamType == EVertexInputStreamType::PositionAndNormalOnly)
	{
		// 2-axis TangentBasis components in a single buffer, hence *2u
		Elements.Add(FVertexElement(1, 4, VET_PackedNormal, 2, sizeof(FPackedNormal)*2u, false));
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

void FLocalVertexFactory::GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, EVertexInputStreamType InputStreamType, bool bSupportsManualVertexFetch, FDataType& Data, FVertexDeclarationElementList& Elements)
{
	FVertexStreamList VertexStreams;
	int32 ColorStreamIndex;
	GetVertexElements(FeatureLevel, InputStreamType, bSupportsManualVertexFetch, Data, Elements, VertexStreams, ColorStreamIndex);

	if (UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel) 
		&& !PlatformGPUSceneUsesUniformBufferView(GMaxRHIShaderPlatform))
	{
		Elements.Add(FVertexElement(VertexStreams.Num(), 0, VET_UInt, 13, sizeof(uint32), true));
	}
}

void FLocalVertexFactory::SetData(const FDataType& InData)
{
	SetData(FRHICommandListImmediate::Get(), InData);
}

void FLocalVertexFactory::SetData(FRHICommandListBase& RHICmdList, const FDataType& InData)
{
	// The shader code makes assumptions that the color component is a FColor, performing swizzles on ES3 and Metal platforms as necessary
	// If the color is sent down as anything other than VET_Color then you'll get an undesired swizzle on those platforms
	check((InData.ColorComponent.Type == VET_None) || (InData.ColorComponent.Type == VET_Color));

	Data = InData;
	UpdateRHI(RHICmdList);
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

void FLocalVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
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

			AddPrimitiveIdStreamElement(InputStreamType, StreamElements, 1, 1);

			InitDeclaration(StreamElements, InputStreamType);
		};

		AddDeclaration(EVertexInputStreamType::PositionOnly, false);
		AddDeclaration(EVertexInputStreamType::PositionAndNormalOnly, true);
	}

	FVertexDeclarationElementList Elements;
	GetVertexElements(GetFeatureLevel(), EVertexInputStreamType::Default, bUseManualVertexFetch, Data, Elements, Streams, ColorStreamIndex);
	AddPrimitiveIdStreamElement(EVertexInputStreamType::Default, Elements, 13, 13);
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
	LooseParameters.FrameNumber = -1;
	LooseParameters.GPUSkinPassThroughPositionBuffer = GNullVertexBuffer.VertexBufferSRV;
	LooseParameters.GPUSkinPassThroughPreviousPositionBuffer = GNullVertexBuffer.VertexBufferSRV;
	LooseParameters.GPUSkinPassThroughPreSkinnedTangentBuffer = GNullVertexBuffer.VertexBufferSRV;
	LooseParametersUniformBuffer = TUniformBufferRef<FLocalVertexFactoryLooseParameters>::CreateUniformBufferImmediate(LooseParameters, UniformBuffer_MultiFrame);

	check(IsValidRef(GetDeclaration()));
}

void FLocalVertexFactory::GetVertexElements(
	ERHIFeatureLevel::Type FeatureLevel, 
	EVertexInputStreamType InputStreamType,
	bool bSupportsManualVertexFetch,
	FDataType& Data, 
	FVertexDeclarationElementList& Elements, 
	FVertexStreamList& InOutStreams, 
	int32& OutColorStreamIndex)
{
	check(InputStreamType == EVertexInputStreamType::Default);
	
	if (Data.PositionComponent.VertexBuffer != nullptr)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0, InOutStreams));
	}

#if !WITH_EDITOR
	// Can't rely on manual vertex fetch in the editor to not add the unused elements because vertex factories created
	// with manual vertex fetch support can somehow still be used when booting up in for example ES3.1 preview mode
	// The vertex factories are then used during mobile rendering and will cause PSO creation failure.
	// First need to fix invalid usage of these vertex factories before this can be enabled again. (UE-165187)
	if (!bSupportsManualVertexFetch)
#endif // WITH_EDITOR
	{
		// Only the tangent and normal are used by the stream; the bitangent is derived in the shader.
		uint8 TangentBasisAttributes[2] = { 1, 2 };
		for (int32 AxisIndex = 0; AxisIndex < 2; AxisIndex++)
		{
			if (Data.TangentBasisComponents[AxisIndex].VertexBuffer != nullptr)
			{
				Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex], InOutStreams));
			}
		}

		if (Data.ColorComponentsSRV == nullptr)
		{
			Data.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
			Data.ColorIndexMask = 0;
		}

		if (Data.ColorComponent.VertexBuffer)
		{
			Elements.Add(AccessStreamComponent(Data.ColorComponent, 3, InOutStreams));
		}
		else
		{
			// If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
			// This wastes 4 bytes per vertex, but prevents having to compile out twice the number of vertex factories.
			FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color, EVertexStreamUsage::ManualFetch);
			Elements.Add(AccessStreamComponent(NullColorComponent, 3, InOutStreams));
		}
		OutColorStreamIndex = Elements.Last().StreamIndex;

		if (Data.TextureCoordinates.Num())
		{
			const int32 BaseTexCoordAttribute = 4;
			for (int32 CoordinateIndex = 0; CoordinateIndex < Data.TextureCoordinates.Num(); ++CoordinateIndex)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[CoordinateIndex],
					BaseTexCoordAttribute + CoordinateIndex,
					InOutStreams
				));
			}

			for (int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < MAX_STATIC_TEXCOORDS / 2; ++CoordinateIndex)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
					BaseTexCoordAttribute + CoordinateIndex,
					InOutStreams
				));
			}
		}

		// TODO: should also check if VFType supports 'SupportsGPUSkinPassThrough'
		if (IsGPUSkinPassThroughSupported(GMaxRHIShaderPlatform))
		{
			// Fill PreSkinPosition slot for GPUSkinPassThrough vertex factory, or else use a dummy buffer.
			FVertexStreamComponent NullComponent(&GNullVertexBuffer, 0, 0, VET_Float4);
			Elements.Add(AccessStreamComponent(Data.PreSkinPositionComponent.VertexBuffer ? Data.PreSkinPositionComponent : NullComponent, 14, InOutStreams));
		}

		if (Data.LightMapCoordinateComponent.VertexBuffer)
		{
			Elements.Add(AccessStreamComponent(Data.LightMapCoordinateComponent, 15, InOutStreams));
		}
		else if (Data.TextureCoordinates.Num())
		{
			Elements.Add(AccessStreamComponent(Data.TextureCoordinates[0], 15, InOutStreams));
		}
	}
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
	| EVertexFactoryFlags::SupportsLumenMeshCards
);
