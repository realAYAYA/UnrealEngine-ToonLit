// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeRender.h: New terrain rendering
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Engine/EngineTypes.h"
#include "Templates/RefCounting.h"
#include "Containers/ArrayView.h"
#include "ShaderParameters.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "LandscapeProxy.h"
#include "RendererInterface.h"
#include "MeshBatch.h"
#include "SceneManagement.h"
#include "Engine/MapBuildDataRegistry.h"
#include "LandscapeComponent.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"
#include "SceneViewExtension.h"
#include "Tasks/Task.h"

// This defines the number of border blocks to surround terrain by when generating lightmaps
#define TERRAIN_PATCH_EXPAND_SCALAR	1

#define LANDSCAPE_LOD_LEVELS 8
#define LANDSCAPE_MAX_SUBSECTION_NUM 2

class FLandscapeComponentSceneProxy;
enum class ERuntimeVirtualTextureMaterialType : uint8;

#if WITH_EDITOR
namespace ELandscapeViewMode
{
	enum Type
	{
		Invalid = -1,
		/** Color only */
		Normal = 0,
		EditLayer,
		/** Layer debug only */
		DebugLayer,
		LayerDensity,
		LayerUsage,
		LOD,
		WireframeOnTop,
		LayerContribution
	};
}

extern LANDSCAPE_API int32 GLandscapeViewMode;

namespace ELandscapeEditRenderMode
{
	enum Type
	{
		None = 0x0,
		Gizmo = 0x1,
		SelectRegion = 0x2,
		SelectComponent = 0x4,
		Select = SelectRegion | SelectComponent,
		Mask = 0x8,
		InvertedMask = 0x10, // Should not be overlapped with other bits 
		BitMaskForMask = Mask | InvertedMask,

	};
}

LANDSCAPE_API extern bool GLandscapeEditModeActive;
LANDSCAPE_API extern int32 GLandscapeEditRenderMode;
LANDSCAPE_API extern UMaterialInterface* GLayerDebugColorMaterial;
LANDSCAPE_API extern UMaterialInterface* GSelectionColorMaterial;
LANDSCAPE_API extern UMaterialInterface* GSelectionRegionMaterial;
LANDSCAPE_API extern UMaterialInterface* GMaskRegionMaterial;
LANDSCAPE_API extern UMaterialInterface* GColorMaskRegionMaterial;
LANDSCAPE_API extern UTexture2D* GLandscapeBlackTexture;
LANDSCAPE_API extern UMaterialInterface* GLandscapeLayerUsageMaterial;
LANDSCAPE_API extern UMaterialInterface* GLandscapeDirtyMaterial;
#endif


/** The uniform shader parameters for a landscape draw call. */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLandscapeUniformShaderParameters, LANDSCAPE_API)
	SHADER_PARAMETER(int32, ComponentBaseX)
	SHADER_PARAMETER(int32, ComponentBaseY)
	SHADER_PARAMETER(int32, SubsectionSizeVerts)
	SHADER_PARAMETER(int32, NumSubsections)
	SHADER_PARAMETER(int32, LastLOD)
    SHADER_PARAMETER(FVector4f, HeightmapUVScaleBias)
    SHADER_PARAMETER(FVector4f, WeightmapUVScaleBias)
    SHADER_PARAMETER(FVector4f, LandscapeLightmapScaleBias)
    SHADER_PARAMETER(FVector4f, SubsectionSizeVertsLayerUVPan)
    SHADER_PARAMETER(FVector4f, SubsectionOffsetParams)
    SHADER_PARAMETER(FVector4f, LightmapSubsectionOffsetParams)
	SHADER_PARAMETER(FMatrix44f, LocalToWorldNoScaling)
	SHADER_PARAMETER_TEXTURE(Texture2D, HeightmapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, HeightmapTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, NormalmapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, NormalmapTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, XYOffsetmapTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, XYOffsetmapTextureSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLandscapeVertexFactoryMVFParameters, LANDSCAPE_API)
	SHADER_PARAMETER(FIntPoint, SubXY)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FLandscapeVertexFactoryMVFParameters> FLandscapeVertexFactoryMVFUniformBufferRef;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLandscapeSectionLODUniformParameters, LANDSCAPE_API)
	SHADER_PARAMETER(int32, LandscapeIndex)
	SHADER_PARAMETER(FIntPoint, Min)
	SHADER_PARAMETER(FIntPoint, Size)
	SHADER_PARAMETER_SRV(Buffer<float>, SectionLODBias)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLandscapeFixedGridUniformShaderParameters, LANDSCAPE_API)
	SHADER_PARAMETER(FVector4f, LodValues)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

