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
#include "Materials/MaterialRenderProxy.h"
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
#include "StaticMeshSceneProxy.h"
#include "SceneViewExtension.h"
#include "Tasks/Task.h"

// This defines the number of border blocks to surround terrain by when generating lightmaps
#define TERRAIN_PATCH_EXPAND_SCALAR	1

#define LANDSCAPE_LOD_LEVELS 8
#define LANDSCAPE_MAX_SUBSECTION_NUM 2

class FLandscapeComponentSceneProxy;
enum class ERuntimeVirtualTextureMaterialType : uint8;

#if RHI_RAYTRACING
struct FLandscapeRayTracingImpl;
#endif

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
LANDSCAPE_API extern TObjectPtr<UMaterialInterface> GLayerDebugColorMaterial;
LANDSCAPE_API extern TObjectPtr<UMaterialInterface> GSelectionColorMaterial;
LANDSCAPE_API extern TObjectPtr<UMaterialInterface> GSelectionRegionMaterial;
LANDSCAPE_API extern TObjectPtr<UMaterialInterface> GMaskRegionMaterial;
LANDSCAPE_API extern TObjectPtr<UMaterialInterface> GColorMaskRegionMaterial;
LANDSCAPE_API extern TObjectPtr<UTexture2D> GLandscapeBlackTexture;
LANDSCAPE_API extern TObjectPtr<UMaterialInterface> GLandscapeLayerUsageMaterial;
LANDSCAPE_API extern TObjectPtr<UMaterialInterface> GLandscapeDirtyMaterial;
#endif


/** The uniform shader parameters for a landscape draw call. */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLandscapeUniformShaderParameters, LANDSCAPE_API)
	SHADER_PARAMETER(int32, ComponentBaseX)
	SHADER_PARAMETER(int32, ComponentBaseY)
	SHADER_PARAMETER(int32, SubsectionSizeVerts)
	SHADER_PARAMETER(int32, NumSubsections)
	SHADER_PARAMETER(int32, LastLOD)
	SHADER_PARAMETER(uint32, VirtualTexturePerPixelHeight)
	SHADER_PARAMETER(FVector4f, HeightmapTextureSize)
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

class FLandscapeVertexFactoryVertexShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FLandscapeVertexFactoryVertexShaderParameters, NonVirtual);
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	void Bind(const FShaderParameterMap& ParameterMap)
	{
	}

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
class FLandscapeVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE_API(FLandscapeVertexFactory, LANDSCAPE_API);

public:

	LANDSCAPE_API FLandscapeVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);

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
	static LANDSCAPE_API bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static LANDSCAPE_API void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	
	/**
	* Get vertex elements used when during PSO precaching materials using this vertex factory type
	*/
	static LANDSCAPE_API void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);

	/**
	* Copy the data from another vertex factory
	* @param Other - factory to copy from
	*/
	LANDSCAPE_API void Copy(const FLandscapeVertexFactory& Other);

	// FRenderResource interface.
	LANDSCAPE_API virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseResource() override final { FVertexFactory::ReleaseResource(); }

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const FDataType& InData)
	{
		Data = InData;
		UpdateRHI(FRHICommandListImmediate::Get());
	}

	/** stream component data bound to this vertex factory */
	FDataType Data;
};


/** vertex factory for VTF-heightmap terrain  */
class FLandscapeXYOffsetVertexFactory : public FLandscapeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE_API(FLandscapeXYOffsetVertexFactory, LANDSCAPE_API);

public:
	FLandscapeXYOffsetVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLandscapeVertexFactory(InFeatureLevel)
	{
	}

	virtual ~FLandscapeXYOffsetVertexFactory() {}

	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};


/** Vertex factory for fixed grid runtime virtual texture lod  */
class FLandscapeFixedGridVertexFactory : public FLandscapeVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE_API(FLandscapeFixedGridVertexFactory, LANDSCAPE_API);

public:
	FLandscapeFixedGridVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLandscapeVertexFactory(InFeatureLevel)
	{
	}

	static LANDSCAPE_API void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};


struct FLandscapeVertex
{
	uint8 VertexX;
	uint8 VertexY;
	uint8 SubX;
	uint8 SubY;
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
	FLandscapeVertexBuffer(FRHICommandListBase& RHICmdList, ERHIFeatureLevel::Type InFeatureLevel, int32 InNumVertices, int32 InSubsectionSizeVerts, int32 InNumSubsections, const FName& InOwnerName)
		: FeatureLevel(InFeatureLevel)
		, NumVertices(InNumVertices)
		, SubsectionSizeVerts(InSubsectionSizeVerts)
		, NumSubsections(InNumSubsections)
	{
		SetOwnerName(InOwnerName);
		InitResource(RHICmdList);
	}

