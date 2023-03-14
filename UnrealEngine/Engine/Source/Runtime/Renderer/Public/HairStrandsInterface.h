// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairStrandsInterface.h: public interface for hair strands rendering.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "Containers/Array.h"
#include "Engine/EngineTypes.h"
#include "Shader.h"
#include "RenderResource.h"
#include "RenderGraphResources.h"
#include "ShaderPrintParameters.h"

class UTexture2D;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Utils buffers for importing/exporting hair resources

enum class ERDGImportedBufferFlags
{
	None = 0,
	CreateSRV = 0x1,
	CreateUAV = 0x2,
	CreateViews = CreateSRV | CreateUAV
};
ENUM_CLASS_FLAGS(ERDGImportedBufferFlags);

struct RENDERER_API FRDGExternalBuffer
{
	TRefCountPtr<FRDGPooledBuffer> Buffer = nullptr;
	FShaderResourceViewRHIRef SRV = nullptr;
	FUnorderedAccessViewRHIRef UAV = nullptr;
	EPixelFormat Format = PF_Unknown;
	void Release();
};

struct RENDERER_API FRDGImportedBuffer
{
	FRDGBufferRef Buffer = nullptr;
	FRDGBufferSRVRef SRV = nullptr;
	FRDGBufferUAVRef UAV = nullptr;
};

RENDERER_API FRDGImportedBuffer Register(FRDGBuilder& GraphBuilder, const FRDGExternalBuffer& In, ERDGImportedBufferFlags Flags, ERDGUnorderedAccessViewFlags UAVFlags = ERDGUnorderedAccessViewFlags::None);
RENDERER_API FRDGBufferSRVRef   RegisterAsSRV(FRDGBuilder& GraphBuilder, const FRDGExternalBuffer& In);
RENDERER_API FRDGBufferUAVRef   RegisterAsUAV(FRDGBuilder& GraphBuilder, const FRDGExternalBuffer& In, ERDGUnorderedAccessViewFlags Flags = ERDGUnorderedAccessViewFlags::None);
RENDERER_API void				ConvertToExternalBufferWithViews(FRDGBuilder& GraphBuilder, FRDGBufferRef& InBuffer, FRDGExternalBuffer& OutBuffer, EPixelFormat Format = PF_Unknown);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Misc/Helpers

enum class EHairStrandsDebugMode : uint8
{
	NoneDebug,
	SimHairStrands,
	RenderHairStrands,
	RenderHairUV,
	RenderHairRootUV,
	RenderHairRootUDIM,
	RenderHairSeed,
	RenderHairDimension,
	RenderHairRadiusVariation,
	RenderHairTangent,
	RenderHairBaseColor,
	RenderHairRoughness,
	RenderVisCluster,
	RenderHairGroup,
	RenderLODColoration,
	RenderHairControlPoints,
	Count
};

enum class EHairDebugMode : uint8
{
	None,
	MacroGroups,
	LightBounds,
	DeepOpacityMaps,
	MacroGroupScreenRect,
	SamplePerPixel,
	CoverageType,
	TAAResolveType,
	VoxelsDensity,
	VoxelsTangent,
	VoxelsBaseColor,
	VoxelsRoughness,
	MeshProjection,
	Coverage,
	MaterialDepth,
	MaterialBaseColor,
	MaterialRoughness,
	MaterialSpecular,
	MaterialTangent,
	Tile
};

/// Return the active debug view mode
RENDERER_API EHairStrandsDebugMode GetHairStrandsDebugStrandsMode();
RENDERER_API EHairDebugMode GetHairStrandsDebugMode();

struct FHairStrandClusterCullingData;
struct IPooledRenderTarget;
struct FRWBuffer;
class  FRDGPooledBuffer;
class  FHairGroupPublicData;
class  FRDGShaderResourceView;
class  FResourceArrayInterface;
class  FSceneView;

enum EHairGeometryType
{
	Strands,
	Cards,
	Meshes,
	NoneGeometry
};

enum EHairBindingType
{
	NoneBinding,
	Rigid,
	Skinning
};

