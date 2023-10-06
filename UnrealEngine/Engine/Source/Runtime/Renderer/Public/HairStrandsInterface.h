// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HairStrandsInterface.h: public interface for hair strands rendering.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Engine/EngineTypes.h"
#include "Shader.h"
#include "RenderResource.h"
#include "RenderGraphResources.h"
#include "ShaderPrintParameters.h"
#include "GroomVisualizationData.h"
#include "HairStrandsDefinitions.h"

class UTexture2D;
class FSceneInterface;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Shader parameters

// Instance attribute parameters
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceAttributeParameters, RENDERER_API)
	SHADER_PARAMETER(uint32, CurveAttributeIndexToChunkDivAsShift)
	SHADER_PARAMETER(uint32, CurveAttributeChunkElementCount)
	SHADER_PARAMETER(uint32, CurveAttributeChunkStrideInBytes)
	SHADER_PARAMETER(uint32, PointAttributeIndexToChunkDivAsShift)
	SHADER_PARAMETER(uint32, PointAttributeChunkElementCount)
	SHADER_PARAMETER(uint32, PointAttributeChunkStrideInBytes)
	SHADER_PARAMETER_ARRAY(FUintVector4, CurveAttributeOffsets, [HAIR_CURVE_ATTRIBUTE_OFFSET_COUNT])
	SHADER_PARAMETER_ARRAY(FUintVector4, PointAttributeOffsets, [HAIR_POINT_ATTRIBUTE_OFFSET_COUNT])
END_SHADER_PARAMETER_STRUCT()

// Instance common parameters
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceCommonParameters, RENDERER_API)
	SHADER_PARAMETER(float,  Density)
	SHADER_PARAMETER(float,  Radius)
	SHADER_PARAMETER(float,  RootScale)
	SHADER_PARAMETER(float,  TipScale)
	SHADER_PARAMETER(float,  Length)
	SHADER_PARAMETER(float,  LengthScale)
	SHADER_PARAMETER(float,  RaytracingRadiusScale)
	SHADER_PARAMETER(uint32, GroupIndex)
	SHADER_PARAMETER(uint32, GroupCount)
	SHADER_PARAMETER(uint32, PointCount)
	SHADER_PARAMETER(uint32, CurveCount)
	SHADER_PARAMETER(uint32, RaytracingProceduralSplits)
	SHADER_PARAMETER(uint32, bRaytracingGeometry)
	SHADER_PARAMETER(uint32, bStableRasterization)
	SHADER_PARAMETER(uint32, bScatterSceneLighting)
	SHADER_PARAMETER(uint32, bSimulation)
	SHADER_PARAMETER(uint32, bSingleGuide)
	SHADER_PARAMETER(FVector3f, PositionOffset)
	SHADER_PARAMETER(FVector3f, PrevPositionOffset)
	SHADER_PARAMETER(FMatrix44f, LocalToWorldPrimitiveTransform)
	SHADER_PARAMETER(FMatrix44f, LocalToTranslatedWorldPrimitiveTransform)
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceAttributeParameters, Attributes)
END_SHADER_PARAMETER_STRUCT()

// Instance resources (RDG)
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceResourceParameters, RENDERER_API)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, PositionBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, PositionOffsetBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, CurveBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, PointToCurveBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, CurveAttributeBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, PointAttributeBuffer)
END_SHADER_PARAMETER_STRUCT()

// Instance prev. resources (RDG)
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstancePrevResourceParameters, RENDERER_API)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, PrevPositionBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, PrevPositionOffsetBuffer)
END_SHADER_PARAMETER_STRUCT()

// Instance culling resources (RDG)
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceCullingParameters, RENDERER_API)
	SHADER_PARAMETER(uint32, bCullingEnable)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, CullingIndirectBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, CullingIndexBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, CullingRadiusScaleBuffer)
	RDG_BUFFER_ACCESS(CullingIndirectBufferArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

// Instance interpolation resources (RDG)
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceInterpolationParameters, RENDERER_API)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, InterpolationBuffer)
END_SHADER_PARAMETER_STRUCT()

// Instance resources (Raw)
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceResourceRawParameters, RENDERER_API)
	SHADER_PARAMETER_SRV(Buffer<uint4>, PositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, PositionOffsetBuffer)
	SHADER_PARAMETER_SRV(Buffer<uint>, CurveBuffer)
	SHADER_PARAMETER_SRV(Buffer<uint>, PointToCurveBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, TangentBuffer)
	SHADER_PARAMETER_SRV(ByteAddressBuffer, CurveAttributeBuffer)
	SHADER_PARAMETER_SRV(ByteAddressBuffer, PointAttributeBuffer)