/* Data needed for the landscape vertex factory to set the render state for an individual batch element */
struct FLandscapeBatchElementParams
{
#if RHI_RAYTRACING
	FRHIUniformBuffer* LandscapeVertexFactoryMVFUniformBuffer;
#endif
	const TUniformBuffer<FLandscapeUniformShaderParameters>* LandscapeUniformShaderParametersResource;
	const TArray<TUniformBuffer<FLandscapeFixedGridUniformShaderParameters>>* FixedGridUniformShaderParameters;
	FUniformBufferRHIRef LandscapeSectionLODUniformParameters;
	const FLandscapeComponentSceneProxy* SceneProxy;
	int32 CurrentLOD;
};

class FLandscapeElementParamArray : public FOneFrameResource
{
public:
	TArray<FLandscapeBatchElementParams, SceneRenderingAllocator> ElementParams;
};

/** Pixel shader parameters for use with FLandscapeVertexFactory */
class FLandscapeVertexFactoryPixelShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FLandscapeVertexFactoryPixelShaderParameters, NonVirtual);
public:
	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* InView,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const;

	
	
};

/** vertex factory for VTF-heightmap terrain  */
class LANDSCAPE_API FLandscapeVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeVertexFactory);

public:

	FLandscapeVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);

	virtual ~FLandscapeVertexFactory()
	{
		// can only be destroyed from the render thread
		ReleaseResource();
	}

	struct FDataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;
	};

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory?
	*/
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	
	/**
	* Get vertex elements used when during PSO precaching materials using this vertex factory type
	*/
	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	void Copy(const FLandscapeVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseResource() override final { FVertexFactory::ReleaseResource(); }

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FDataType& InData)
	{
		Data = InData;
		UpdateRHI();
	}

	/** stream component data bound to this vertex factory */
	FDataType Data;
};


/** vertex factory for VTF-heightmap terrain  */
class FLandscapeXYOffsetVertexFactory : public FLandscapeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeXYOffsetVertexFactory);

public:
	FLandscapeXYOffsetVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLandscapeVertexFactory(InFeatureLevel)
	{
	}

	virtual ~FLandscapeXYOffsetVertexFactory() {}

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};


/** Vertex factory for fixed grid runtime virtual texture lod  */
class LANDSCAPE_API FLandscapeFixedGridVertexFactory : public FLandscapeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeFixedGridVertexFactory);

public:
	FLandscapeFixedGridVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLandscapeVertexFactory(InFeatureLevel)
	{
	}

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};


struct FLandscapeVertex
{
	float VertexX;
	float VertexY;
	float SubX;
	float SubY;
};

//
// FLandscapeVertexBuffer
//
class FLandscapeVertexBuffer final : public FVertexBuffer
{
	ERHIFeatureLevel::Type FeatureLevel;
	int32 NumVertices;
	int32 SubsectionSizeVerts;
	int32 NumSubsections;
public:

	/** Constructor. */
	FLandscapeVertexBuffer(ERHIFeatureLevel::Type InFeatureLevel, int32 InNumVertices, int32 InSubsectionSizeVerts, int32 InNumSubsections)
		: FeatureLevel(InFeatureLevel)
		, NumVertices(InNumVertices)
		, SubsectionSizeVerts(InSubsectionSizeVerts)
		, NumSubsections(InNumSubsections)
	{
		InitResource();
	}

	/** Destructor. */
	virtual ~FLandscapeVertexBuffer()
	{
		ReleaseResource();
	}

	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override;
};

//
// FLandscapeSharedBuffers
//
class LANDSCAPE_API FLandscapeSharedBuffers : public FRefCountedObject
{
public:
	struct FLandscapeIndexRanges
	{
		int32 MinIndex[LANDSCAPE_MAX_SUBSECTION_NUM][LANDSCAPE_MAX_SUBSECTION_NUM];
		int32 MaxIndex[LANDSCAPE_MAX_SUBSECTION_NUM][LANDSCAPE_MAX_SUBSECTION_NUM];
		int32 MinIndexFull;
		int32 MaxIndexFull;
	};

	int32 NumVertices;
	int32 SharedBuffersKey;
	int32 NumIndexBuffers;
	int32 SubsectionSizeVerts;
	int32 NumSubsections;