enum EHairInterpolationType
{
	NoneSkinning,				// Use no skinning data (i.e. with no binding)
	RigidSkinning,				// Use skinning data & apply a rigid triangle deformation
	OffsetSkinning,				// Use skinning data & apply a offset deformation
	SmoothSkinning,				// Use skinning data & apply a smooth (rotation) offset
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Public group data 

struct RENDERER_API FHairStrandsInstance
{
	virtual ~FHairStrandsInstance() = default;
	uint32 GetRefCount() const;
	uint32 AddRef() const;
	uint32 Release() const;
	int32 RegisteredIndex = -1;
	virtual const FBoxSphereBounds& GetBounds() const = 0;
	virtual const FBoxSphereBounds& GetLocalBounds() const = 0;
	virtual const FHairGroupPublicData* GetHairData() const { return nullptr; }
	virtual const EHairGeometryType GetHairGeometry() const { return EHairGeometryType::NoneGeometry; }
protected:
	mutable uint32 RefCount = 0;
};
typedef TArray<FHairStrandsInstance*> FHairStrandsInstances;

class RENDERER_API FHairGroupPublicData : public FRenderResource
{
public:
	FHairGroupPublicData(uint32 InGroupIndex);
	void SetClusters(uint32 InClusterCount, uint32 InVertexCount);
	
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("FHairGroupPublicData"); }
	void Allocate(FRDGBuilder& GraphBuilder);
	void Release();
	uint32 GetResourcesSize() const;

	// The primitive count when no culling and neither lod happens
	uint32 GetGroupInstanceVertexCount() const { return GroupControlTriangleStripVertexCount; }
	uint32 GetGroupControlPointCount() const { return VertexCount; }

	uint32 GetGroupIndex() const { return GroupIndex; }

	FRDGExternalBuffer& GetDrawIndirectRasterComputeBuffer() { return DrawIndirectRasterComputeBuffer; }
	const FRDGExternalBuffer& GetDrawIndirectRasterComputeBuffer() const { return DrawIndirectRasterComputeBuffer; }
	FRDGExternalBuffer& GetDrawIndirectBuffer() { return DrawIndirectBuffer; }
	FRDGExternalBuffer& GetClusterAABBBuffer() { return ClusterAABBBuffer; }
	FRDGExternalBuffer& GetGroupAABBBuffer() { return GroupAABBBuffer; }
	const FRDGExternalBuffer& GetGroupAABBBuffer() const { return GroupAABBBuffer; }

	const FRDGExternalBuffer& GetCulledVertexIdBuffer() const { return CulledVertexIdBuffer; }
	const FRDGExternalBuffer& GetCulledVertexRadiusScaleBuffer() const { return CulledVertexRadiusScaleBuffer; }

	FRDGExternalBuffer& GetCulledVertexIdBuffer() { return CulledVertexIdBuffer; }
	FRDGExternalBuffer& GetCulledVertexRadiusScaleBuffer() { return CulledVertexRadiusScaleBuffer; }

	bool GetCullingResultAvailable() const { return bCullingResultAvailable; }
	void SetCullingResultAvailable(bool b) { bCullingResultAvailable = b; }

	void SupportVoxelization(bool InVoxelize) { bSupportVoxelization = InVoxelize; }
	bool DoesSupportVoxelization() const { return bSupportVoxelization; }

	void SetLODGeometryTypes(const TArray<EHairGeometryType>& InTypes) { LODGeometryTypes = InTypes; }
	const TArray<EHairGeometryType>& GetLODGeometryTypes() const { return LODGeometryTypes; }

	void SetLODVisibilities(const TArray<bool>& InLODVisibility) { LODVisibilities = InLODVisibility; }
	const TArray<bool>& GetLODVisibilities() const { return LODVisibilities; }

	bool IsVisible(int32 InLODIndex) const
	{
		if (InLODIndex < 0 && InLODIndex >= LODVisibilities.Num()) return false;
		return LODVisibilities[InLODIndex];
	}

	EHairGeometryType GetGeometryType(int32 InLODIndex) const
	{
		if (InLODIndex < 0 && InLODIndex >= LODGeometryTypes.Num()) return EHairGeometryType::NoneGeometry;
		return LODGeometryTypes[InLODIndex];
	}

	EHairBindingType GetBindingType(int32 InLODIndex) const
	{ 
		if (InLODIndex < 0 || InLODIndex >= BindingTypes.Num()) return EHairBindingType::NoneBinding;
		return BindingTypes[InLODIndex];
	}	

	bool IsSimulationEnable(int32 InLODIndex) const
	{
		if (InLODIndex < 0 || InLODIndex >= LODSimulations.Num()) return false;
		return LODSimulations[InLODIndex];
	}

