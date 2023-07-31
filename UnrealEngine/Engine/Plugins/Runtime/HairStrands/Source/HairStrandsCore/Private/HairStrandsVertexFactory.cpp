// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StrandHairVertexFactory.cpp: Strand hair vertex factory implementation
=============================================================================*/

#include "HairStrandsVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "ShaderParameterUtils.h"
#include "Rendering/ColorVertexBuffer.h"
#include "MeshMaterialShader.h"
#include "HairStrandsInterface.h"
#include "GroomInstance.h"

#define VF_STRANDS_SUPPORT_GPU_SCENE 0
#define VF_STRANDS_PROCEDURAL_INTERSECTOR 1

bool GetSupportHairStrandsProceduralPrimitive(EShaderPlatform InShaderPlatform)
{
	return VF_STRANDS_PROCEDURAL_INTERSECTOR && FDataDrivenShaderPlatformInfo::GetSupportsRayTracingProceduralPrimitive(InShaderPlatform);
}

/////////////////////////////////////////////////////////////////////////////////////////

template<typename T> inline void VFS_BindParam(FMeshDrawSingleShaderBindings& ShaderBindings, const FShaderResourceParameter& Param, T* Value)	{ if (Param.IsBound() && Value) ShaderBindings.Add(Param, Value); }
template<typename T> inline void VFS_BindParam(FMeshDrawSingleShaderBindings& ShaderBindings, const FShaderParameter& Param, const T& Value)	{ if (Param.IsBound()) ShaderBindings.Add(Param, Value); }

class FDummyCulledDispatchVertexIdsBuffer : public FVertexBuffer
{
public:
	FShaderResourceViewRHIRef SRVUint;
	FShaderResourceViewRHIRef SRVFloat;
	FShaderResourceViewRHIRef SRVRGBA;
	FShaderResourceViewRHIRef SRVRGBA_Uint;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FDummyCulledDispatchVertexIdsBuffer"));
		uint32 NumBytes = sizeof(uint32) * 4;
		VertexBufferRHI = RHICreateBuffer(NumBytes, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		uint32* DummyContents = (uint32*)RHILockBuffer(VertexBufferRHI, 0, NumBytes, RLM_WriteOnly);
		DummyContents[0] = DummyContents[1] = DummyContents[2] = DummyContents[3] = 0;
		RHIUnlockBuffer(VertexBufferRHI);

		SRVUint = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_UINT);
		SRVFloat = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_FLOAT);
		SRVRGBA = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R8G8B8A8);
		SRVRGBA_Uint = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R8G8B8A8_UINT);
	}

	virtual void ReleaseRHI() override
	{
		VertexBufferRHI.SafeRelease();
		SRVUint.SafeRelease();
		SRVFloat.SafeRelease();
		SRVRGBA.SafeRelease();
		SRVRGBA_Uint.SafeRelease();
	}
};
TGlobalResource<FDummyCulledDispatchVertexIdsBuffer> GDummyCulledDispatchVertexIdsBuffer;

/////////////////////////////////////////////////////////////////////////////////////////

