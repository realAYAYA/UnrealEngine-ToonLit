// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCacheVertexFactory.cpp: Geometry Cache vertex factory implementation
=============================================================================*/

#include "GeometryCacheVertexFactory.h"
#include "GlobalRenderResources.h"
#include "MeshBatch.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "Misc/DelayedAutoRegister.h"
#include "PackedNormal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderUtils.h"

/*-----------------------------------------------------------------------------
FGeometryCacheVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGeometryCacheVertexFactoryUniformBufferParameters, "GeomCache");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGeometryCacheManualVertexFetchUniformBufferParameters, "GeomCacheMVF");

/** Shader parameters for use with TGPUSkinVertexFactory */
class FGeometryCacheVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FGeometryCacheVertexFactoryShaderParameters, NonVirtual);
public:

	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		MeshOrigin.Bind(ParameterMap, TEXT("MeshOrigin"));
		MeshExtension.Bind(ParameterMap, TEXT("MeshExtension"));
		MotionBlurDataOrigin.Bind(ParameterMap, TEXT("MotionBlurDataOrigin"));
		MotionBlurDataExtension.Bind(ParameterMap, TEXT("MotionBlurDataExtension"));
		MotionBlurPositionScale.Bind(ParameterMap, TEXT("MotionBlurPositionScale"));
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* GenericVertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		// Ensure the vertex factory matches this parameter object and cast relevant objects
		check(GenericVertexFactory->GetType() == &FGeometryCacheVertexVertexFactory::StaticType);
		const FGeometryCacheVertexVertexFactory* GCVertexFactory = static_cast<const FGeometryCacheVertexVertexFactory*>(GenericVertexFactory);

		FGeometryCacheVertexFactoryUserData* BatchData = (FGeometryCacheVertexFactoryUserData*)BatchElement.VertexFactoryUserData;

		// Check the passed in vertex buffers make sense
		checkf(BatchData->PositionBuffer->IsInitialized(), TEXT("Batch position Vertex buffer was not initialized! Name %s"), *BatchData->PositionBuffer->GetFriendlyName());
		checkf(BatchData->MotionBlurDataBuffer->IsInitialized(), TEXT("Batch motion blur data buffer was not initialized! Name %s"), *BatchData->MotionBlurDataBuffer->GetFriendlyName());

		VertexStreams.Add(FVertexInputStream(GCVertexFactory->PositionStreamIndex, 0, BatchData->PositionBuffer->VertexBufferRHI));
		VertexStreams.Add(FVertexInputStream(GCVertexFactory->MotionBlurDataStreamIndex, 0, BatchData->MotionBlurDataBuffer->VertexBufferRHI));

		ShaderBindings.Add(MeshOrigin, BatchData->MeshOrigin);
		ShaderBindings.Add(MeshExtension, BatchData->MeshExtension);
		ShaderBindings.Add(MotionBlurDataOrigin, BatchData->MotionBlurDataOrigin);
		ShaderBindings.Add(MotionBlurDataExtension, BatchData->MotionBlurDataExtension);
		ShaderBindings.Add(MotionBlurPositionScale, BatchData->MotionBlurPositionScale);

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FGeometryCacheVertexFactoryUniformBufferParameters>(), BatchData->UniformBuffer);
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FGeometryCacheManualVertexFetchUniformBufferParameters>(), BatchData->ManualVertexFetchUniformBuffer);
	}

private:
	LAYOUT_FIELD(FShaderParameter, MeshOrigin);
	LAYOUT_FIELD(FShaderParameter, MeshExtension);
	LAYOUT_FIELD(FShaderParameter, MotionBlurDataOrigin);
	LAYOUT_FIELD(FShaderParameter, MotionBlurDataExtension);
	LAYOUT_FIELD(FShaderParameter, MotionBlurPositionScale);
};

IMPLEMENT_TYPE_LAYOUT(FGeometryCacheVertexFactoryShaderParameters);

/*-----------------------------------------------------------------------------
FGPUSkinPassthroughVertexFactory
-----------------------------------------------------------------------------*/
void FGeometryCacheVertexVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	const bool bUseGPUScene = UseGPUScene(Parameters.Platform, GetMaxSupportedFeatureLevel(Parameters.Platform)) && GetMaxSupportedFeatureLevel(Parameters.Platform) > ERHIFeatureLevel::ES3_1;
	const bool bSupportsPrimitiveIdStream = Parameters.VertexFactoryType->SupportsPrimitiveIdStream();

	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bSupportsPrimitiveIdStream && bUseGPUScene);
}

void FGeometryCacheVertexVertexFactory::SetData(const FDataType& InData)
{
	SetData(FRHICommandListImmediate::Get(), InData);
}