END_SHADER_PARAMETER_STRUCT()

// Instance prev. resources (Raw)
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstancePrevResourceRawParameters, RENDERER_API)
	SHADER_PARAMETER_SRV(Buffer<uint4>, PreviousPositionBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, PreviousPositionOffsetBuffer)
END_SHADER_PARAMETER_STRUCT()

// Instance culling resources (Raw)
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceCullingRawParameters, RENDERER_API)
	SHADER_PARAMETER(uint32, bCullingEnable)
//	SHADER_PARAMETER_SRV(Buffer<uint>, CullingIndirectBuffer)
	SHADER_PARAMETER_SRV(Buffer<uint>, CullingIndexBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, CullingRadiusScaleBuffer)
END_SHADER_PARAMETER_STRUCT()

// Intermediate struct which can be referenced by FHairStrandsInstanceParameters for getting 
// the HairStrandsVF_ decoration in shader
BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceIntermediateParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceCommonParameters, Common)
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceResourceParameters, Resources)
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceCullingParameters, Culling)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FHairStrandsInstanceParameters, )
	SHADER_PARAMETER_STRUCT(FHairStrandsInstanceIntermediateParameters, HairStrandsVF)
END_SHADER_PARAMETER_STRUCT()

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

struct FRDGExternalBuffer
{
	TRefCountPtr<FRDGPooledBuffer> Buffer = nullptr;
	FShaderResourceViewRHIRef SRV = nullptr;
	FUnorderedAccessViewRHIRef UAV = nullptr;
	EPixelFormat Format = PF_Unknown;
	RENDERER_API void Release();
};

struct FRDGImportedBuffer
{
	FRDGBufferRef Buffer = nullptr;
	FRDGBufferSRVRef SRV = nullptr;
	FRDGBufferUAVRef UAV = nullptr;
};

RENDERER_API FRDGImportedBuffer Register(FRDGBuilder& GraphBuilder, const FRDGExternalBuffer& In, ERDGImportedBufferFlags Flags, ERDGUnorderedAccessViewFlags UAVFlags = ERDGUnorderedAccessViewFlags::None);
RENDERER_API FRDGBufferSRVRef   RegisterAsSRV(FRDGBuilder& GraphBuilder, const FRDGExternalBuffer& In);
RENDERER_API FRDGBufferUAVRef   RegisterAsUAV(FRDGBuilder& GraphBuilder, const FRDGExternalBuffer& In, ERDGUnorderedAccessViewFlags Flags = ERDGUnorderedAccessViewFlags::None);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Misc/Helpers

struct FHairStrandClusterData;
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

struct FHairStrandsInstance
{
	virtual ~FHairStrandsInstance() = default;
	RENDERER_API uint32 GetRefCount() const;
	RENDERER_API uint32 AddRef() const;
	RENDERER_API uint32 Release() const;
	int32 RegisteredIndex = -1;
	virtual const FBoxSphereBounds& GetBounds() const = 0;
	virtual const FBoxSphereBounds& GetLocalBounds() const = 0;
	virtual const FHairGroupPublicData* GetHairData() const { return nullptr; }
	virtual const EHairGeometryType GetHairGeometry() const { return EHairGeometryType::NoneGeometry; }
protected:
	mutable uint32 RefCount = 0;
};
typedef TArray<FHairStrandsInstance*> FHairStrandsInstances;

class FHairGroupPublicData
{
public:
	RENDERER_API FHairGroupPublicData(uint32 InGroupIndex, const FName& OwnerName);
	
	uint32 GetGroupIndex() const { return GroupIndex; }

	FRDGExternalBuffer& GetDrawIndirectRasterComputeBuffer() { return Culling->DrawIndirectRasterComputeBuffer; }
	const FRDGExternalBuffer& GetDrawIndirectRasterComputeBuffer() const { return Culling->DrawIndirectRasterComputeBuffer; }
	FRDGExternalBuffer& GetDrawIndirectBuffer() { return Culling->DrawIndirectBuffer; }
	FRDGExternalBuffer& GetClusterAABBBuffer() { return Culling->ClusterAABBBuffer; }
	FRDGExternalBuffer& GetGroupAABBBuffer() { return Culling->GroupAABBBuffer; }
	const FRDGExternalBuffer& GetGroupAABBBuffer() const { return Culling->GroupAABBBuffer; }

	const FRDGExternalBuffer& GetCulledCurveBuffer() const { return Culling->CulledCurveBuffer; }
	const FRDGExternalBuffer& GetCulledVertexIdBuffer() const { return Culling->CulledVertexIdBuffer; }
	const FRDGExternalBuffer& GetCulledVertexRadiusScaleBuffer() const { return Culling->CulledVertexRadiusScaleBuffer; }

