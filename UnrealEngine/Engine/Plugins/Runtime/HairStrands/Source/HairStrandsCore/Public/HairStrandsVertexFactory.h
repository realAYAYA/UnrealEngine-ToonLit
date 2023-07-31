// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Components.h"
#include "VertexFactory.h"
#include "RenderGraphResources.h"
#include "HairStrandsDatas.h"
#include "HairStrandsRendering.h"
#include "HairStrandsInterface.h"
#include "PrimitiveSceneProxy.h"

class FMaterial;
class FSceneView;
struct FMeshBatchElement;

/**
 * A vertex factory which simply transforms explicit vertex attributes from local to world space.
 */
class HAIRSTRANDSCORE_API FHairStrandsVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FHairStrandsVertexFactory);
public:
	
	struct FDataType
	{
		FHairGroupInstance* Instance = nullptr;
	};

	FHairStrandsVertexFactory(FHairGroupInstance* Instance, ERHIFeatureLevel::Type InFeatureLevel, const char* InDebugName)
		: FVertexFactory(InFeatureLevel)
		, DebugName(InDebugName)
	{		
		Data.Instance = Instance;
	}

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FDataType& InData);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FHairStrandsVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	void InitResources();

	const FDataType& GetData() const { return Data; }
	FDataType Data;
protected:
	bool bIsInitialized = false;

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