	bool IsGlobalInterpolationEnable(int32 InLODIndex) const 
	{
		if (InLODIndex < 0 || InLODIndex >= LODGlobalInterpolations.Num()) return false;
		return LODGlobalInterpolations[InLODIndex];
	}

	void SetLODScreenSizes(const TArray<float>& ScreenSizes) { LODScreenSizes = ScreenSizes; }
	const TArray<float>& GetLODScreenSizes() const { return LODScreenSizes;  }

	void SetLODBias(float InLODBias) { LODBias = InLODBias; }
	float GetLODBias() const { return LODBias; }

	void SetLODIndex(float InLODIndex) { LODIndex = InLODIndex; }
	float GetLODIndex() const { return LODIndex; }
	int32 GetIntLODIndex() const { return FMath::Max(0, FMath::FloorToInt(LODIndex)); }

	void SetMeshLODIndex(float InMeshLODIndex) { MeshLODIndex = InMeshLODIndex; }
	float GetMeshLODIndex() const { return MeshLODIndex; }

	void SetLODVisibility(bool bVisible) { bLODVisibility = bVisible; }
	bool GetLODVisibility() const { return bLODVisibility; }

	uint32 GetClusterCount() const { return ClusterCount;  }

	uint32 GetActiveStrandsVertexStart(uint32 InVertexCount) const;
	uint32 GetActiveStrandsVertexCount(uint32 InVertexCount, float ScreenSize) const;
	float GetActiveStrandsSampleWeight(bool bUseTemporalWeight, float ScreenSize) const;

	void UpdateTemporalIndex();

	struct FVertexFactoryInput 
	{
		struct FStrands
		{
			FRDGImportedBuffer PositionBuffer;
			FRDGImportedBuffer PrevPositionBuffer;
			FRDGImportedBuffer TangentBuffer;
			FRDGImportedBuffer MaterialBuffer;
			FRDGImportedBuffer Attribute0Buffer;
			FRDGImportedBuffer Attribute1Buffer;
			FRDGImportedBuffer PositionOffsetBuffer;
			FRDGImportedBuffer PrevPositionOffsetBuffer;

			FRDGExternalBuffer PositionBufferExternal;
			FRDGExternalBuffer PrevPositionBufferExternal;
			FRDGExternalBuffer TangentBufferExternal;
			FRDGExternalBuffer MaterialBufferExternal;
			FRDGExternalBuffer Attribute0BufferExternal;
			FRDGExternalBuffer Attribute1BufferExternal;
			FRDGExternalBuffer PositionOffsetBufferExternal;
			FRDGExternalBuffer PrevPositionOffsetBufferExternal;

			FShaderResourceViewRHIRef PositionBufferRHISRV				= nullptr;
			FShaderResourceViewRHIRef PrevPositionBufferRHISRV			= nullptr;
			FShaderResourceViewRHIRef TangentBufferRHISRV				= nullptr;
			FShaderResourceViewRHIRef MaterialBufferRHISRV				= nullptr;
			FShaderResourceViewRHIRef Attribute0BufferRHISRV			= nullptr;
			FShaderResourceViewRHIRef Attribute1BufferRHISRV			= nullptr;
			FShaderResourceViewRHIRef PositionOffsetBufferRHISRV		= nullptr;
			FShaderResourceViewRHIRef PrevPositionOffsetBufferRHISRV	= nullptr;

			FVector PositionOffset = FVector::ZeroVector;
			FVector PrevPositionOffset = FVector::ZeroVector;

			uint32 VertexCount = 0;
			float HairRadius = 0;
			float HairRootScale = 0;
			float HairTipScale = 0;
			float HairRaytracingRadiusScale = 0;
			float HairLengthScale = 1.f;
			float HairLength = 0;
			float HairDensity = 0;
			bool bUseStableRasterization = false;
			bool bScatterSceneLighting = false;
			bool bUseRaytracingGeometry = false;
		} Strands;

		struct FCards
		{

		} Cards;

		struct FMeshes
		{
			
		} Meshes;

		bool bHasLODSwitch = false;
		EHairGeometryType GeometryType = EHairGeometryType::NoneGeometry;
		EHairBindingType BindingType = EHairBindingType::NoneBinding;
		FTransform LocalToWorldTransform;
	};

	// Current hook for retriving instance data. 
	// This needs to be refactor to merge FHairGroupPublicData & HairStrandsInstance
	FHairStrandsInstance* Instance = nullptr;