	FRDGExternalBuffer& GetCulledCurveBuffer() { return Culling->CulledCurveBuffer; }
	FRDGExternalBuffer& GetCulledVertexIdBuffer() { return Culling->CulledVertexIdBuffer; }
	FRDGExternalBuffer& GetCulledVertexRadiusScaleBuffer() { return Culling->CulledVertexRadiusScaleBuffer; }

	bool GetCullingResultAvailable() const { return Culling->bCullingResultAvailable; }
	void SetCullingResultAvailable(bool b) { Culling->bCullingResultAvailable = b; }
	
	void SetClusterAABBValid(bool In) { Culling->bClusterAABBValid = In;  }
	bool GetClusterAABBValid() const  { return Culling->bClusterAABBValid;  }

	void SetGroupAABBValid(bool In) { Culling->bGroupAABBValid = In;  }

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
	float GetClusterScale() const { return ClusterScale;  }

	// Return the number of active point/curve for strand geometry
	RENDERER_API uint32 GetActiveStrandsPointCount() const;
	RENDERER_API uint32 GetActiveStrandsCurveCount() const;
	RENDERER_API float  GetActiveStrandsCoverageScale() const;

	struct FVertexFactoryInput 
	{
		struct FStrands
		{
			FRDGImportedBuffer PositionBuffer;
			FRDGImportedBuffer PrevPositionBuffer;
			FRDGImportedBuffer TangentBuffer;
			FRDGImportedBuffer CurveAttributeBuffer;
			FRDGImportedBuffer PointAttributeBuffer;
			FRDGImportedBuffer PointToCurveBuffer;
			FRDGImportedBuffer PositionOffsetBuffer;
			FRDGImportedBuffer PrevPositionOffsetBuffer;
			FRDGImportedBuffer CurveBuffer;

			FRDGExternalBuffer PositionBufferExternal;
			FRDGExternalBuffer PrevPositionBufferExternal;
			FRDGExternalBuffer TangentBufferExternal;
			FRDGExternalBuffer CurveAttributeBufferExternal;
			FRDGExternalBuffer PointAttributeBufferExternal;
			FRDGExternalBuffer PointToCurveBufferExternal;
			FRDGExternalBuffer PositionOffsetBufferExternal;
			FRDGExternalBuffer PrevPositionOffsetBufferExternal;
			FRDGExternalBuffer CurveBufferExternal;

			FShaderResourceViewRHIRef PositionBufferRHISRV				= nullptr;
			FShaderResourceViewRHIRef PrevPositionBufferRHISRV			= nullptr;
			FShaderResourceViewRHIRef TangentBufferRHISRV				= nullptr;
			FShaderResourceViewRHIRef CurveAttributeBufferRHISRV		= nullptr;
			FShaderResourceViewRHIRef PointAttributeBufferRHISRV		= nullptr;
			FShaderResourceViewRHIRef PointToCurveBufferRHISRV			= nullptr;
			FShaderResourceViewRHIRef PositionOffsetBufferRHISRV		= nullptr;
			FShaderResourceViewRHIRef PrevPositionOffsetBufferRHISRV	= nullptr;
			FShaderResourceViewRHIRef CurveBufferRHISRV					= nullptr;

			FHairStrandsInstanceCommonParameters Common; 
		} Strands;

		struct FCards
		{

		} Cards;

		struct FMeshes
		{
			
		} Meshes;

		bool bHasLODSwitch = false;
		bool bHasLODSwitchBindingType = false;
		EHairGeometryType GeometryType = EHairGeometryType::NoneGeometry;
		EHairBindingType BindingType = EHairBindingType::NoneBinding;
		FTransform LocalToWorldTransform;
	};

	struct FCulling
	{
		/* Indirect draw buffer to draw everything or the result of the culling per pass */
		FRDGExternalBuffer DrawIndirectBuffer;
		FRDGExternalBuffer DrawIndirectRasterComputeBuffer;

		/* Hair Cluster & Hair Group bounding box buffer */
		FRDGExternalBuffer ClusterAABBBuffer;
		FRDGExternalBuffer GroupAABBBuffer;
		bool bGroupAABBValid = false;
		bool bClusterAABBValid = false;

		/* Culling & LODing results for a hair group */ // Better to be transient?
		FRDGExternalBuffer CulledCurveBuffer;
		FRDGExternalBuffer CulledVertexIdBuffer;
		FRDGExternalBuffer CulledVertexRadiusScaleBuffer;
		bool bCullingResultAvailable = false;
	};