	FLandscapeVertexFactory* VertexFactory;
	FLandscapeVertexFactory* FixedGridVertexFactory;
	FLandscapeVertexBuffer* VertexBuffer;
	FIndexBuffer** IndexBuffers;
	FLandscapeIndexRanges* IndexRanges;
	bool bUse32BitIndices;
#if WITH_EDITOR
	FIndexBuffer* GrassIndexBuffer;
	TArray<int32, TInlineAllocator<8>> GrassIndexMipOffsets;
#endif

#if RHI_RAYTRACING
	TArray<FIndexBuffer*> ZeroOffsetIndexBuffers;
#endif

	FLandscapeSharedBuffers(int32 SharedBuffersKey, int32 SubsectionSizeQuads, int32 NumSubsections, ERHIFeatureLevel::Type FeatureLevel);

	template <typename INDEX_TYPE>
	void CreateIndexBuffers();
	
#if WITH_EDITOR
	template <typename INDEX_TYPE>
	void CreateGrassIndexBuffer();
#endif

	virtual ~FLandscapeSharedBuffers();
};

//
// FLandscapeSectionInfo
//

class FLandscapeSectionInfo : public TIntrusiveLinkedList<FLandscapeSectionInfo>
{
public:
	FLandscapeSectionInfo(const UWorld* InWorld, const FGuid& InLandscapeGuid, const FIntPoint& InSectionBase);
	virtual ~FLandscapeSectionInfo() = default;

	void RegisterSection();
	void UnregisterSection();

	virtual float ComputeLODForView(const FSceneView& InView) const = 0;
	virtual float ComputeLODBias() const = 0;
	virtual int32 GetSectionPriority() const { return INDEX_NONE; }

	/** Computes the worldspace units per vertex of the landscape section. */
	virtual double ComputeSectionResolution() const { return -1.0; }

public:
	uint32 LandscapeKey;
	FIntPoint ComponentBase;
	bool bRegistered;
};

struct FLandscapeRenderSystem
{
	typedef uint32 FViewKey;

	struct LODSettingsComponent
	{
		float LOD0ScreenSizeSquared;
		float LOD1ScreenSizeSquared;
		float LODOnePlusDistributionScalarSquared;
		float LastLODScreenSizeSquared;
		int8 LastLODIndex;
		int8 ForcedLOD;
		int8 DrawCollisionPawnLOD;
		int8 DrawCollisionVisibilityLOD;
	};

	static int8 GetLODFromScreenSize(LODSettingsComponent LODSettings, float InScreenSizeSquared, float InViewLODScale, float& OutFractionalLOD)
	{
		float ScreenSizeSquared = InScreenSizeSquared / InViewLODScale;
		
		if (ScreenSizeSquared <= LODSettings.LastLODScreenSizeSquared)
		{
			OutFractionalLOD = LODSettings.LastLODIndex;
			return LODSettings.LastLODIndex;
		}
		else if (ScreenSizeSquared > LODSettings.LOD1ScreenSizeSquared)
		{
			OutFractionalLOD = (LODSettings.LOD0ScreenSizeSquared - FMath::Min(ScreenSizeSquared, LODSettings.LOD0ScreenSizeSquared)) / (LODSettings.LOD0ScreenSizeSquared - LODSettings.LOD1ScreenSizeSquared);
			return 0;
		}
		else
		{
			// No longer linear fraction, but worth the cache misses
			OutFractionalLOD = 1 + FMath::LogX(LODSettings.LODOnePlusDistributionScalarSquared, LODSettings.LOD1ScreenSizeSquared / ScreenSizeSquared);
			return (int8)OutFractionalLOD;
		}
	}

	static TBitArray<> LandscapeIndexAllocator;

	int32 LandscapeIndex;

	FIntPoint Min;
	FIntPoint Size;

	TResourceArray<float> SectionLODBiases;
	TArray<FLandscapeSectionInfo*> SectionInfos;
	int32 ReferenceCount;

	FBufferRHIRef SectionLODBiasBuffer;
	FShaderResourceViewRHIRef SectionLODBiasSRV;

	FUniformBufferRHIRef SectionLODUniformBuffer;

	TMap<FViewKey, TResourceArray<float>> CachedSectionLODValues;

	/** Forced LOD level which overrides the ForcedLOD level of all the sections under this LandscapeRenderSystem. */
	int8 ForcedLODOverride;

	FLandscapeRenderSystem();
	~FLandscapeRenderSystem();