void FGeometryCacheVertexVertexFactory::SetData(FRHICommandListBase& RHICmdList, const FDataType& InData)
{
	// The shader code makes assumptions that the color component is a FColor, performing swizzles on ES3 and Metal platforms as necessary
	// If the color is sent down as anything other than VET_Color then you'll get an undesired swizzle on those platforms
	check((InData.ColorComponent.Type == VET_None) || (InData.ColorComponent.Type == VET_Color));

	Data = InData;
	// This will call InitRHI below where the real action happens
	UpdateRHI(RHICmdList);
}

class FDefaultGeometryCacheVertexBuffer : public FVertexBuffer
{
public:
	FShaderResourceViewRHIRef SRV;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("DefaultGeometryCacheVertexBuffer"));
		VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FVector4f) * 2, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		FVector4f* DummyContents = (FVector4f*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 2, RLM_WriteOnly);
		DummyContents[0] = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[1] = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
		RHICmdList.UnlockBuffer(VertexBufferRHI);

		SRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
	}

	virtual void ReleaseRHI() override
	{
		SRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
};
TGlobalResource<FDefaultGeometryCacheVertexBuffer> GDefaultGeometryCacheVertexBuffer;

class FDummyTangentBuffer : public FVertexBuffer
{
public:
	FShaderResourceViewRHIRef SRV;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("DummyTangentBuffer"));
		VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FVector4f) * 2, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		FVector4f* DummyContents = (FVector4f*)RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 2, RLM_WriteOnly);
		DummyContents[0] = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[1] = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
		RHICmdList.UnlockBuffer(VertexBufferRHI);

		SRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI, sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);
	}

	virtual void ReleaseRHI() override
	{
		SRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}
};
TGlobalResource<FDummyTangentBuffer> GDummyTangentBuffer;

void FGeometryCacheVertexVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	// Position needs to be separate from the rest (we just theck tangents here)
	check(Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer);
	// Motion Blur data also needs to be separate from the rest
	check(Data.MotionBlurDataComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer);
	check(Data.MotionBlurDataComponent.VertexBuffer != Data.PositionComponent.VertexBuffer);

	// If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
	// then initialize PositionStream and PositionDeclaration.
	if (Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
	{
		{
			FVertexDeclarationElementList PositionOnlyStreamElements;
			PositionOnlyStreamElements.Add(AccessStreamComponent(Data.PositionComponent, 0, EVertexInputStreamType::PositionOnly));
			AddPrimitiveIdStreamElement(EVertexInputStreamType::PositionOnly, PositionOnlyStreamElements, 1, 1);
			InitDeclaration(PositionOnlyStreamElements, EVertexInputStreamType::PositionOnly);
		}

		{
			FVertexDeclarationElementList PositionAndNormalOnlyStreamElements;
			PositionAndNormalOnlyStreamElements.Add(AccessStreamComponent(Data.PositionComponent, 0, EVertexInputStreamType::PositionAndNormalOnly));
			PositionAndNormalOnlyStreamElements.Add(AccessStreamComponent(Data.TangentBasisComponents[1], 1, EVertexInputStreamType::PositionAndNormalOnly));
			AddPrimitiveIdStreamElement(EVertexInputStreamType::PositionAndNormalOnly, PositionAndNormalOnlyStreamElements, 2, 2);
			InitDeclaration(PositionAndNormalOnlyStreamElements, EVertexInputStreamType::PositionAndNormalOnly);
		}
	}

	FVertexDeclarationElementList Elements;
	if (Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
		PositionStreamIndex = Elements.Last().StreamIndex;
	}

	// only tangent,normal are used by the stream. the binormal is derived in the shader
	uint8 TangentBasisAttributes[2] = { 1, 2 };
	for (int32 AxisIndex = 0; AxisIndex < 2; AxisIndex++)
	{
		if (Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex]));
		}
	}

	if (Data.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.ColorComponent, 3));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		Elements.Add(AccessStreamComponent(NullColorComponent, 3));
	}

	if (Data.MotionBlurDataComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.MotionBlurDataComponent, 4));
	}
	else if (Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 4));
	}
	MotionBlurDataStreamIndex = Elements.Last().StreamIndex;

	if (Data.TextureCoordinates.Num())
	{
		const int32 BaseTexCoordAttribute = 5;
		for (int32 CoordinateIndex = 0; CoordinateIndex < Data.TextureCoordinates.Num(); CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}

		for (int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < MAX_STATIC_TEXCOORDS / 2; CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}
	}

	AddPrimitiveIdStreamElement(EVertexInputStreamType::Default, Elements, 13, 13);

	check(Streams.Num() > 0);
	check(PositionStreamIndex >= 0);
	check(MotionBlurDataStreamIndex >= 0);
	check(MotionBlurDataStreamIndex != PositionStreamIndex);

	InitDeclaration(Elements);

	check(IsValidRef(GetDeclaration()));
}

void FGeometryCacheVertexVertexFactory::CreateManualVertexFetchUniformBuffer(
	const FVertexBuffer* PositionBuffer,
	const FVertexBuffer* MotionBlurBuffer,
	FGeometryCacheVertexFactoryUserData& OutUserData) const
{
	CreateManualVertexFetchUniformBuffer(FRHICommandListImmediate::Get(), PositionBuffer, MotionBlurBuffer, OutUserData);
}