FHairGroupPublicData::FVertexFactoryInput ComputeHairStrandsVertexInputData(const FHairGroupInstance* Instance);
int GetHairRaytracingProceduralSplits();
FHairStrandsVertexFactoryUniformShaderParameters FHairGroupInstance::GetHairStandsUniformShaderParameters() const
{
	const FHairGroupPublicData::FVertexFactoryInput VFInput = ComputeHairStrandsVertexInputData(this);

	FHairStrandsVertexFactoryUniformShaderParameters Out = {};
	Out.GroupIndex					= Debug.GroupIndex;
	Out.Radius 						= VFInput.Strands.HairRadius;
	Out.RootScale 					= VFInput.Strands.HairRootScale;
	Out.TipScale 					= VFInput.Strands.HairTipScale;
	Out.RaytracingRadiusScale		= VFInput.Strands.HairRaytracingRadiusScale;
	Out.RaytracingProceduralSplits  = GetHairRaytracingProceduralSplits();
	Out.Length 						= VFInput.Strands.HairLength;
	Out.Density 					= VFInput.Strands.HairDensity;
	Out.StableRasterization			= VFInput.Strands.bUseStableRasterization;
	Out.ScatterSceneLighing			= VFInput.Strands.bScatterSceneLighting;
	Out.PositionBuffer				= VFInput.Strands.PositionBufferRHISRV;
	Out.PreviousPositionBuffer		= VFInput.Strands.PrevPositionBufferRHISRV;
	Out.Attribute0Buffer			= VFInput.Strands.Attribute0BufferRHISRV;
	Out.Attribute1Buffer			= VFInput.Strands.Attribute1BufferRHISRV;
	Out.MaterialBuffer 				= VFInput.Strands.MaterialBufferRHISRV;
	Out.HasMaterial 				= Out.MaterialBuffer != nullptr;
	Out.TangentBuffer 				= VFInput.Strands.TangentBufferRHISRV;
	Out.PositionOffsetBuffer 		= VFInput.Strands.PositionOffsetBufferRHISRV;
	Out.PreviousPositionOffsetBuffer= VFInput.Strands.PrevPositionOffsetBufferRHISRV;

	// swap in some default data for those buffers that are not valid yet
	if (!Out.PositionBuffer) 				{ Out.PositionBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }
	if (!Out.PreviousPositionBuffer) 		{ Out.PreviousPositionBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }
	if (!Out.Attribute0Buffer) 				{ Out.Attribute0Buffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }
	if (!Out.Attribute1Buffer) 				{ Out.Attribute1Buffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }
	if (!Out.MaterialBuffer) 				{ Out.MaterialBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVRGBA; }
	if (!Out.TangentBuffer) 				{ Out.TangentBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }
	if (!Out.PositionOffsetBuffer) 			{ Out.PositionOffsetBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }
	if (!Out.PreviousPositionOffsetBuffer) 	{ Out.PreviousPositionOffsetBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }

	Out.CullingEnable = HairGroupPublicData->GetCullingResultAvailable();
	if (Out.CullingEnable)
	{
		Out.CulledVertexIdsBuffer = HairGroupPublicData->GetCulledVertexIdBuffer().SRV;
		Out.CulledVertexRadiusScaleBuffer = HairGroupPublicData->GetCulledVertexRadiusScaleBuffer().SRV;
	}
	else
	{
		Out.CulledVertexIdsBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVUint;
		Out.CulledVertexRadiusScaleBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat;
	}
	return Out;
}


IMPLEMENT_UNIFORM_BUFFER_STRUCT(FHairStrandsVertexFactoryUniformShaderParameters, "HairStrandsVF");

class HAIRSTRANDSCORE_API FHairStrandsVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FHairStrandsVertexFactoryShaderParameters, NonVirtual);
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
	}

	void GetElementShaderBindings(
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
		const FHairStrandsVertexFactory* VF = static_cast<const FHairStrandsVertexFactory*>(VertexFactory);
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FHairStrandsVertexFactoryUniformShaderParameters>(), VF->Data.Instance->Strands.UniformBuffer);
	}
};

IMPLEMENT_TYPE_LAYOUT(FHairStrandsVertexFactoryShaderParameters);

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
bool FHairStrandsVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return (Parameters.MaterialParameters.MaterialDomain == MD_Surface && Parameters.MaterialParameters.bIsUsedWithHairStrands && IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform)) || Parameters.MaterialParameters.bIsSpecialEngineMaterial;
}

void FHairStrandsVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	bool bUseGPUSceneAndPrimitiveIdStream = false;
#if VF_STRANDS_SUPPORT_GPU_SCENE
	bUseGPUSceneAndPrimitiveIdStream = 
		Parameters.VertexFactoryType->SupportsPrimitiveIdStream() 
		&& UseGPUScene(Parameters.Platform, GetMaxSupportedFeatureLevel(Parameters.Platform))
		// TODO: support GPUScene on mobile
		&& !IsMobilePlatform(Parameters.Platform);