	static void CreateResources(FLandscapeSectionInfo* SectionInfo);
	static void DestroyResources(FLandscapeSectionInfo* SectionInfo);

	static void RegisterSection(FLandscapeSectionInfo* SectionInfo);
	static void UnregisterSection(FLandscapeSectionInfo* SectionInfo);

	int32 GetSectionLinearIndex(FIntPoint InSectionBase) const
	{
		return (InSectionBase.Y - Min.Y) * Size.X + InSectionBase.X - Min.X;
	}
	void ResizeAndMoveTo(FIntPoint NewMin, FIntPoint NewSize);

	void SetSectionInfo(FIntPoint InSectionBase, FLandscapeSectionInfo* InSectionInfo)
	{
		SectionInfos[GetSectionLinearIndex(InSectionBase)] = InSectionInfo;
	}

	FLandscapeSectionInfo* GetSectionInfo(FIntPoint InSectionBase)
	{
		return SectionInfos[GetSectionLinearIndex(InSectionBase)];
	}

	float GetSectionLODValue(const FSceneView& SceneView, FIntPoint InSectionBase) const
	{
		return CachedSectionLODValues[SceneView.GetViewKey()][GetSectionLinearIndex(InSectionBase)];
	}

	float GetSectionLODBias(FIntPoint InSectionBase) const
	{
		return SectionLODBiases[GetSectionLinearIndex(InSectionBase)];
	}

	const TResourceArray<float>& ComputeSectionsLODForView(const FSceneView& InView);
	void FetchHeightmapLODBiases();
	void UpdateBuffers();

private:
	void CreateResources_Internal(FLandscapeSectionInfo* InSectionInfo);
	void DestroyResources_Internal(FLandscapeSectionInfo* InSectionInfo);
};


//
// FLandscapeSceneViewExtension
//
class FLandscapeSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FLandscapeSceneViewExtension(const FAutoRegister& AutoReg);
	virtual ~FLandscapeSceneViewExtension();

	void EndFrame_RenderThread();

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PreInitViews_RenderThread(FRDGBuilder& GraphBuilder) override;

	LANDSCAPE_API const TMap<uint32, FLandscapeRenderSystem*>& GetLandscapeRenderSystems() const;
private:
	FBufferRHIRef LandscapeLODDataBuffer;
	FBufferRHIRef LandscapeIndirectionBuffer;

	struct FLandscapeViewData
	{
		FLandscapeViewData() = default;

		FLandscapeViewData(FSceneView& InView)
			: View(&InView)
		{}

		FSceneView* View = nullptr;
		TResourceArray<uint32> LandscapeIndirection;
		TResourceArray<float> LandscapeLODData;
	};

	TArray<FLandscapeViewData> LandscapeViews;
	UE::Tasks::FTask LandscapeSetupTask;
};


//
// FLandscapeVisibilityHelper
//
class FLandscapeVisibilityHelper
{
public:
	void Init(UPrimitiveComponent* LandscapeComponent, FPrimitiveSceneProxy* ProxyIn);
	bool OnAddedToWorld();
	bool OnRemoveFromWorld();
	bool ShouldBeVisible() const { return !bRequiresVisibleLevelToRender || bIsComponentLevelVisible; }
	bool RequiresVisibleLevelToRender() const { return bRequiresVisibleLevelToRender; }
private:
	bool bRequiresVisibleLevelToRender = false;
	bool bIsComponentLevelVisible = false;
};

//
// FLandscapeDebugOptions
//
struct LANDSCAPE_API FLandscapeDebugOptions
{
	FLandscapeDebugOptions();

	enum eCombineMode
	{
		eCombineMode_Default = 0,
		eCombineMode_CombineAll = 1,
		eCombineMode_Disabled = 2
	};

	bool bShowPatches;
	bool bDisableStatic;
	eCombineMode CombineMode;

private:
	FAutoConsoleCommand PatchesConsoleCommand;
	FAutoConsoleCommand StaticConsoleCommand;
	FAutoConsoleCommand CombineConsoleCommand;

	void Patches();
	void Static();
	void Combine(const TArray<FString>& Args);
};

LANDSCAPE_API extern FLandscapeDebugOptions GLandscapeDebugOptions;

//
// FLandscapeMeshProxySceneProxy
//
class FLandscapeMeshProxySceneProxy final : public FStaticMeshSceneProxy
{
public:
	SIZE_T GetTypeHash() const override;

