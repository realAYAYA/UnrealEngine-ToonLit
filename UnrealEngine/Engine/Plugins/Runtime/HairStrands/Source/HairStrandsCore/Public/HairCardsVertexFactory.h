// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Components.h"
#include "VertexFactory.h"
#include "RenderGraphResources.h"
#include "HairCardsDatas.h"
#include "HairStrandsInterface.h"
#include "PrimitiveSceneProxy.h"
#include "MeshBatch.h"

class FMaterial;
class FSceneView;

// Wrapper to reinterepet FRDGPooledBuffer as a FVertexBuffer
class FRDGWrapperVertexBuffer : public FVertexBuffer
{
public:
	FRDGWrapperVertexBuffer() {}
	FRDGWrapperVertexBuffer(FRDGExternalBuffer& In): ExternalBuffer(In) { check(ExternalBuffer.Buffer); }
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		check(ExternalBuffer.Buffer && ExternalBuffer.Buffer->GetRHI());
		VertexBufferRHI = ExternalBuffer.Buffer->GetRHI();
	}

	virtual void ReleaseRHI() override
	{
		VertexBufferRHI = nullptr;
	}

	FRDGExternalBuffer ExternalBuffer;
};

/**
 * A vertex factory which simply transforms explicit vertex attributes from local to world space.
 */
class HAIRSTRANDSCORE_API FHairCardsVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FHairCardsVertexFactory);
public:
	struct FDataType
	{
		FHairGroupInstance* Instance = nullptr;
		uint32 LODIndex = 0;
		EHairGeometryType GeometryType = EHairGeometryType::NoneGeometry;
	};

	FHairCardsVertexFactory(FHairGroupInstance* Instance, uint32 LODIndex, EHairGeometryType GeometryType, EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName);
	
	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);
	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);

	// Return the primitive id supported by the VF
	EPrimitiveIdMode GetPrimitiveIdMode(ERHIFeatureLevel::Type In) const;

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FDataType& InData);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FHairCardsVertexFactory& Other);

	void InitResources(FRHICommandListBase& RHICmdList);
	virtual void ReleaseResource() override;

	// FRenderResource interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
	const FDataType& GetData() const { return Data; }
	FDataType Data;
protected:

	bool bIsInitialized = false;

	FRDGWrapperVertexBuffer DeformedPositionVertexBuffer[2];
	FRDGWrapperVertexBuffer DeformedNormalVertexBuffer;

	struct FDebugName
	{
		FDebugName(const char* InDebugName)
#if !UE_BUILD_SHIPPING
			: DebugName(InDebugName)
#endif
		{}
	private:
#if !UE_BUILD_SHIPPING
		const char* DebugName;
#endif
	} DebugName;
};