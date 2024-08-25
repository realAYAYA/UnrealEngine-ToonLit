// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StrandHairVertexFactory.cpp: Strand hair vertex factory implementation
=============================================================================*/

#include "HairStrandsVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "MeshDrawShaderBindings.h"
#include "ShaderParameterUtils.h"
#include "Rendering/ColorVertexBuffer.h"
#include "MaterialDomain.h"
#include "MeshMaterialShader.h"
#include "HairStrandsInterface.h"
#include "GroomInstance.h"
#include "GroomVisualizationData.h"
#include "DataDrivenShaderPlatformInfo.h"

#define VF_STRANDS_SUPPORT_GPU_SCENE 1
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

	FShaderResourceViewRHIRef SRVByteAddress;
	FBufferRHIRef ByteAddressBufferRHI;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		const static FLazyName ClassName(TEXT("FDummyCulledDispatchVertexIdsBuffer"));
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FDummyCulledDispatchVertexIdsBuffer"));
			CreateInfo.ClassName = ClassName;
			uint32 NumBytes = sizeof(uint32) * 4;
			VertexBufferRHI = RHICmdList.CreateBuffer(NumBytes, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
			uint32* DummyContents = (uint32*)RHICmdList.LockBuffer(VertexBufferRHI, 0, NumBytes, RLM_WriteOnly);
			DummyContents[0] = DummyContents[1] = DummyContents[2] = DummyContents[3] = 0;
			RHICmdList.UnlockBuffer(VertexBufferRHI);
		}

		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FDummyByteAddressBuffer"));
			CreateInfo.ClassName = ClassName;
			uint32 NumBytes = sizeof(uint32) * 4;
			ByteAddressBufferRHI = RHICmdList.CreateBuffer(NumBytes, BUF_Static | BUF_ShaderResource |BUF_ByteAddressBuffer, 0, ERHIAccess::SRVMask, CreateInfo);
			uint32* DummyContents = (uint32*)RHICmdList.LockBuffer(ByteAddressBufferRHI, 0, NumBytes, RLM_WriteOnly);
			DummyContents[0] = DummyContents[1] = DummyContents[2] = DummyContents[3] = 0;
			RHICmdList.UnlockBuffer(ByteAddressBufferRHI);
		}

		SRVUint = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_UINT);
		SRVFloat = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_FLOAT);
		SRVRGBA = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R8G8B8A8);
		SRVRGBA_Uint = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R8G8B8A8_UINT);
		SRVByteAddress = RHICmdList.CreateShaderResourceView(ByteAddressBufferRHI, sizeof(uint32), PF_R32_UINT);
	}

	virtual void ReleaseRHI() override
	{
		ByteAddressBufferRHI.SafeRelease();
		SRVByteAddress.SafeRelease();

		VertexBufferRHI.SafeRelease();
		SRVUint.SafeRelease();
		SRVFloat.SafeRelease();
		SRVRGBA.SafeRelease();
		SRVRGBA_Uint.SafeRelease();
	}
};
TGlobalResource<FDummyCulledDispatchVertexIdsBuffer> GDummyCulledDispatchVertexIdsBuffer;

/////////////////////////////////////////////////////////////////////////////////////////