void FGeometryCacheVertexVertexFactory::CreateManualVertexFetchUniformBuffer(
	FRHICommandListBase& RHICmdList,
	const FVertexBuffer* PoistionBuffer, 
	const FVertexBuffer* MotionBlurBuffer,
	FGeometryCacheVertexFactoryUserData& OutUserData) const
{
	FGeometryCacheManualVertexFetchUniformBufferParameters ManualVertexFetchParameters;

	if (PoistionBuffer != NULL)
	{
		OutUserData.PositionSRV = RHICmdList.CreateShaderResourceView(PoistionBuffer->VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
		// Position will need per-component fetch since we don't have R32G32B32 pixel format
		ManualVertexFetchParameters.Position = OutUserData.PositionSRV;
	}
	else
	{
		ManualVertexFetchParameters.Position = GDefaultGeometryCacheVertexBuffer.SRV;
	}

	if (Data.TangentBasisComponents[0].VertexBuffer != NULL)
	{
		OutUserData.TangentXSRV = RHICmdList.CreateShaderResourceView(Data.TangentBasisComponents[0].VertexBuffer->VertexBufferRHI, sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);
		ManualVertexFetchParameters.TangentX = OutUserData.TangentXSRV;
	}
	else
	{
		ManualVertexFetchParameters.TangentX = GDummyTangentBuffer.SRV;
	}

	if (Data.TangentBasisComponents[1].VertexBuffer != NULL)
	{
		OutUserData.TangentZSRV = RHICmdList.CreateShaderResourceView(Data.TangentBasisComponents[1].VertexBuffer->VertexBufferRHI, sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);
		ManualVertexFetchParameters.TangentZ = OutUserData.TangentZSRV;
	}
	else
	{
		ManualVertexFetchParameters.TangentZ = GDummyTangentBuffer.SRV;
	}

	if (Data.ColorComponent.VertexBuffer)
	{
		OutUserData.ColorSRV = RHICmdList.CreateShaderResourceView(Data.ColorComponent.VertexBuffer->VertexBufferRHI, sizeof(FColor), PF_B8G8R8A8);
		ManualVertexFetchParameters.Color = OutUserData.ColorSRV;
	}
	else
	{
		OutUserData.ColorSRV = GNullColorVertexBuffer.VertexBufferSRV;
		ManualVertexFetchParameters.Color = OutUserData.ColorSRV;
	}

	if (MotionBlurBuffer)
	{
		OutUserData.MotionBlurDataSRV = RHICmdList.CreateShaderResourceView(MotionBlurBuffer->VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
		ManualVertexFetchParameters.MotionBlurData = OutUserData.MotionBlurDataSRV;
	}
	else if (PoistionBuffer != NULL)
	{
		ManualVertexFetchParameters.MotionBlurData = OutUserData.PositionSRV;
	}
	else
	{
		ManualVertexFetchParameters.MotionBlurData = GDefaultGeometryCacheVertexBuffer.SRV;
	}

	if (Data.TextureCoordinates.Num())
	{
		checkf(Data.TextureCoordinates.Num() <= 1, TEXT("We're assuming FGeometryCacheSceneProxy uses only one TextureCoordinates vertex buffer"));
		OutUserData.TexCoordsSRV = RHICmdList.CreateShaderResourceView(Data.TextureCoordinates[0].VertexBuffer->VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
		// TexCoords will need per-component fetch since we don't have R32G32 pixel format
		ManualVertexFetchParameters.TexCoords = OutUserData.TexCoordsSRV;
	}
	else
	{
		ManualVertexFetchParameters.TexCoords = GDefaultGeometryCacheVertexBuffer.SRV;
	}

	OutUserData.ManualVertexFetchUniformBuffer = FGeometryCacheManualVertexFetchUniformBufferParametersRef::CreateUniformBufferImmediate(ManualVertexFetchParameters, UniformBuffer_SingleFrame);
}

bool FGeometryCacheVertexVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	// Should this be platform or mesh type based? Returning true should work in all cases, but maybe too expensive? 
	// return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsConsolePlatform(Platform);
	// TODO currently GeomCache supports only 4 UVs which could cause compilation errors when trying to compile shaders which use > 4
	return Parameters.MaterialParameters.bIsUsedWithGeometryCache || Parameters.MaterialParameters.bIsSpecialEngineMaterial;
}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGeometryCacheVertexVertexFactory, SF_Vertex, FGeometryCacheVertexFactoryShaderParameters);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGeometryCacheVertexVertexFactory, SF_RayHitGroup, FGeometryCacheVertexFactoryShaderParameters);
#endif
IMPLEMENT_VERTEX_FACTORY_TYPE(FGeometryCacheVertexVertexFactory, "/Engine/Private/GeometryCacheVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPositionOnly
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
);
