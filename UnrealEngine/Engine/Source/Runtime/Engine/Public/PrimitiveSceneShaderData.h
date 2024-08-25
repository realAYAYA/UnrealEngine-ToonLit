// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneDefinitions.h"
#include "Containers/StaticArray.h"
#include "InstanceUniformShaderParameters.h"
#include "LightmapUniformShaderParameters.h"

#if !UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "PrimitiveUniformShaderParameters.h"
#endif

class FPrimitiveSceneProxy;

struct FPrimitiveSceneShaderData
{
	static const uint32 DataStrideInFloat4s = PRIMITIVE_SCENE_DATA_STRIDE;

	TStaticArray<FVector4f, DataStrideInFloat4s> Data;

	FPrimitiveSceneShaderData()
		: Data(InPlace, NoInit)
	{
		Setup(GetIdentityPrimitiveParameters());
	}

	explicit FPrimitiveSceneShaderData(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters)
		: Data(InPlace, NoInit)
	{
		Setup(PrimitiveUniformShaderParameters);
	}

	ENGINE_API FPrimitiveSceneShaderData(const FPrimitiveSceneProxy* RESTRICT Proxy);

	/**
	 * Directly construct the data from the proxy into an output array, removing the need to construct an intermediate.
	 */
	ENGINE_API static void BuildDataFromProxy(const FPrimitiveSceneProxy* RESTRICT Proxy, FVector4f* RESTRICT OutData);

	/**
	 */
	ENGINE_API static void Setup(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters, FVector4f* RESTRICT OutData);

	ENGINE_API void Setup(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters);
};

class FSinglePrimitiveStructured : public FRenderResource
{
public:

	FSinglePrimitiveStructured()
		: ShaderPlatform(SP_NumPlatforms)
	{}

	ENGINE_API virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	virtual void ReleaseRHI() override
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		PrimitiveSceneDataBufferRHI.SafeRelease();
		PrimitiveSceneDataBufferSRV.SafeRelease();
		SkyIrradianceEnvironmentMapRHI.SafeRelease();
		SkyIrradianceEnvironmentMapSRV.SafeRelease();
		InstanceSceneDataBufferRHI.SafeRelease();
		InstanceSceneDataBufferSRV.SafeRelease();
		InstancePayloadDataBufferRHI.SafeRelease();
		InstancePayloadDataBufferSRV.SafeRelease();
		PrimitiveSceneDataTextureRHI.SafeRelease();
		PrimitiveSceneDataTextureSRV.SafeRelease();
		LightmapSceneDataBufferRHI.SafeRelease();
		LightmapSceneDataBufferSRV.SafeRelease();
		EditorVisualizeLevelInstanceDataBufferRHI.SafeRelease();
		EditorVisualizeLevelInstanceDataBufferSRV.SafeRelease();
		EditorSelectedDataBufferRHI.SafeRelease();
		EditorSelectedDataBufferSRV.SafeRelease();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	ENGINE_API void UploadToGPU(FRHICommandListBase& RHICmdList);

	EShaderPlatform ShaderPlatform=SP_NumPlatforms;

	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FPrimitiveSceneShaderData PrimitiveSceneData;
	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FInstanceSceneShaderData InstanceSceneData;
	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FLightmapSceneShaderData LightmapSceneData;

	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FBufferRHIRef PrimitiveSceneDataBufferRHI;
	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FShaderResourceViewRHIRef PrimitiveSceneDataBufferSRV;

	FBufferRHIRef SkyIrradianceEnvironmentMapRHI;
	FShaderResourceViewRHIRef SkyIrradianceEnvironmentMapSRV;

	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FBufferRHIRef InstanceSceneDataBufferRHI;
	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FShaderResourceViewRHIRef InstanceSceneDataBufferSRV;

	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FBufferRHIRef InstancePayloadDataBufferRHI;
	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FShaderResourceViewRHIRef InstancePayloadDataBufferSRV;

	FTextureRHIRef PrimitiveSceneDataTextureRHI;
	FShaderResourceViewRHIRef PrimitiveSceneDataTextureSRV;

	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FBufferRHIRef LightmapSceneDataBufferRHI;
	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FShaderResourceViewRHIRef LightmapSceneDataBufferSRV;

	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FBufferRHIRef EditorVisualizeLevelInstanceDataBufferRHI;
	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FShaderResourceViewRHIRef EditorVisualizeLevelInstanceDataBufferSRV;

	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FBufferRHIRef EditorSelectedDataBufferRHI;
	UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
	FShaderResourceViewRHIRef EditorSelectedDataBufferSRV;
};

/**
* Default Primitive data buffer.  
* This is used when the VF is used for rendering outside normal mesh passes, where there is no valid scene.
*/
extern ENGINE_API TGlobalResource<FSinglePrimitiveStructured> GIdentityPrimitiveBuffer;
UE_DEPRECATED(5.3, "Use IRendererModule::CreateSinglePrimitiveSceneUniformBuffer instead")
extern ENGINE_API TGlobalResource<FSinglePrimitiveStructured> GTilePrimitiveBuffer;