#endif
	const bool bUseProceduralIntersection = GetSupportHairStrandsProceduralPrimitive(Parameters.Platform);
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bUseGPUSceneAndPrimitiveIdStream);
	OutEnvironment.SetDefine(TEXT("HAIR_STRAND_MESH_FACTORY"), 1);
	OutEnvironment.SetDefine(TEXT("ENABLE_PROCEDURAL_INTERSECTOR"), bUseProceduralIntersection);
}

void FHairStrandsVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
#if VF_STRANDS_SUPPORT_GPU_SCENE
	if (Type->SupportsPrimitiveIdStream()
		&& UseGPUScene(Platform, GetMaxSupportedFeatureLevel(Platform)) 
		&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::StaticStructMetadata.GetShaderVariableName()))
	{
		OutErrors.AddUnique(*FString::Printf(TEXT("Shader attempted to bind the Primitive uniform buffer even though Vertex Factory %s computes a PrimitiveId per-instance.  This will break auto-instancing.  Shaders should use GetPrimitiveData(PrimitiveId).Member instead of Primitive.Member."), Type->GetName()));
	}
#endif
}

void FHairStrandsVertexFactory::SetData(const FDataType& InData)
{
	check(IsInRenderingThread());
	Data = InData;
	UpdateRHI();
}

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FHairStrandsVertexFactory::Copy(const FHairStrandsVertexFactory& Other)
{
	FHairStrandsVertexFactory* VertexFactory = this;
	const FDataType* DataCopy = &Other.Data;
	ENQUEUE_RENDER_COMMAND(FHairStrandsVertexFactoryCopyData)(
		[VertexFactory, DataCopy](FRHICommandListImmediate& RHICmdList)
		{
			VertexFactory->Data = *DataCopy;
		});
	BeginUpdateResourceRHI(this);
}

void FHairStrandsVertexFactory::InitResources()
{
	if (bIsInitialized)
		return;

	FVertexFactory::InitResource(); //Call VertexFactory/RenderResources::InitResource() to mark the resource as initialized();

	bIsInitialized = true;
	bNeedsDeclaration = false;

	// We create different streams based on feature level
	check(HasValidFeatureLevel());

	FVertexDeclarationElementList Elements;
#if VF_STRANDS_SUPPORT_GPU_SCENE
	if (AddPrimitiveIdStreamElement(EVertexInputStreamType::Default, Elements, 13, 0xff))
	{
		bNeedsDeclaration = true;
	}
#endif

	if (bNeedsDeclaration)
	{
		check(Streams.Num() > 0);
	}
	InitDeclaration(Elements);
	check(IsValidRef(GetDeclaration()));

	// create the buffer
	FHairStrandsVertexFactoryUniformShaderParameters Parameters = Data.Instance->GetHairStandsUniformShaderParameters();
	Data.Instance->Strands.UniformBuffer = FHairStrandsUniformBuffer::CreateUniformBufferImmediate(Parameters, UniformBuffer_MultiFrame);
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHairStrandsVertexFactory, SF_Vertex,		FHairStrandsVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHairStrandsVertexFactory, SF_Pixel,		FHairStrandsVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHairStrandsVertexFactory, SF_Compute,		FHairStrandsVertexFactoryShaderParameters);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHairStrandsVertexFactory, SF_RayHitGroup,	FHairStrandsVertexFactoryShaderParameters);
#endif

void FHairStrandsVertexFactory::InitRHI()
{
	// Nothing as the initialize runs only on first use
}

void FHairStrandsVertexFactory::ReleaseRHI()
{
	FVertexFactory::ReleaseRHI();
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FHairStrandsVertexFactory, "/Engine/Private/HairStrands/HairStrandsVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsCachingMeshDrawCommands
	| (VF_STRANDS_SUPPORT_GPU_SCENE ? EVertexFactoryFlags::SupportsPrimitiveIdStream : EVertexFactoryFlags::None)
	| EVertexFactoryFlags::SupportsRayTracing
	| (VF_STRANDS_PROCEDURAL_INTERSECTOR ? EVertexFactoryFlags::SupportsRayTracingProceduralPrimitive : EVertexFactoryFlags::None)
	| EVertexFactoryFlags::SupportsManualVertexFetch
);