	FLandscapeMeshProxySceneProxy(UStaticMeshComponent* InComponent, const FGuid& InLandscapeGuid, const TArray<FIntPoint>& InProxySectionsBases, int8 InProxyLOD);
	virtual void CreateRenderThreadResources() override;
	virtual void DestroyRenderThreadResources() override;
	virtual bool OnLevelAddedToWorld_RenderThread() override;
	virtual void OnLevelRemovedFromWorld_RenderThread() override;

private:
	void RegisterSections();
	void UnregisterSections();

	FLandscapeVisibilityHelper VisibilityHelper;

	TArray<TUniquePtr<FLandscapeSectionInfo>> ProxySectionsInfos;
};

//
// FLandscapeComponentSceneProxy
//
class LANDSCAPE_API FLandscapeComponentSceneProxy : public FPrimitiveSceneProxy, public FLandscapeSectionInfo
{
	friend class FLandscapeSharedBuffers;

	SIZE_T GetTypeHash() const override;
	class FLandscapeLCI final : public FLightCacheInterface
	{
	public:
		/** Initialization constructor. */
		FLandscapeLCI(const ULandscapeComponent* InComponent, ERHIFeatureLevel::Type FeatureLevel)
			: FLightCacheInterface()
		{
			const FMeshMapBuildData* MapBuildData = InComponent->GetMeshMapBuildData();

			if (MapBuildData)
			{
				SetLightMap(MapBuildData->LightMap);
				SetShadowMap(MapBuildData->ShadowMap);
				SetResourceCluster(MapBuildData->ResourceCluster);
				if (FeatureLevel >= ERHIFeatureLevel::SM5)
				{
					// Landscape does not support GPUScene on mobile
					// TODO: enable this when GPUScene support is implemented
					bCanUsePrecomputedLightingParametersFromGPUScene = true;
				}
				IrrelevantLights = MapBuildData->IrrelevantLights;
			}
		}

		// FLightCacheInterface
		virtual FLightInteraction GetInteraction(const FLightSceneProxy* LightSceneProxy) const override;

	private:
		TArray<FGuid> IrrelevantLights;
	};

public:
	static const int8 MAX_SUBSECTION_COUNT = 2*2;

#if RHI_RAYTRACING
	struct FLandscapeSectionRayTracingState
	{
		int8 CurrentLOD;
		float FractionalLOD;
		float HeightmapLODBias;
		uint32 ReferencedTextureRHIHash;

		FRayTracingGeometry Geometry;
		FRWBuffer RayTracingDynamicVertexBuffer;
		FLandscapeVertexFactoryMVFUniformBufferRef UniformBuffer;

		FLandscapeSectionRayTracingState() 
			: CurrentLOD(-1)
			, FractionalLOD(-1000.0f)
			, HeightmapLODBias(-1000.0f)
			, ReferencedTextureRHIHash(0) {}
	};

	TStaticArray<FLandscapeSectionRayTracingState, MAX_SUBSECTION_COUNT> SectionRayTracingStates;
#endif

	friend FLandscapeRenderSystem;

	// Reference counted vertex and index buffer shared among all landscape scene proxies of the same component size
	// Key is the component size and number of subsections.
	// Also being reused by GPULightmass currently to save mem
	static TMap<uint32, FLandscapeSharedBuffers*> SharedBuffersMap;

protected:
	int8						MaxLOD;		// Maximum LOD level, user override possible
	int8						NumWeightmapLayerAllocations;
	uint8						StaticLightingLOD;
	float						WeightmapSubsectionOffset;
	TArray<float>				LODScreenRatioSquared;		// Table of valid screen size -> LOD index
	int32						FirstLOD;	// First LOD we have batch elements for
	int32						LastLOD;	// Last LOD we have batch elements for
	int32						FirstVirtualTextureLOD;
	int32						LastVirtualTextureLOD;
	float						ComponentMaxExtend; 		// The max extend value in any axis
	float						ComponentSquaredScreenSizeToUseSubSections; // Size at which we start to draw in sub lod if LOD are different per sub section

	FLandscapeRenderSystem::LODSettingsComponent LODSettings;