	// Current hook for retriving instance data. 
	// This needs to be refactor to merge FHairGroupPublicData & HairStrandsInstance
	FHairStrandsInstance* Instance = nullptr;
	FCulling* Culling = nullptr;

	FVertexFactoryInput VFInput;
	uint32 ClusterDataIndex = ~0; // #hair_todo: move this into instance data, or remove FHairStrandClusterData

	uint32 GroupIndex = 0;
	uint32 RestPointCount = 0;
	uint32 RestCurveCount = 0;
	uint32 ClusterCount = 0;
	float ClusterScale = 0;

	bool bSupportVoxelization = true;

	/* CPU LOD selection. Hair LOD selection can be done by CPU or GPU. If bUseCPULODSelection is true, 
	   CPU LOD selection is enabled otherwise the GPU selection is used. CPU LOD selection use the CPU 
	   bounding box, which might not be as accurate as the GPU ones*/
	TArray<bool> LODVisibilities;
	TArray<float>LODScreenSizes;
	TArray<bool> LODSimulations;
	TArray<bool> LODGlobalInterpolations;
	TArray<EHairGeometryType> LODGeometryTypes;
	bool bIsDeformationEnable = false;
	bool bIsSimulationCacheEnable = false;

	TArray<EHairBindingType> BindingTypes;

	// Data change every frame by the groom proxy based on views data
	float MeshLODIndex = -1;
	float LODIndex = -1;		// Current LOD used for all views
	float LODBias = 0;			// Current LOD bias
	bool bLODVisibility = true; // Enable/disable hair rendering for this component
	bool bAutoLOD = false;

	// Active/used point/curved based on select continuous LOD
	uint32 ContinuousLODPointCount = 0;
	uint32 ContinuousLODCurveCount = 0;
	float ContinuousLODScreenSize = 1.f;
	float ContinuousLODCoverageScale = 1.f;
	FVector2f ContinuousLODScreenPos = FVector2f(0,0);
	FBoxSphereBounds ContinuousLODBounds; 	//used by Continuous LOD

	// Debug
	bool  bDebugDrawLODInfo = false; // Enable/disable hair LOD info
	float DebugScreenSize = 0.f;
	FLinearColor DebugGroupColor;
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

RENDERER_API bool IsHairRayTracingEnabled();

// Return true if hair simulation is enabled.
RENDERER_API bool IsHairStrandsSimulationEnable();

// Return true if hair binding is enabled (i.e., hair can be attached to skeletal mesh)
RENDERER_API bool IsHairStrandsBindingEnable();

// Return true if strand reordering for compute raster continuous LOD is enabled
RENDERER_API bool IsHairStrandContinuousDecimationReorderingEnabled();

// Return true if continuous LOD is enabled - implies computer raster and continuous decimation reordering is true
RENDERER_API bool IsHairVisibilityComputeRasterContinuousLODEnabled();

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
	ProcessCardsAndMeshesInterpolation_PrimaryView,
	ProcessCardsAndMeshesInterpolation_ShadowView,
	ProcessStrandsInterpolation,
	ProcessDebug,
	ProcessEndOfFrame
};

enum class EHairInstanceCount : uint8
{
	StrandsPrimaryView = 0,
	StrandsShadowView = 1,
	CardsOrMeshesPrimaryView = 2,
	CardsOrMeshesShadowView = 3,
	Count
};

struct FHairStrandsBookmarkParameters
{
	FShaderPrintData* ShaderPrintData = nullptr;
	class FGlobalShaderMap* ShaderMap = nullptr;

	uint32 ViewUniqueID = ~0; // View 0
	FIntRect ViewRect; // View 0

	FHairStrandsInstances VisibleStrands; // Primary & Shadow
	FHairStrandsInstances VisibleCardsOrMeshes_Primary;
	FHairStrandsInstances VisibleCardsOrMeshes_Shadow;

	FHairStrandsInstances* Instances = nullptr;
	TBitArray<> InstancesVisibility;
	const FSceneView* View = nullptr;// // View 0
	FSceneInterface* Scene = nullptr;
	TArray<const FSceneView*> AllViews;
	FRDGTextureRef SceneColorTexture = nullptr;
	FRDGTextureRef SceneDepthTexture = nullptr; 

	FUintVector4 InstanceCountPerType = FUintVector4(0);

	inline bool HasInstances() const { return Instances != nullptr && Instances->Num() > 0; }
};

typedef void (*THairStrandsBookmarkFunction)(FRDGBuilder* GraphBuilder, EHairStrandsBookmark Bookmark, FHairStrandsBookmarkParameters& Parameters);
RENDERER_API void RegisterBookmarkFunction(THairStrandsBookmarkFunction Bookmark);