	/** Destructor. */
	virtual ~FLandscapeVertexBuffer()
	{
		ReleaseResource();
	}

	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
};

//
// FLandscapeSharedBuffers
//
class FLandscapeSharedBuffers : public FRefCountedObject
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
	
	FRenderResource* TileMesh;
	FLandscapeVertexFactory* TileVertexFactory;
	FVertexBuffer* TileDataBuffer;
	
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

	LANDSCAPE_API FLandscapeSharedBuffers(FRHICommandListBase& RHICmdList, int32 SharedBuffersKey, int32 SubsectionSizeQuads, int32 NumSubsections, ERHIFeatureLevel::Type FeatureLevel, const FName& OwnerName = NAME_None);

	template <typename INDEX_TYPE>
	void CreateIndexBuffers(FRHICommandListBase& RHICmdList, const FName& OwnerName);
	
#if WITH_EDITOR
	template <typename INDEX_TYPE>
	void CreateGrassIndexBuffer(FRHICommandListBase& RHICmdList, const FName& InOwnerName);
#endif

	LANDSCAPE_API virtual ~FLandscapeSharedBuffers();
};

//
// FLandscapeSectionInfo
//

class FLandscapeSectionInfo : public TIntrusiveLinkedList<FLandscapeSectionInfo>
{
public:
	FLandscapeSectionInfo(const UWorld* InWorld, const FGuid& InLandscapeGuid, const FIntPoint& InComponentBase, uint32 LODGroupKey);
	virtual ~FLandscapeSectionInfo() = default;

	virtual float ComputeLODForView(const FSceneView& InView) const = 0;
	virtual float ComputeLODBias() const = 0;
	virtual int32 GetSectionPriority() const { return INDEX_NONE; }

	/** Computes the worldspace units per vertex of the landscape section. */
	virtual double ComputeSectionResolution() const { return -1.0; }
	
	virtual void GetSectionBoundsAndLocalToWorld(FBoxSphereBounds& LocalBounds, FMatrix& LocalToWorld) const = 0;

	/* return the resolution of a component, in vertices (-1 for any sections that are not grid based, i.e. mesh sections) */
	virtual int32 GetComponentResolution() const { return -1; }

	/* Used to notify derived classes when render coords are calculated */
	virtual void OnRenderCoordsChanged() = 0;

public:
	uint32 LandscapeKey;					// a hash of the world and (LandscapeGUID or LOD Group Key)
	uint32 LODGroupKey;						// LOD Group Key (0 if no group)
	FIntPoint RenderCoord;					// coordinate in the RenderSystem
	FIntPoint ComponentBase;				// component base coordinate (relative to the ALandscape actor)

	bool bResourcesCreated;
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

	//  Resolution, Origin and Size, for use in LOD Groups to verify that all landscapes are of matching resolutions, orientation and scale
	int32 ComponentResolution = -1;
	FVector ComponentOrigin = FVector::ZeroVector;		// world space position of the center of the origin component (render coord 0,0)
	FVector ComponentXVector = FVector::ZeroVector;		// world space vector in the direction of component local X
	FVector ComponentYVector = FVector::ZeroVector;		// world space vector in the direction of component local Y

	// Counter used to reduce how often we call compact on the map when removing sections
	int32 SectionsRemovedSinceLastCompact;

	FLandscapeRenderSystem();
	~FLandscapeRenderSystem();

	static void CreateResources(FLandscapeSectionInfo* SectionInfo);
	static void DestroyResources(FLandscapeSectionInfo* SectionInfo);

	static void RegisterSection(FLandscapeSectionInfo* SectionInfo);
	static void UnregisterSection(FLandscapeSectionInfo* SectionInfo);

	bool IsValidCoord(FIntPoint InRenderCoord) const
	{
		return	InRenderCoord.X >= Min.X && InRenderCoord.X < Min.X + Size.X &&
				InRenderCoord.Y >= Min.Y && InRenderCoord.Y < Min.Y + Size.Y;
	}

	int32 GetSectionLinearIndex(FIntPoint InRenderCoord) const
	{
		check(IsValidCoord(InRenderCoord));
		int32 LinearIndex = (InRenderCoord.Y - Min.Y) * Size.X + InRenderCoord.X - Min.X;
		return LinearIndex;
	}
	