	FVertexFactoryInput VFInput;
	uint32 ClusterDataIndex = ~0; // #hair_todo: move this into instance data, or remove FHairStrandClusterData

	uint32 GroupControlTriangleStripVertexCount;
	uint32 GroupIndex;
	uint32 ClusterCount;
	uint32 VertexCount;

	/* Indirect draw buffer to draw everything or the result of the culling per pass */
	FRDGExternalBuffer DrawIndirectBuffer;
	FRDGExternalBuffer DrawIndirectRasterComputeBuffer;

	/* Hair Cluster & Hair Group bounding box buffer */
	FRDGExternalBuffer ClusterAABBBuffer;
	FRDGExternalBuffer GroupAABBBuffer;
	bool bGroupAABBValid = false;
	bool bClusterAABBValid = false;

	/* Culling & LODing results for a hair group */ // Better to be transient?
	FRDGExternalBuffer CulledVertexIdBuffer;
	FRDGExternalBuffer CulledVertexRadiusScaleBuffer;
	bool bCullingResultAvailable = false;
	bool bSupportVoxelization = true;
	bool bIsInitialized = false;

	/* CPU LOD selection. Hair LOD selection can be done by CPU or GPU. If bUseCPULODSelection is true, 
	   CPU LOD selection is enabled otherwise the GPU selection is used. CPU LOD selection use the CPU 
	   bounding box, which might not be as accurate as the GPU ones*/
	TArray<bool> LODVisibilities;
	TArray<float>LODScreenSizes;
	TArray<bool> LODSimulations;
	TArray<bool> LODGlobalInterpolations;
	bool bIsDeformationEnable;
	TArray<EHairGeometryType> LODGeometryTypes;

	TArray<EHairBindingType> BindingTypes;

	// Data change every frame by the groom proxy based on views data
	float MeshLODIndex = -1;
	float LODIndex = -1;		// Current LOD used for all views
	float LODBias = 0;			// Current LOD bias
	bool bLODVisibility = true; // Enable/disable hair rendering for this component

	FBoxSphereBounds ContinuousLODBounds; 	//used by Continuous LOD
	float MaxScreenSize = 0.f; 				//used by Continuous LOD
	uint32 TemporalIndex = 0; 				//used by Temporal Layering

	// Debug
	bool  bDebugDrawLODInfo = false; // Enable/disable hair LOD info
	float DebugScreenSize = 0.f;
	FLinearColor DebugGroupColor;
	EHairStrandsDebugMode DebugMode = EHairStrandsDebugMode::NoneDebug;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Cluster information exchanged between renderer and the hair strand plugin. 

struct FHairStrandClusterData
{
	struct FHairGroup
	{
		uint32 ClusterCount = 0;
		uint32 VertexCount = 0;

		float LODIndex = -1;
		float LODBias = 0.0f;
		bool bVisible = false;

		// See FHairStrandsClusterCullingResource fro details about those buffers.
		FRDGExternalBuffer* GroupAABBBuffer = nullptr;
		FRDGExternalBuffer* ClusterAABBBuffer = nullptr;
		FRDGExternalBuffer* ClusterInfoBuffer = nullptr;
		FRDGExternalBuffer* ClusterLODInfoBuffer = nullptr;
		FRDGExternalBuffer* VertexToClusterIdBuffer = nullptr;
		FRDGExternalBuffer* ClusterVertexIdBuffer = nullptr;

		TRefCountPtr<FRDGPooledBuffer> ClusterIdBuffer;
		TRefCountPtr<FRDGPooledBuffer> ClusterIndexOffsetBuffer;
		TRefCountPtr<FRDGPooledBuffer> ClusterIndexCountBuffer;

		// Culling & LOD output
		FRDGExternalBuffer* GetCulledVertexIdBuffer() const				{ return HairGroupPublicPtr ? &HairGroupPublicPtr->GetCulledVertexIdBuffer() : nullptr; }
		FRDGExternalBuffer* GetCulledVertexRadiusScaleBuffer() const	{ return HairGroupPublicPtr ? &HairGroupPublicPtr->GetCulledVertexRadiusScaleBuffer() : nullptr; }
		bool GetCullingResultAvailable() const							{ return HairGroupPublicPtr ? HairGroupPublicPtr->GetCullingResultAvailable() : false; }
		void SetCullingResultAvailable(bool b)							{ if (HairGroupPublicPtr) HairGroupPublicPtr->SetCullingResultAvailable(b); }