	/** 
	 * Number of subsections within the component in each dimension, this can be 1 or 2.
	 * Subsections exist to improve the speed at which LOD transitions can take place over distance.
	 */
	int32						NumSubsections;
	/** Number of unique heights in the subsection. */
	int32						SubsectionSizeQuads;
	/** Number of heightmap heights in the subsection. This includes the duplicate row at the end. */
	int32						SubsectionSizeVerts;
	/** Size of the component in unique heights. */
	int32						ComponentSizeQuads;
	/** 
	 * ComponentSizeQuads + 1.
	 * Note: in the case of multiple subsections, this is not very useful, as there will be an internal duplicate row of heights in addition to the row at the end.
	 */
	int32						ComponentSizeVerts;
	float						StaticLightingResolution;
	/** Address of the component within the parent Landscape in unique height texels. */
	FIntPoint					SectionBase;

	const ULandscapeComponent* LandscapeComponent;

	FMatrix						LocalToWorldNoScaling;

	TArray<FVector>				SubSectionScreenSizeTestingPosition;	// Precomputed sub section testing position for screen size calculation

	// Storage for static draw list batch params
	TArray<FLandscapeBatchElementParams> StaticBatchParamArray;

	bool bNaniteActive;


#if WITH_EDITOR
	// Precomputed grass rendering MeshBatch and per-LOD params
	FMeshBatch                           GrassMeshBatch;
	TArray<FLandscapeBatchElementParams> GrassBatchParams;
#endif

	FVector4f WeightmapScaleBias;
	TArray<UTexture2D*> WeightmapTextures;

	UTexture2D* VisibilityWeightmapTexture;
	int32 VisibilityWeightmapChannel;

#if WITH_EDITOR
	TArray<FLinearColor> LayerColors;
#endif
	// Heightmap in RG and Normalmap in BA
	UTexture2D* HeightmapTexture; 
	UTexture2D* BaseColorForGITexture;
	FVector4f HeightmapScaleBias;
	float HeightmapSubsectionOffsetU;
	float HeightmapSubsectionOffsetV;

	UTexture2D* XYOffsetmapTexture;

	uint32						SharedBuffersKey;
	FLandscapeSharedBuffers*	SharedBuffers;
	FLandscapeVertexFactory*	VertexFactory;
	FLandscapeVertexFactory*	FixedGridVertexFactory;

	/** All available materials, including LOD Material, Tessellation generated materials*/
	TArray<UMaterialInterface*> AvailableMaterials;

	// FLightCacheInterface
	TUniquePtr<FLandscapeLCI> ComponentLightInfo;

	/** Mapping between LOD and Material Index*/
	TArray<int8> LODIndexToMaterialIndex;
	
	/** Mapping between Material Index to Static Mesh Batch */
	TArray<int8> MaterialIndexToStaticMeshBatchLOD;

	/** Material Relevance for each material in AvailableMaterials */
	TArray<FMaterialRelevance> MaterialRelevances;

#if WITH_EDITORONLY_DATA
	FLandscapeEditToolRenderData EditToolRenderData;
#endif

#if WITH_EDITORONLY_DATA
	ELandscapeLODFalloff::Type LODFalloff_DEPRECATED;
#endif

	// data used in editor or visualisers
#if WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	int32 CollisionMipLevel;
	int32 SimpleCollisionMipLevel;

	FCollisionResponseContainer CollisionResponse;
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** LightMap resolution used for VMI_LightmapDensity */
	int32 LightMapResolution;
#endif

	TUniformBuffer<FLandscapeUniformShaderParameters> LandscapeUniformShaderParameters;

	TArray< TUniformBuffer<FLandscapeFixedGridUniformShaderParameters> > LandscapeFixedGridUniformShaderParameters;

	// Cached versions of these
	FMatrix					WorldToLocal;

	FLandscapeVisibilityHelper VisibilityHelper;

protected:
	virtual ~FLandscapeComponentSceneProxy();
	
	int8 GetLODFromScreenSize(float InScreenSizeSquared, float InViewLODScale) const;

	bool GetMeshElementForVirtualTexture(int32 InLodIndex, ERuntimeVirtualTextureMaterialType MaterialType, UMaterialInterface* InMaterialInterface, FMeshBatch& OutMeshBatch, TArray<FLandscapeBatchElementParams>& OutStaticBatchParamArray) const;
	template<class ArrayType> bool GetStaticMeshElement(int32 LODIndex, bool bForToolMesh, FMeshBatch& MeshBatch, ArrayType& OutStaticBatchParamArray) const;
	
	virtual void ApplyMeshElementModifier(FMeshBatchElement& InOutMeshElement, int32 InLodIndex) const {}

public:
	// constructor
	FLandscapeComponentSceneProxy(ULandscapeComponent* InComponent);