	void ResizeAndMoveTo(FIntPoint NewMin, FIntPoint NewMax);
	void ResizeToInclude(const FIntPoint& NewCoord);
	void CompactMap();
	bool AnySectionsInRangeInclusive(FIntPoint RangeMin, FIntPoint RangeMax);

	void SetSectionInfo(FIntPoint InRenderCoord, FLandscapeSectionInfo* InSectionInfo)
	{
		SectionInfos[GetSectionLinearIndex(InRenderCoord)] = InSectionInfo;
	}

	FLandscapeSectionInfo* GetSectionInfo(FIntPoint InRenderCoord)
	{
		return SectionInfos[GetSectionLinearIndex(InRenderCoord)];
	}

	float GetSectionLODValue(const FSceneView& SceneView, FIntPoint InRenderCoord) const
	{
		return CachedSectionLODValues[SceneView.GetViewKey()][GetSectionLinearIndex(InRenderCoord)];
	}

	float GetSectionLODBias(FIntPoint InRenderCoord) const
	{
		return SectionLODBiases[GetSectionLinearIndex(InRenderCoord)];
	}

	const TResourceArray<float>& ComputeSectionsLODForView(const FSceneView& InView);
	void FetchHeightmapLODBiases();
	void UpdateBuffers(FRHICommandListBase& RHICmdList);

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

	void EndFrame_GameThread();
	void EndFrame_RenderThread();

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PreInitViews_RenderThread(FRDGBuilder& GraphBuilder) override;

	LANDSCAPE_API const TMap<uint32, FLandscapeRenderSystem*>& GetLandscapeRenderSystems() const;
	int32 GetNumViewsWithShowCollision() const { return NumViewsWithShowCollision; }
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
	int32 NumViewsWithShowCollision = 0;    // Last frame number of views with collision enabled.
	int32 NumViewsWithShowCollisionAcc = 0; // Accumulate the number of views with collision
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
struct FLandscapeDebugOptions
{
	LANDSCAPE_API FLandscapeDebugOptions();

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

	FLandscapeMeshProxySceneProxy(UStaticMeshComponent* InComponent, const FGuid& InLandscapeGuid, const TArray<FIntPoint>& InProxySectionsBases, int8 InProxyLOD, uint32 InLODGroupKey);
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
class FLandscapeComponentSceneProxy : public FPrimitiveSceneProxy, public FLandscapeSectionInfo
{
	friend class FLandscapeSharedBuffers;