		TRefCountPtr<FRDGPooledBuffer> ClusterDebugInfoBuffer;	// Null if this debug is not enabled.
		FRDGBufferRef CulledClusterCountBuffer = nullptr;
		FRDGBufferRef CulledCluster1DIndirectArgsBuffer = nullptr;
		FRDGBufferRef CulledCluster2DIndirectArgsBuffer = nullptr;
		uint32 GroupSize1D = 0;

		FHairGroupPublicData* HairGroupPublicPtr = nullptr;
	};

	TArray<FHairGroup> HairGroups;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

// API for enabling/disabling the various geometry representation
enum class EHairStrandsShaderType
{
	Strands,
	Cards,
	Meshes,
	Tool,
	All
};
RENDERER_API bool IsHairStrandsSupported(EHairStrandsShaderType Type, EShaderPlatform Platform);
RENDERER_API bool IsHairStrandsEnabled(EHairStrandsShaderType Type, EShaderPlatform Platform = EShaderPlatform::SP_NumPlatforms);
RENDERER_API void SetHairStrandsEnabled(bool In);

RENDERER_API bool IsHairRayTracingEnabled();

// Return true if hair simulation is enabled.
RENDERER_API bool IsHairStrandsSimulationEnable();

// Return true if hair binding is enabled (i.e., hair can be attached to skeletal mesh)
RENDERER_API bool IsHairStrandsBindingEnable();

// Return true if strand reordering for compute raster continuous LOD is enabled
RENDERER_API bool IsHairStrandContinuousDecimationReorderingEnabled();

// Return true if continuous LOD is enabled - implies computer raster and continuous decimation reordering is true
RENDERER_API bool IsHairVisibilityComputeRasterContinuousLODEnabled();

// Return true if temporal layering is enabled - implies computer raster and continuous decimation reordering is true
RENDERER_API bool IsHairVisibilityComputeRasterTemporalLayeringEnabled();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef TArray<FRHIUnorderedAccessView*> FBufferTransitionQueue;
RENDERER_API void TransitBufferToReadable(FRDGBuilder& GraphBuilder, FBufferTransitionQueue& BuffersToTransit);

/// Return the hair coverage for a certain hair count and normalized avg hair radius (i.e, [0..1])
RENDERER_API float GetHairCoverage(uint32 HairCount, float AverageHairRadius);

/// Return the average hair normalized radius for a given hair count and a given coverage value
RENDERER_API float GetHairAvgRadius(uint32 InCount, float InCoverage);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HairStrands Bookmark API
enum class EHairStrandsBookmark : uint8
{
	ProcessTasks,
	ProcessLODSelection,
	ProcessGuideInterpolation,
	ProcessGatherCluster,
	ProcessStrandsInterpolation,
	ProcessDebug,
	ProcessEndOfFrame
};

enum EHairInstanceCount : uint8
{
	HairInstanceCount_StrandsPrimaryView = 0,
	HairInstanceCount_StrandsShadowView = 1,
	HairInstanceCount_CardsOrMeshes = 2,
};

struct FHairStrandsBookmarkParameters
{
	FShaderPrintData* ShaderPrintData = nullptr;
	class FGlobalShaderMap* ShaderMap = nullptr;

	uint32 ViewUniqueID = ~0; // View 0
	FIntRect ViewRect; // View 0
	FHairStrandsInstances VisibleInstances;
	FHairStrandsInstances* Instances = nullptr;
	const FSceneView* View = nullptr;// // View 0
	TArray<const FSceneView*> AllViews;
	FRDGTextureRef SceneColorTexture = nullptr;
	FRDGTextureRef SceneDepthTexture = nullptr; 

	FUintVector4 InstanceCountPerType = FUintVector4(0);

	bool bHzbRequest = false;
	uint32 FrameIndex = ~0;

	// Temporary
	FHairStrandClusterData HairClusterData;

	inline bool HasInstances() const { return Instances != nullptr && Instances->Num() > 0; }
};

typedef void (*THairStrandsBookmarkFunction)(FRDGBuilder* GraphBuilder, EHairStrandsBookmark Bookmark, FHairStrandsBookmarkParameters& Parameters);
RENDERER_API void RegisterBookmarkFunction(THairStrandsBookmarkFunction Bookmark);