FHairGroupPublicData::FVertexFactoryInput ComputeHairStrandsVertexInputData(const FHairGroupInstance* Instance, EGroomViewMode ViewMode);
uint32 GetHairRaytracingProceduralSplits();
FHairStrandsVertexFactoryUniformShaderParameters FHairGroupInstance::GetHairStandsUniformShaderParameters(EGroomViewMode ViewMode) const
{
	const FHairGroupPublicData::FVertexFactoryInput VFInput = ComputeHairStrandsVertexInputData(this, ViewMode);

	FHairStrandsVertexFactoryUniformShaderParameters Out = {};
	Out.Common = VFInput.Strands.Common;

	Out.Resources.PositionBuffer					= VFInput.Strands.PositionBufferRHISRV;
	Out.Resources.PositionOffsetBuffer 				= VFInput.Strands.PositionOffsetBufferRHISRV;
	Out.Resources.PointAttributeBuffer				= VFInput.Strands.PointAttributeBufferRHISRV;
	Out.Resources.CurveAttributeBuffer				= VFInput.Strands.CurveAttributeBufferRHISRV;
	Out.Resources.CurveBuffer						= VFInput.Strands.CurveBufferRHISRV;
	Out.Resources.PointToCurveBuffer				= VFInput.Strands.PointToCurveBufferRHISRV;
	Out.Resources.TangentBuffer 					= VFInput.Strands.TangentBufferRHISRV;
	Out.PrevResources.PreviousPositionBuffer		= VFInput.Strands.PrevPositionBufferRHISRV;
	Out.PrevResources.PreviousPositionOffsetBuffer	= VFInput.Strands.PrevPositionOffsetBufferRHISRV;

	// swap in some default data for those buffers that are not valid yet
	if (!Out.Resources.PositionBuffer) 						{ Out.Resources.PositionBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVByteAddress; }
	if (!Out.Resources.PositionOffsetBuffer) 				{ Out.Resources.PositionOffsetBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }
	if (!Out.Resources.CurveAttributeBuffer) 				{ Out.Resources.CurveAttributeBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVByteAddress; }
	if (!Out.Resources.PointAttributeBuffer) 				{ Out.Resources.PointAttributeBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVByteAddress; }
	if (!Out.Resources.CurveBuffer) 						{ Out.Resources.CurveBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVByteAddress; }
	if (!Out.Resources.PointToCurveBuffer) 					{ Out.Resources.PointToCurveBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVByteAddress; }
	if (!Out.Resources.TangentBuffer) 						{ Out.Resources.TangentBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }

	if (!Out.PrevResources.PreviousPositionBuffer) 			{ Out.PrevResources.PreviousPositionBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVByteAddress; }
	if (!Out.PrevResources.PreviousPositionOffsetBuffer) 	{ Out.PrevResources.PreviousPositionOffsetBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVFloat; }

	Out.Culling.bCullingEnable = HairGroupPublicData->GetCullingResultAvailable();
	if (Out.Culling.bCullingEnable)
	{
		Out.Culling.CullingIndexBuffer = HairGroupPublicData->GetCulledVertexIdBuffer().SRV;
	}
	else
	{
		Out.Culling.CullingIndexBuffer = GDummyCulledDispatchVertexIdsBuffer.SRVUint;
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
		&& !IsMobilePlatform(Platform) // On mobile VS may use PrimtiveUB while GPUScene is enabled
		&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName()))
	{
		OutErrors.AddUnique(*FString::Printf(TEXT("Shader attempted to bind the Primitive uniform buffer even though Vertex Factory %s computes a PrimitiveId per-instance.  This will break auto-instancing.  Shaders should use GetPrimitiveData(PrimitiveId).Member instead of Primitive.Member."), Type->GetName()));
	}
#endif
}

void FHairStrandsVertexFactory::SetData(const FDataType& InData)
{
	Data = InData;
	UpdateRHI(FRHICommandListImmediate::Get());
}

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FHairStrandsVertexFactory::Copy(const FHairStrandsVertexFactory& Other)
{
	FHairStrandsVertexFactory* VertexFactory = this;
	const FDataType* DataCopy = &Other.Data;
	ENQUEUE_RENDER_COMMAND(FHairStrandsVertexFactoryCopyData)(/*UE::RenderCommandPipe::Groom,*/
		[VertexFactory, DataCopy](FRHICommandListImmediate& RHICmdList)
		{
			VertexFactory->Data = *DataCopy;
		});
	BeginUpdateResourceRHI(this);
}

void FHairStrandsVertexFactory::InitResources(FRHICommandListBase& RHICmdList)
{
	if (bIsInitialized)
		return;

	FVertexFactory::InitResource(RHICmdList); //Call VertexFactory/RenderResources::InitResource() to mark the resource as initialized();

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
	FHairStrandsVertexFactoryUniformShaderParameters Parameters = Data.Instance->GetHairStandsUniformShaderParameters(EGroomViewMode::None);
	Data.Instance->Strands.UniformBuffer = FHairStrandsUniformBuffer::CreateUniformBufferImmediate(Parameters, UniformBuffer_MultiFrame);
}

void FHairStrandsVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
#if VF_STRANDS_SUPPORT_GPU_SCENE
	Elements.Add(FVertexElement(0, 0, VET_UInt, 13, sizeof(uint32), true));
#endif
}

EPrimitiveIdMode FHairStrandsVertexFactory::GetPrimitiveIdMode(ERHIFeatureLevel::Type In) const
{
	return PrimID_ForceZero;
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHairStrandsVertexFactory, SF_Vertex,		FHairStrandsVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHairStrandsVertexFactory, SF_Pixel,		FHairStrandsVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHairStrandsVertexFactory, SF_Compute,		FHairStrandsVertexFactoryShaderParameters);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHairStrandsVertexFactory, SF_RayHitGroup,	FHairStrandsVertexFactoryShaderParameters);
#endif

void FHairStrandsVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
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
	| EVertexFactoryFlags::SupportsPSOPrecaching
);