	LANDSCAPE_API SIZE_T GetTypeHash() const override;
	class FLandscapeLCI final : public FLightCacheInterface
	{
	public:
		/** Initialization constructor. */
		FLandscapeLCI(const ULandscapeComponent* InComponent, ERHIFeatureLevel::Type FeatureLevel, bool bVFRequiresPrimitiveUniformBuffer)
			: FLightCacheInterface()
		{
			const FMeshMapBuildData* MapBuildData = InComponent->GetMeshMapBuildData();

			if (MapBuildData)
			{
				SetLightMap(MapBuildData->LightMap);
				SetShadowMap(MapBuildData->ShadowMap);
				SetResourceCluster(MapBuildData->ResourceCluster);
				// If landscape uses VF that requires primitive UB that means it does not use GPUScene therefore it may need precomputed lighting buffer as well
				if (FeatureLevel >= ERHIFeatureLevel::SM5 && !bVFRequiresPrimitiveUniformBuffer)
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
	TPimplPtr<FLandscapeRayTracingImpl> RayTracingImpl;
#endif

	friend FLandscapeRenderSystem;

	// Reference counted vertex and index buffer shared among all landscape scene proxies of the same component size
	// Key is the component size and number of subsections.
	// Also being reused by GPULightmass currently to save mem
	static LANDSCAPE_API TMap<uint32, FLandscapeSharedBuffers*> SharedBuffersMap;

protected:
	int8						MaxLOD;						// Maximum LOD level, user override possible
	int8						NumWeightmapLayerAllocations;
	uint8						StaticLightingLOD;
	uint8						VirtualTexturePerPixelHeight;
	float						WeightmapSubsectionOffset;
	TArray<float>				LODScreenRatioSquared;		// Table of valid screen size -> LOD index
	int32						FirstLOD;					// First LOD we have batch elements for
	int32						LastLOD;					// Last LOD we have batch elements for
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

	const ULandscapeComponent*	LandscapeComponent;

	FMatrix						LocalToWorldNoScaling;

	TArray<FVector>				SubSectionScreenSizeTestingPosition;	// Precomputed sub section testing position for screen size calculation

	// Storage for static draw list batch params
	TArray<FLandscapeBatchElementParams> StaticBatchParamArray;

	bool bNaniteActive;
	bool bUsesLandscapeCulling;


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
	FVector4f HeightmapScaleBias;
	float HeightmapSubsectionOffsetU;
	float HeightmapSubsectionOffsetV;

	UTexture2D* XYOffsetmapTexture;

	uint32						SharedBuffersKey;
	FLandscapeSharedBuffers*	SharedBuffers;
	FLandscapeVertexFactory*	VertexFactory;
	FLandscapeVertexFactory*	FixedGridVertexFactory;

	/** All available materials, including LOD Material, Tessellation generated materials*/
	TArray<FMaterialRenderProxy*> AvailableMaterials;

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
	LANDSCAPE_API virtual ~FLandscapeComponentSceneProxy();
	
	LANDSCAPE_API int8 GetLODFromScreenSize(float InScreenSizeSquared, float InViewLODScale) const;

	LANDSCAPE_API bool GetMeshElementForVirtualTexture(int32 InLodIndex, ERuntimeVirtualTextureMaterialType MaterialType, FMaterialRenderProxy* InMaterialInterface, FMeshBatch& OutMeshBatch, TArray<FLandscapeBatchElementParams>& OutStaticBatchParamArray) const;
	template<class ArrayType> bool GetStaticMeshElement(int32 LODIndex, bool bForToolMesh, FMeshBatch& MeshBatch, ArrayType& OutStaticBatchParamArray) const;

public:
	// constructor
	LANDSCAPE_API FLandscapeComponentSceneProxy(ULandscapeComponent* InComponent);

	// FPrimitiveSceneProxy interface.
	LANDSCAPE_API virtual void ApplyWorldOffset(FVector InOffset) override;
	LANDSCAPE_API virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	LANDSCAPE_API virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	LANDSCAPE_API virtual void ApplyViewDependentMeshArguments(const FSceneView& View, FMeshBatch& ViewDependentMeshBatch) const override;
	virtual uint32 GetMemoryFootprint() const override { return(sizeof(*this) + GetAllocatedSize()); }
	LANDSCAPE_API virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	LANDSCAPE_API virtual bool CanBeOccluded() const override;
	LANDSCAPE_API virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override;
	LANDSCAPE_API virtual void OnTransformChanged() override;
	LANDSCAPE_API virtual void CreateRenderThreadResources() override;
	LANDSCAPE_API virtual void DestroyRenderThreadResources() override;
	LANDSCAPE_API virtual bool OnLevelAddedToWorld_RenderThread() override;
	LANDSCAPE_API virtual void OnLevelRemovedFromWorld_RenderThread() override;
	
	friend class ULandscapeComponent;
	friend class FLandscapeVertexFactoryVertexShaderParameters;
	friend class FLandscapeXYOffsetVertexFactoryVertexShaderParameters;
	friend class FLandscapeVertexFactoryPixelShaderParameters;
	friend struct FLandscapeBatchElementParams;

#if WITH_EDITOR
	const FMeshBatch& GetGrassMeshBatch() const { return GrassMeshBatch; }
#endif

	// FLandcapeSceneProxy
	LANDSCAPE_API void ChangeComponentScreenSizeToUseSubSections_RenderThread(float InComponentScreenSizeToUseSubSections);

	LANDSCAPE_API virtual bool HeightfieldHasPendingStreaming() const override;

	LANDSCAPE_API virtual void GetHeightfieldRepresentation(UTexture2D*& OutHeightmapTexture, UTexture2D*& OutVisibilityTexture, FHeightfieldComponentDescription& OutDescription) const override;

	LANDSCAPE_API virtual void GetLCIs(FLCIArray& LCIs) override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual int32 GetLightMapResolution() const override { return LightMapResolution; }
#endif

#if RHI_RAYTRACING
	LANDSCAPE_API virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override final;
	virtual bool HasRayTracingRepresentation() const override { return true; }
	virtual bool IsRayTracingRelevant() const override { return true; }
#endif

	// FLandscapeSectionInfo interface
	LANDSCAPE_API virtual float ComputeLODForView(const FSceneView& InView) const override;
	LANDSCAPE_API virtual float ComputeLODBias() const override;
	LANDSCAPE_API virtual void OnRenderCoordsChanged() override;
	LANDSCAPE_API virtual int32 GetComponentResolution() const override;

	LANDSCAPE_API virtual double ComputeSectionResolution() const override;
	LANDSCAPE_API virtual void GetSectionBoundsAndLocalToWorld(FBoxSphereBounds& LocalBounds, FMatrix& LocalToWorld) const override;
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
