// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "HairStrandsInterface.h"

class UMeshComponent;
class UGroomComponent;
class FHairCardsVertexFactory;
class FHairStrandsVertexFactory;
enum class EGroomCacheType : uint8;

// @hair_todo: pack card ID + card UV in 32Bits alpha channel's of the position buffer:
//  * 10/10 bits for UV -> max 1024/1024 rect resolution
//  * 12 bits for cards count -> 4000 cards for a hair group
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FHairCardsVertexFactoryUniformShaderParameters, HAIRSTRANDSCORE_API)
	SHADER_PARAMETER(uint32, Flags)
	SHADER_PARAMETER(uint32, MaxVertexCount)
	SHADER_PARAMETER_SRV(Buffer<float4>, PositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, PreviousPositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, NormalsBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, UVsBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, MaterialsBuffer)

	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, DepthTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, TangentTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, TangentSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, CoverageTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, CoverageSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, AttributeTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, AttributeSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, AuxilaryDataTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, AuxilaryDataSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, MaterialTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, MaterialSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FHairCardsVertexFactoryUniformShaderParameters> FHairCardsUniformBuffer;


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FHairStrandsVertexFactoryUniformShaderParameters, HAIRSTRANDSCORE_API)
	SHADER_PARAMETER(float, Radius)
	SHADER_PARAMETER(float, RootScale)
	SHADER_PARAMETER(float, TipScale)
	SHADER_PARAMETER(float, Length)
	SHADER_PARAMETER(float, Density)
	SHADER_PARAMETER(float, RaytracingRadiusScale)
	SHADER_PARAMETER(uint32, CullingEnable)
	SHADER_PARAMETER(uint32, HasMaterial)
	SHADER_PARAMETER(uint32, StableRasterization)
	SHADER_PARAMETER(uint32, ScatterSceneLighing)
	SHADER_PARAMETER(uint32, RaytracingProceduralSplits)
	SHADER_PARAMETER(float, GroupIndex)

	SHADER_PARAMETER_SRV(Buffer<float4>, PositionOffsetBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, PreviousPositionOffsetBuffer)

	SHADER_PARAMETER_SRV(Buffer<uint4>, PositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<uint4>, PreviousPositionBuffer)

	SHADER_PARAMETER_SRV(Buffer<float2>, Attribute0Buffer)
	SHADER_PARAMETER_SRV(Buffer<uint>,   Attribute1Buffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, MaterialBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, TangentBuffer)

	SHADER_PARAMETER_SRV(Buffer<uint>, CulledVertexIdsBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, CulledVertexRadiusScaleBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FHairStrandsVertexFactoryUniformShaderParameters> FHairStrandsUniformBuffer;

enum class EHairLODSelectionType
{
	Immediate,	// Done on the rendering thread, prior to render
	Predicted,	// Predicted based on rendering->game thread feedback
	Forced		// Forced LOD value
};

enum EHairViewRayTracingMask
{
	RayTracing  = 0x1, // Visible for raytracing effects (RT shadow, RT refleciton, Lumen, ...)
	PathTracing = 0x2, // Visible for pathtracing rendering
};

// Represent/Describe data & resources of a hair group belonging to a groom
struct HAIRSTRANDSCORE_API FHairGroupInstance : public FHairStrandsInstance
{
	//////////////////////////////////////////////////////////////////////////////////////////
	// Helper struct which aggregate strands based data/resources

	struct FStrandsBase
	{
		bool HasValidData() const { return Data != nullptr && Data->GetNumPoints() > 0; }
		bool IsValid() const { return RestResource != nullptr; }

		// Data - Render & sim (rest) data
		FHairStrandsBulkData* Data = nullptr;

		// Resources - Strands rest position data for sim & render strands
		// Resources - Strands deformed position data for sim & render strands
		FHairStrandsRestResource* RestResource = nullptr;
		FHairStrandsDeformedResource* DeformedResource = nullptr;

		// Resources - Rest root data, for deforming strands attached to a skinned mesh surface
		// Resources - Deformed root data, for deforming strands attached to a skinned mesh surface
		FHairStrandsRestRootResource* RestRootResource = nullptr;
		FHairStrandsDeformedRootResource* DeformedRootResource = nullptr;

		bool HasValidRootData() const { return RestRootResource != nullptr && DeformedRootResource != nullptr; }
	};

	struct FStrandsBaseWithInterpolation : FStrandsBase
	{
		// Data - Interpolation data (weights/Id/...) for transfering sim strands (i.e. guide) motion to render strands
		// Resources - Strands deformed position data for sim & render strands
		FHairStrandsInterpolationResource* InterpolationResource = nullptr;

		EHairInterpolationType HairInterpolationType = EHairInterpolationType::NoneSkinning;

		// Indicates if culling is enabled for this hair strands data.
		bool bIsCullingEnabled = false;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Simulation
	struct FGuides : FStrandsBase
	{
		bool bIsSimulationEnable = false;
		bool bIsDeformationEnable = false;
		bool bHasGlobalInterpolation = false;
	} Guides;

	//////////////////////////////////////////////////////////////////////////////////////////
	// Strands
	struct FStrands : FStrandsBaseWithInterpolation
	{
		// Resources - Strands cluster data for culling/voxelization purpose
		FHairStrandsClusterCullingResource* ClusterCullingResource = nullptr;

		// Resources - Raytracing data when enabling (expensive) raytracing method
		#if RHI_RAYTRACING
		FHairStrandsRaytracingResource* RenRaytracingResource = nullptr;
		bool RenRaytracingResourceOwned = false;
		uint32 ViewRayTracingMask = 0u;
		float CachedHairScaledRadius = 0;
		float CachedHairRootScale = 0;
		float CachedHairTipScale = 0;
		int   CachedProceduralSplits = 0;
		#endif

		FRDGExternalBuffer DebugAttributeBuffer;
		FHairGroupInstanceModifer Modifier;

		FHairStrandsUniformBuffer UniformBuffer;
		FHairStrandsVertexFactory* VertexFactory = nullptr;
	} Strands;

	//////////////////////////////////////////////////////////////////////////////////////////
	// Cards
	struct FCards
	{
		bool IsValid() const { for (const FLOD& LOD: LODs) { if (LOD.IsValid()) { return true; } } return false; }
		bool IsValid(int32 LODIndex) const { return LODIndex >= 0 && LODIndex < LODs.Num() && LODs[LODIndex].RestResource != nullptr; }
		struct FLOD
		{
			bool IsValid() const { return RestResource != nullptr; }

			// Data
			FHairCardsBulkData* Data = nullptr;

			// Resources
			FHairCardsRestResource* RestResource = nullptr;
			FHairCardsDeformedResource* DeformedResource = nullptr;
			#if RHI_RAYTRACING
			FHairStrandsRaytracingResource* RaytracingResource = nullptr;
			bool RaytracingResourceOwned = false;
			#endif

			// Interpolation data/resources
			FHairCardsInterpolationResource* InterpolationResource = nullptr;

			FStrandsBaseWithInterpolation Guides;

			FHairCardsUniformBuffer UniformBuffer;
			FHairCardsVertexFactory* VertexFactory = nullptr;
			FHairCardsVertexFactory* GetVertexFactory() const
			{
				return VertexFactory;
			}
			void InitVertexFactory();

		};
		TArray<FLOD> LODs;
	} Cards;

	//////////////////////////////////////////////////////////////////////////////////////////
	// Meshes
	struct FMeshes
	{
		bool IsValid() const { for (const FLOD& LOD : LODs) { if (LOD.IsValid()) { return true; } } return false; }
		bool IsValid(int32 LODIndex) const { return LODIndex >= 0 && LODIndex < LODs.Num() && LODs[LODIndex].RestResource != nullptr; }
		struct FLOD
		{
			bool IsValid() const { return RestResource != nullptr; }

			// Data
			FHairMeshesBulkData* Data = nullptr;

			// Resources
			FHairMeshesRestResource* RestResource = nullptr;
			FHairMeshesDeformedResource* DeformedResource = nullptr;
			#if RHI_RAYTRACING
			FHairStrandsRaytracingResource* RaytracingResource = nullptr;
			bool RaytracingResourceOwned = false;
			#endif

			FHairCardsUniformBuffer UniformBuffer;
			FHairCardsVertexFactory* VertexFactory = nullptr;
			FHairCardsVertexFactory* GetVertexFactory() const 
			{
				return VertexFactory;
			}
			void InitVertexFactory();
		};
		TArray<FLOD> LODs;
	} Meshes;
	
	//////////////////////////////////////////////////////////////////////////////////////////
	// Debug
	struct FDebug
	{
		// Data
		EHairStrandsDebugMode	DebugMode = EHairStrandsDebugMode::NoneDebug;
		uint32					ComponentId = ~0;
		uint32					GroupIndex = ~0;
		uint32					GroupCount = 0;
		FString					GroomAssetName;
		uint32					LastFrameIndex = ~0;

		float					LODPredictedIndex = -1.f;	// Computed on the rendering-thread, readback on the game-thread during tick()
		float					LODForcedIndex = -1.f;		// Set by the game-thread, read on the rendering-thread to force a particular LOD
		EHairLODSelectionType	LODSelectionTypeForDebug = EHairLODSelectionType::Immediate; // For debug only, not used

		int32					MeshLODIndex = ~0;
		EGroomBindingMeshType	GroomBindingType;
		EGroomCacheType			GroomCacheType;
		FPrimitiveSceneProxy*	Proxy = nullptr;
		UMeshComponent*			MeshComponent = nullptr;
		FString					MeshComponentName;
		const UGroomComponent*	GroomComponentForDebug = nullptr; // For debug only, shouldn't be deferred on the rendering thread
		FTransform				RigidCurrentLocalToWorld = FTransform::Identity;
		FTransform				SkinningCurrentLocalToWorld = FTransform::Identity;
		FTransform				RigidPreviousLocalToWorld = FTransform::Identity;
		FTransform				SkinningPreviousLocalToWorld = FTransform::Identity;
		bool					bDrawCardsGuides = false;

		TSharedPtr<class IGroomCacheBuffers, ESPMode::ThreadSafe> GroomCacheBuffers;

		// Transfer
		TArray<FRWBuffer> TransferredPositions;
		FHairStrandsProjectionMeshData SourceMeshData;
		FHairStrandsProjectionMeshData TargetMeshData;

		// Resources
		FHairStrandsDebugDatas::FResources* HairDebugResource = nullptr;
	} Debug;

	FTransform				LocalToWorld = FTransform::Identity;
	FHairGroupPublicData*	HairGroupPublicData = nullptr;
	EHairGeometryType		GeometryType = EHairGeometryType::NoneGeometry;
	EHairBindingType		BindingType = EHairBindingType::NoneBinding;
	bool					bForceCards = false;
	bool					bUpdatePositionOffset = false;
	bool					bCastShadow = true;
	
	// Deformed component to extract the bone buffer 
	UMeshComponent*	 DeformedComponent = nullptr;
	
	// Section of the deformed component to be used 
	int32	 DeformedSection = INDEX_NONE;
	
	bool IsValid() const 
	{
		return Meshes.IsValid() || Cards.IsValid() || Strands.IsValid();
	}

	virtual const FBoxSphereBounds& GetBounds() const override { return Debug.Proxy->GetBounds(); }
	virtual const FBoxSphereBounds& GetLocalBounds() const { return Debug.Proxy->GetLocalBounds(); }
	virtual const FHairGroupPublicData* GetHairData() const { return HairGroupPublicData; }
	virtual const EHairGeometryType GetHairGeometry() const { return GeometryType; }

	/** Get the current local to world transform according to the internal binding type */
	FORCEINLINE const FTransform& GetCurrentLocalToWorld() const
	{
		return (BindingType == EHairBindingType::Skinning) ? Debug.SkinningCurrentLocalToWorld: 
															 Debug.RigidCurrentLocalToWorld;
	}

	/** Get the previous local to world transform according to the internal binding type */
	FORCEINLINE const FTransform& GetPreviousLocalToWorld() const
	{
		return (BindingType == EHairBindingType::Skinning) ? Debug.SkinningPreviousLocalToWorld :
															 Debug.RigidPreviousLocalToWorld;
	}

	FHairStrandsVertexFactoryUniformShaderParameters GetHairStandsUniformShaderParameters() const;
};