	// FPrimitiveSceneProxy interface.
	virtual void ApplyWorldOffset(FVector InOffset) override;
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual uint32 GetMemoryFootprint() const override { return(sizeof(*this) + GetAllocatedSize()); }
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override;
	virtual void OnTransformChanged() override;
	virtual void CreateRenderThreadResources() override;
	virtual void DestroyRenderThreadResources() override;
	virtual bool OnLevelAddedToWorld_RenderThread() override;
	virtual void OnLevelRemovedFromWorld_RenderThread() override;
	
	friend class ULandscapeComponent;
	friend class FLandscapeVertexFactoryVertexShaderParameters;
	friend class FLandscapeXYOffsetVertexFactoryVertexShaderParameters;
	friend class FLandscapeVertexFactoryPixelShaderParameters;
	friend struct FLandscapeBatchElementParams;

#if WITH_EDITOR
	const FMeshBatch& GetGrassMeshBatch() const { return GrassMeshBatch; }
#endif

	// FLandcapeSceneProxy
	void ChangeComponentScreenSizeToUseSubSections_RenderThread(float InComponentScreenSizeToUseSubSections);

	virtual bool HeightfieldHasPendingStreaming() const override;

	virtual void GetHeightfieldRepresentation(UTexture2D*& OutHeightmapTexture, UTexture2D*& OutDiffuseColorTexture, UTexture2D*& OutVisibilityTexture, FHeightfieldComponentDescription& OutDescription) const override;

	virtual void GetLCIs(FLCIArray& LCIs) override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual int32 GetLightMapResolution() const override { return LightMapResolution; }
#endif

#if RHI_RAYTRACING
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override final;
	virtual bool HasRayTracingRepresentation() const override { return true; }
	virtual bool IsRayTracingRelevant() const override { return true; }
#endif

	// FLandscapeSectionInfo interface
	virtual float ComputeLODForView(const FSceneView& InView) const override;
	virtual float ComputeLODBias() const override;
	virtual double ComputeSectionResolution() const override;
};

class FLandscapeDebugMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* RedTexture;
	const UTexture2D* GreenTexture;
	const UTexture2D* BlueTexture;
	const FLinearColor R;
	const FLinearColor G;
	const FLinearColor B;

	/** Initialization constructor. */
	FLandscapeDebugMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* TexR, const UTexture2D* TexG, const UTexture2D* TexB,
		const FLinearColor& InR, const FLinearColor& InG, const FLinearColor& InB) :
		FMaterialRenderProxy(InParent->GetMaterialName()),
		Parent(InParent),
		RedTexture(TexR),
		GreenTexture(TexG),
		BlueTexture(TexB),
		R(InR),
		G(InG),
		B(InB)
	{}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetMaterialNoFallback(InFeatureLevel);
	}

	virtual const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetFallback(InFeatureLevel);
	}

	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const
	{
		switch (Type)
		{
		case EMaterialParameterType::Vector:
			if (ParameterInfo.Name == FName(TEXT("Landscape_RedMask")))
			{
				OutValue = R;
				return true;
			}
			else if (ParameterInfo.Name == FName(TEXT("Landscape_GreenMask")))
			{
				OutValue = G;
				return true;
			}
			else if (ParameterInfo.Name == FName(TEXT("Landscape_BlueMask")))
			{
				OutValue = B;
				return true;
			}
			break;
		case EMaterialParameterType::Texture:
			if (ParameterInfo.Name == FName(TEXT("Landscape_RedTexture")))
			{
				OutValue = RedTexture;
				return true;
			}
			else if (ParameterInfo.Name == FName(TEXT("Landscape_GreenTexture")))
			{
				OutValue = GreenTexture;
				return true;
			}
			else if (ParameterInfo.Name == FName(TEXT("Landscape_BlueTexture")))
			{
				OutValue = BlueTexture;
				return true;
			}
			break;
		default:
			break;
		}
		return Parent->GetParameterValue(Type, ParameterInfo, OutValue, Context);
	}
};

class FLandscapeSelectMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* SelectTexture;

	/** Initialization constructor. */
	FLandscapeSelectMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* InTexture) :
		FMaterialRenderProxy(InParent->GetMaterialName()),
		Parent(InParent),
		SelectTexture(InTexture)
	{}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetMaterialNoFallback(InFeatureLevel);
	}
	virtual const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetFallback(InFeatureLevel);
	}

	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const
	{
		switch (Type)
		{
		case EMaterialParameterType::Vector:
			if (ParameterInfo.Name == FName(TEXT("HighlightColor")))
			{
				OutValue = FLinearColor(1.f, 0.5f, 0.5f);
				return true;
			}
			break;
		case EMaterialParameterType::Texture:
			if (ParameterInfo.Name == FName(TEXT("SelectedData")))
			{
				OutValue = SelectTexture;
				return true;
			}
			break;
		default:
			break;
		}
		return Parent->GetParameterValue(Type, ParameterInfo, OutValue, Context);
	}
};

class FLandscapeMaskMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* SelectTexture;
	const bool bInverted;

	/** Initialization constructor. */
	FLandscapeMaskMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* InTexture, const bool InbInverted) :
		FMaterialRenderProxy(InParent->GetMaterialName()),
		Parent(InParent),
		SelectTexture(InTexture),
		bInverted(InbInverted)
	{}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetMaterialNoFallback(InFeatureLevel);
	}
	virtual const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetFallback(InFeatureLevel);
	}
	
	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const
	{
		switch (Type)
		{
		case EMaterialParameterType::Scalar:
			if (ParameterInfo.Name == FName(TEXT("bInverted")))
			{
				OutValue = (float)bInverted;
				return true;
			}
			break;
		case EMaterialParameterType::Texture:
			if (ParameterInfo.Name == FName(TEXT("SelectedData")))
			{
				OutValue = SelectTexture;
				return true;
			}
			break;
		default:
			break;
		}
		return Parent->GetParameterValue(Type, ParameterInfo, OutValue, Context);
	}
};

class FLandscapeLayerUsageRenderProxy : public FMaterialRenderProxy
{
	const FMaterialRenderProxy* const Parent;

	int32 ComponentSizeVerts;
	TArray<FLinearColor> LayerColors;
	float Rotation;
public:
	FLandscapeLayerUsageRenderProxy(const FMaterialRenderProxy* InParent, int32 InComponentSizeVerts, const TArray<FLinearColor>& InLayerColors, float InRotation)
	: FMaterialRenderProxy(InParent->GetMaterialName())
	, Parent(InParent)
	, ComponentSizeVerts(InComponentSizeVerts)
	, LayerColors(InLayerColors)
	, Rotation(InRotation)
	{}

	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetMaterialNoFallback(InFeatureLevel);
	}
	virtual const FMaterialRenderProxy* GetFallback(ERHIFeatureLevel::Type InFeatureLevel) const override
	{
		return Parent->GetFallback(InFeatureLevel);
	}
	
	virtual bool GetParameterValue(EMaterialParameterType Type, const FHashedMaterialParameterInfo& ParameterInfo, FMaterialParameterValue& OutValue, const FMaterialRenderContext& Context) const
	{
		static FName ColorNames[] =
		{
			FName(TEXT("Color0")),
			FName(TEXT("Color1")),
			FName(TEXT("Color2")),
			FName(TEXT("Color3")),
			FName(TEXT("Color4")),
			FName(TEXT("Color5")),
			FName(TEXT("Color6")),
			FName(TEXT("Color7")),
			FName(TEXT("Color8")),
			FName(TEXT("Color9")),
			FName(TEXT("Color10")),
			FName(TEXT("Color11")),
			FName(TEXT("Color12")),
			FName(TEXT("Color13")),
			FName(TEXT("Color14")),
			FName(TEXT("Color15"))
		};

		switch (Type)
		{
		case EMaterialParameterType::Vector:
			for (int32 i = 0; i < UE_ARRAY_COUNT(ColorNames) && i < LayerColors.Num(); i++)
			{
				if (ParameterInfo.Name == ColorNames[i])
				{
					OutValue = LayerColors[i];
					return true;
				}
			}
			break;
		case EMaterialParameterType::Scalar:
			if (ParameterInfo.Name == FName(TEXT("Rotation")))
			{
				OutValue = Rotation;
				return true;
			}
			else if (ParameterInfo.Name == FName(TEXT("NumStripes")))
			{
				OutValue = (float)LayerColors.Num();
				return true;
			}
			else if (ParameterInfo.Name == FName(TEXT("ComponentSizeVerts")))
			{
				OutValue = (float)ComponentSizeVerts;
				return true;
			}
			break;
		default:
			break;
		}

		return Parent->GetParameterValue(Type, ParameterInfo, OutValue, Context);
	}
};
