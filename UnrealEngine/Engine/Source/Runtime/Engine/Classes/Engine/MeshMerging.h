// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/MaterialMerging.h"
#include "MeshMerging.generated.h"

class AActor;
class UInstancedStaticMeshComponent;

/** The importance of a mesh feature when automatically generating mesh LODs. */
UENUM()
namespace EMeshFeatureImportance
{
	enum Type : int
	{
		Off,
		Lowest,
		Low,
		Normal,
		High,
		Highest
	};
}


/** Enum specifying the reduction type to use when simplifying static meshes with the engines internal tool */
UENUM()
enum class EStaticMeshReductionTerimationCriterion : uint8
{
	Triangles UMETA(DisplayName = "Triangles", ToolTip = "Triangle percent criterion will be used for simplification."),
	Vertices UMETA(DisplayName = "Vertice", ToolTip = "Vertice percent criterion will be used for simplification."),
	Any UMETA(DisplayName = "First Percent Satisfied", ToolTip = "Simplification will continue until either Triangle or Vertex count criteria is met."),
};

/** Settings used to reduce a mesh. */
USTRUCT(Blueprintable)
struct FMeshReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/** Percentage of triangles to keep. 1.0 = no reduction, 0.0 = no triangles. (Triangles criterion properties) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float PercentTriangles;

	/** The maximum number of triangles to retain when using percentage termination criterion. (Triangles criterion properties) */
	UPROPERTY(EditAnywhere, Category = ReductionMethod, meta = (DisplayName = "Max Triangle Count", ClampMin = 2, UIMin = "2"))
	uint32 MaxNumOfTriangles;

	/** Percentage of vertices to keep. 1.0 = no reduction, 0.0 = no vertices. (Vertices criterion properties) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float PercentVertices;

	/** The maximum number of vertices to retain when using percentage termination criterion. (Vertices criterion properties) */
	UPROPERTY(EditAnywhere, Category = ReductionMethod, meta = (DisplayName = "Max Vertex Count", ClampMin = 4, UIMin = "4"))
	uint32 MaxNumOfVerts;

	/** The maximum distance in object space by which the reduced mesh may deviate from the original mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float MaxDeviation;

	/** The amount of error in pixels allowed for this LOD. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float PixelError;

	/** Threshold in object space at which vertices are welded together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float WeldingThreshold;

	/** Angle at which a hard edge is introduced between faces. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	float HardAngleThreshold;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	int32 BaseLODModel;

	/** Higher values minimize change to border edges. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> SilhouetteImportance;

	/** Higher values reduce texture stretching. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> TextureImportance;

	/** Higher values try to preserve normals better. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> ShadingImportance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bRecalculateNormals:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bGenerateUniqueLightmapUVs:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bKeepSymmetry:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bVisibilityAided:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	uint8 bCullOccluded:1;

	/** The method to use when optimizing static mesh LODs */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	EStaticMeshReductionTerimationCriterion TerminationCriterion;

	/** Higher values generates fewer samples*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> VisibilityAggressiveness;

	/** Higher values minimize change to vertex color data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ReductionSettings)
	TEnumAsByte<EMeshFeatureImportance::Type> VertexColorImportance;

	/** Default settings. */
	FMeshReductionSettings()
		: PercentTriangles(1.0f)
		, MaxNumOfTriangles(MAX_uint32)
		, PercentVertices(1.0f)
		, MaxNumOfVerts(MAX_uint32)
		, MaxDeviation(0.0f)
		, PixelError(8.0f)
		, WeldingThreshold(0.0f)
		, HardAngleThreshold(80.0f)
		, BaseLODModel(0)
		, SilhouetteImportance(EMeshFeatureImportance::Normal)
		, TextureImportance(EMeshFeatureImportance::Normal)
		, ShadingImportance(EMeshFeatureImportance::Normal)
		, bRecalculateNormals(false)
		, bGenerateUniqueLightmapUVs(false)
		, bKeepSymmetry(false)
		, bVisibilityAided(false)
		, bCullOccluded(false)
		, TerminationCriterion(EStaticMeshReductionTerimationCriterion::Triangles)
		, VisibilityAggressiveness(EMeshFeatureImportance::Lowest)
		, VertexColorImportance(EMeshFeatureImportance::Off)
	{
	}

	/** Equality operator. */
	bool operator==(const FMeshReductionSettings& Other) const
	{
		return
			TerminationCriterion == Other.TerminationCriterion
			&& PercentVertices == Other.PercentVertices
			&& PercentTriangles == Other.PercentTriangles
			&& MaxNumOfTriangles == Other.MaxNumOfTriangles
			&& MaxNumOfVerts == Other.MaxNumOfVerts
			&& MaxDeviation == Other.MaxDeviation
			&& PixelError == Other.PixelError
			&& WeldingThreshold == Other.WeldingThreshold
			&& HardAngleThreshold == Other.HardAngleThreshold
			&& SilhouetteImportance == Other.SilhouetteImportance
			&& TextureImportance == Other.TextureImportance
			&& ShadingImportance == Other.ShadingImportance
			&& bRecalculateNormals == Other.bRecalculateNormals
			&& BaseLODModel == Other.BaseLODModel
			&& bGenerateUniqueLightmapUVs == Other.bGenerateUniqueLightmapUVs
			&& bKeepSymmetry == Other.bKeepSymmetry
			&& bVisibilityAided == Other.bVisibilityAided
			&& bCullOccluded == Other.bCullOccluded
			&& VisibilityAggressiveness == Other.VisibilityAggressiveness
			&& VertexColorImportance == Other.VertexColorImportance;
	}

	/** Inequality. */
	bool operator!=(const FMeshReductionSettings& Other) const
	{
		return !(*this == Other);
	}
};

UENUM()
namespace ELandscapeCullingPrecision
{
	enum Type : int
	{
		High = 0 UMETA(DisplayName = "High memory intensity and computation time"),
		Medium = 1 UMETA(DisplayName = "Medium memory intensity and computation time"),
		Low = 2 UMETA(DisplayName = "Low memory intensity and computation time")
	};
}

UENUM()
namespace EProxyNormalComputationMethod
{
	enum Type : int
	{
		AngleWeighted = 0 UMETA(DisplayName = "Angle Weighted"),
		AreaWeighted = 1 UMETA(DisplayName = "Area  Weighted"),
		EqualWeighted = 2 UMETA(DisplayName = "Equal Weighted")
	};
}


USTRUCT(Blueprintable)
struct FMeshProxySettings
{
	GENERATED_USTRUCT_BODY()
	/** Screen size of the resulting proxy mesh in pixels*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (ClampMin = "1", ClampMax = "1200", UIMin = "1", UIMax = "1200"))
	int32 ScreenSize;

	/** Override when converting multiple meshes for proxy LOD merging. Warning, large geometry with small sampling has very high memory costs*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = ProxySettings, meta = (EditCondition = "bOverrideVoxelSize", ClampMin = "0.1", DisplayName = "Override Spatial Sampling Distance"))
	float VoxelSize;

	/** Material simplification */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	FMaterialProxySettings MaterialSettings;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	int32 TextureWidth_DEPRECATED;
	UPROPERTY()
	int32 TextureHeight_DEPRECATED;

	UPROPERTY()
	uint8 bExportNormalMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportMetallicMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportRoughnessMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportSpecularMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bBakeVertexData_DEPRECATED:1;

	UPROPERTY()
	uint8 bGenerateNaniteEnabledMesh_DEPRECATED : 1;
	
	UPROPERTY()
	float NaniteProxyTrianglePercent_DEPRECATED;
#endif

	/** Distance at which meshes should be merged together, this can close gaps like doors and windows in distant geometry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	float MergeDistance;

	/** Base color assigned to LOD geometry that can't be associated with the source geometry: e.g. doors and windows that have been closed by the Merge Distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (DisplayName = "Unresolved Geometry Color"))
	FColor UnresolvedGeometryColor;

	/** Override search distance used when discovering texture values for simplified geometry. Useful when non-zero Merge Distance setting generates new geometry in concave corners.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (EditCondition = "bOverrideTransferDistance", DisplayName = "Transfer Distance Override", ClampMin = 0))
	float MaxRayCastDist;

	/** Angle at which a hard edge is introduced between faces */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (EditCondition = "bUseHardAngleThreshold", DisplayName = "Hard Edge Angle", ClampMin = 0, ClampMax = 180))
	float HardAngleThreshold;

	/** Lightmap resolution */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (ClampMin = 32, ClampMax = 4096, EditCondition = "!bComputeLightMapResolution", DisplayAfter="NormalCalculationMethod", DisplayName="Lightmap Resolution"))
	int32 LightMapResolution;

	/** Controls the method used to calculate the normal for the simplified geometry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (DisplayName = "Normal Calculation Method"))
	TEnumAsByte<EProxyNormalComputationMethod::Type> NormalCalculationMethod;

	/** Level of detail of the landscape that should be used for the culling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = LandscapeCulling, meta = (EditCondition="bUseLandscapeCulling", DisplayAfter="bUseLandscapeCulling"))
	TEnumAsByte<ELandscapeCullingPrecision::Type> LandscapeCullingPrecision;

	/** Determines whether or not the correct LOD models should be calculated given the source meshes and transition size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta=(DisplayAfter="ScreenSize"))
	uint8 bCalculateCorrectLODModel:1;

	/** If true, Spatial Sampling Distance will not be automatically computed based on geometry and you must set it directly */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = ProxySettings, meta = (InlineEditConditionToggle))
	uint8 bOverrideVoxelSize : 1;

	/** Enable an override for material transfer distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MaxRayCastDist, meta = (InlineEditConditionToggle))
	uint8 bOverrideTransferDistance:1;

	/** Enable the use of hard angle based vertex splitting */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = HardAngleThreshold, meta = (InlineEditConditionToggle))
	uint8 bUseHardAngleThreshold:1;

	/** If ticked will compute the lightmap resolution by summing the dimensions for each mesh included for merging */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings, meta = (DisplayName="Compute Lightmap Resolution"))
	uint8 bComputeLightMapResolution:1;

	/** Whether Simplygon should recalculate normals, otherwise the normals channel will be sampled from the original mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bRecalculateNormals:1;

	/** Whether or not to use available landscape geometry to cull away invisible triangles */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = LandscapeCulling)
	uint8 bUseLandscapeCulling:1;

	/** Whether ray tracing will be supported on this mesh. Disable this to save memory if the generated mesh will only be rendered in the distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bSupportRayTracing : 1;

	/** Whether to allow distance field to be computed for this mesh. Disable this to save memory if the merged mesh will only be rendered in the distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bAllowDistanceField:1;

	/** Whether to attempt to re-use the source mesh's lightmap UVs when baking the material or always generate a new set. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bReuseMeshLightmapUVs:1;

	/** Bake identical meshes (or mesh instances) only once. Can lead to discrepancies with the source mesh visual, especially for materials that are using world position or per instance data. However, this will result in better quality baked textures & greatly reduce baking time. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bGroupIdenticalMeshesForBaking:1;

	/** Whether to generate collision for the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bCreateCollision:1;

	/** Whether to allow vertex colors saved in the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bAllowVertexColors:1;

	/** Whether to generate lightmap uvs for the merged mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ProxySettings)
	uint8 bGenerateLightmapUVs:1;

	/** Settings related to building Nanite data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NaniteSettings)
	FMeshNaniteSettings NaniteSettings;

	/** Default settings. */
	FMeshProxySettings()
		: ScreenSize(300)
		, VoxelSize(3.f)
#if WITH_EDITORONLY_DATA
		, TextureWidth_DEPRECATED(512)
		, TextureHeight_DEPRECATED(512)
		, bExportNormalMap_DEPRECATED(true)
		, bExportMetallicMap_DEPRECATED(false)
		, bExportRoughnessMap_DEPRECATED(false)
		, bExportSpecularMap_DEPRECATED(false)
		, bBakeVertexData_DEPRECATED(false)
		, bGenerateNaniteEnabledMesh_DEPRECATED(false)
		, NaniteProxyTrianglePercent_DEPRECATED(100)
#endif
		, MergeDistance(0)
		, UnresolvedGeometryColor(FColor::Black)
		, MaxRayCastDist(20)
		, HardAngleThreshold(130.f)
		, LightMapResolution(256)
		, NormalCalculationMethod(EProxyNormalComputationMethod::AngleWeighted)
		, LandscapeCullingPrecision(ELandscapeCullingPrecision::Medium)
		, bCalculateCorrectLODModel(false)
		, bOverrideVoxelSize(false)
		, bOverrideTransferDistance(false)
		, bUseHardAngleThreshold(false)
		, bComputeLightMapResolution(false)
		, bRecalculateNormals(true)
		, bUseLandscapeCulling(false)
		, bSupportRayTracing(true)
		, bAllowDistanceField(false)
		, bReuseMeshLightmapUVs(true)
		, bGroupIdenticalMeshesForBaking(false)
		, bCreateCollision(true)
		, bAllowVertexColors(false)
		, bGenerateLightmapUVs(false)
	{
		MaterialSettings.MaterialMergeType = EMaterialMergeType::MaterialMergeType_Simplygon;
	}

	/** Equality operator. */
	bool operator==(const FMeshProxySettings& Other) const
	{
		return ScreenSize == Other.ScreenSize
			&& VoxelSize == Other.VoxelSize
			&& MaterialSettings == Other.MaterialSettings
			&& MergeDistance == Other.MergeDistance
			&& UnresolvedGeometryColor == Other.UnresolvedGeometryColor
			&& MaxRayCastDist == Other.MaxRayCastDist
			&& HardAngleThreshold == Other.HardAngleThreshold
			&& LightMapResolution == Other.LightMapResolution
			&& NormalCalculationMethod == Other.NormalCalculationMethod
			&& LandscapeCullingPrecision == Other.LandscapeCullingPrecision
			&& bCalculateCorrectLODModel == Other.bCalculateCorrectLODModel
			&& bOverrideVoxelSize == Other.bOverrideVoxelSize
			&& bOverrideTransferDistance == Other.bOverrideTransferDistance
			&& bUseHardAngleThreshold == Other.bUseHardAngleThreshold
			&& bComputeLightMapResolution == Other.bComputeLightMapResolution
			&& bRecalculateNormals == Other.bRecalculateNormals
			&& bUseLandscapeCulling == Other.bUseLandscapeCulling
			&& bSupportRayTracing == Other.bSupportRayTracing
			&& bAllowDistanceField == Other.bAllowDistanceField
			&& bReuseMeshLightmapUVs == Other.bReuseMeshLightmapUVs
			&& bGroupIdenticalMeshesForBaking == Other.bGroupIdenticalMeshesForBaking
			&& bCreateCollision == Other.bCreateCollision
			&& bAllowVertexColors == Other.bAllowVertexColors
			&& bGenerateLightmapUVs == Other.bGenerateLightmapUVs
			&& NaniteSettings == Other.NaniteSettings;
	}

	/** Inequality. */
	bool operator!=(const FMeshProxySettings& Other) const
	{
		return !(*this == Other);
	}

#if WITH_EDITORONLY_DATA
	/** Handles deprecated properties */
	void PostSerialize(const FArchive& Ar);
#endif
};

template<>
struct TStructOpsTypeTraits<FMeshProxySettings> : public TStructOpsTypeTraitsBase2<FMeshProxySettings>
{
#if WITH_EDITORONLY_DATA
	enum
	{
		WithPostSerialize = true,
	};
#endif
};


UENUM()
enum class EMeshLODSelectionType : uint8
{
	// Whether or not to export all of the LODs found in the source meshes
	AllLODs = 0 UMETA(DisplayName = "Use all LOD levels"),
	// Whether or not to export all of the LODs found in the source meshes
	SpecificLOD = 1 UMETA(DisplayName = "Use specific LOD level"),
	// Whether or not to calculate the appropriate LOD model for the given screen size
	CalculateLOD = 2 UMETA(DisplayName = "Calculate correct LOD level"),
	// Whether or not to use the lowest-detail LOD
	LowestDetailLOD = 3 UMETA(DisplayName = "Always use the lowest-detail LOD (i.e. the highest LOD index)")
};

UENUM()
enum class EMeshMergeType : uint8
{
	MeshMergeType_Default,
	MeshMergeType_MergeActor
};

/** As UHT doesnt allow arrays of bools, we need this binary enum :( */
UENUM()
enum class EUVOutput : uint8
{
	DoNotOutputChannel,
	OutputChannel
};

/**
* Mesh merging settings
*/
USTRUCT(Blueprintable)
struct FMeshMergingSettings
{
	GENERATED_USTRUCT_BODY()

	/** The lightmap resolution used both for generating lightmap UV coordinates, and also set on the generated static mesh */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings, meta=(ClampMax = 4096, EditCondition = "!bComputedLightMapResolution", DisplayAfter="bGenerateLightMapUV", DisplayName="Target Lightmap Resolution"))
	int32 TargetLightMapResolution;

	/** Whether to output the specified UV channels into the merged mesh (only if the source meshes contain valid UVs for the specified channel) */
	UPROPERTY(EditAnywhere, Category = MeshSettings, meta=(DisplayAfter="bBakeVertexDataToMesh"))
	EUVOutput OutputUVs[8];	// Should be MAX_MESH_TEXTURE_COORDS but as this is an engine module we cant include RawMesh

	/** Material simplification */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials", DisplayAfter="bMergeMaterials"))
	FMaterialProxySettings MaterialSettings;

	/** The gutter (in texels) to add to each sub-chart for our baked-out material for the top mip level */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, meta=(DisplayAfter="MaterialSettings"))
	int32 GutterSize;

	/** Which selection mode should be used when generating the merged static mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings, meta = (DisplayAfter="bBakeVertexDataToMesh", DisplayName = "LOD Selection Type"))
	EMeshLODSelectionType LODSelectionType;

	/** A given LOD level to export from the source meshes, used if LOD Selection Type is set to SpecificLOD */
	UPROPERTY(EditAnywhere, Category = MeshSettings, BlueprintReadWrite, meta = (DisplayAfter="LODSelectionType", EditCondition = "LODSelectionType == EMeshLODSelectionType::SpecificLOD", ClampMin = "0", ClampMax = "7", UIMin = "0", UIMax = "7", EnumCondition = 1))
	int32 SpecificLOD;

	/** Whether to generate lightmap UVs for a merged mesh*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings, meta=(DisplayName="Generate Lightmap UV"))
	uint8 bGenerateLightMapUV:1;

	/** Whether or not the lightmap resolution should be computed by summing the lightmap resolutions for the input Mesh Components */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = MeshSettings, meta=(DisplayName="Computed Lightmap Resolution"))
	uint8 bComputedLightMapResolution:1;

	/** Whether merged mesh should have pivot at world origin, or at first merged component otherwise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bPivotPointAtZero:1;

	/** Whether to merge physics data (collision primitives)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bMergePhysicsData:1;

	/** Whether to merge sockets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bMergeMeshSockets : 1;

	/** Whether to merge source materials into one flat material, ONLY available when LOD Selection Type is set to LowestDetailLOD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaterialSettings, meta=(EditCondition="LODSelectionType == EMeshLODSelectionType::LowestDetailLOD || LODSelectionType == EMeshLODSelectionType::SpecificLOD"))
	uint8 bMergeMaterials:1;

	/** Whether or not vertex data such as vertex colours should be baked into the resulting mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bBakeVertexDataToMesh:1;

	/** Whether or not vertex data such as vertex colours should be used when baking out materials */
	UPROPERTY(EditAnywhere, Category = MaterialSettings, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials"))
	uint8 bUseVertexDataForBakingMaterial:1;

	/** Whether or not to calculate varying output texture sizes according to their importance in the final atlas texture */
	UPROPERTY(Category = MaterialSettings, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bMergeMaterials"))
	uint8 bUseTextureBinning:1;

	/** Whether to attempt to re-use the source mesh's lightmap UVs when baking the material or always generate a new set. */
	UPROPERTY(EditAnywhere, Category = MaterialSettings)
	uint8 bReuseMeshLightmapUVs:1;

	/** Whether to attempt to merge materials that are deemed equivalent. This can cause artifacts in the merged mesh if world position/actor position etc. is used to determine output color. */
	UPROPERTY(EditAnywhere, Category = MaterialSettings)
	uint8 bMergeEquivalentMaterials:1;

	/** Whether or not to use available landscape geometry to cull away invisible triangles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LandscapeCulling)
	uint8 bUseLandscapeCulling:1;

	/** Whether or not to include any imposter LODs that are part of the source static meshes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bIncludeImposters:1;

	/** Whether ray tracing will be supported on this mesh. Disable this to save memory if the generated mesh will only be rendered in the distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshSettings)
	uint8 bSupportRayTracing : 1;

	/** Whether to allow distance field to be computed for this mesh. Disable this to save memory if the merged mesh will only be rendered in the distance. */
	UPROPERTY(EditAnywhere, Category = MeshSettings)
	uint8 bAllowDistanceField:1;

	/** Settings related to building Nanite data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NaniteSettings)
	FMeshNaniteSettings NaniteSettings;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint8 bImportVertexColors_DEPRECATED:1;

	UPROPERTY()
	uint8 bCalculateCorrectLODModel_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportNormalMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportMetallicMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportRoughnessMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bExportSpecularMap_DEPRECATED:1;

	UPROPERTY()
	uint8 bCreateMergedMaterial_DEPRECATED : 1;

	UPROPERTY()
	int32 MergedMaterialAtlasResolution_DEPRECATED;

	UPROPERTY()
	int32 ExportSpecificLOD_DEPRECATED;

	UPROPERTY()
	uint8 bGenerateNaniteEnabledMesh_DEPRECATED : 1;

	UPROPERTY()
	float NaniteFallbackTrianglePercent_DEPRECATED;	
#endif

	EMeshMergeType MergeType;

	/** Default settings. */
	FMeshMergingSettings()
		: TargetLightMapResolution(256)
		, GutterSize(2)
		, LODSelectionType(EMeshLODSelectionType::CalculateLOD)
		, SpecificLOD(0)
		, bGenerateLightMapUV(true)
		, bComputedLightMapResolution(false)
		, bPivotPointAtZero(false)
		, bMergePhysicsData(false)
		, bMergeMeshSockets(false)
		, bMergeMaterials(false)
		, bBakeVertexDataToMesh(false)
		, bUseVertexDataForBakingMaterial(true)
		, bUseTextureBinning(false)
		, bReuseMeshLightmapUVs(true)
		, bMergeEquivalentMaterials(true)
		, bUseLandscapeCulling(false)
		, bIncludeImposters(true)
		, bSupportRayTracing(true)
		, bAllowDistanceField(false)
#if WITH_EDITORONLY_DATA
		, bImportVertexColors_DEPRECATED(false)
		, bCalculateCorrectLODModel_DEPRECATED(false)
		, bExportNormalMap_DEPRECATED(true)
		, bExportMetallicMap_DEPRECATED(false)
		, bExportRoughnessMap_DEPRECATED(false)
		, bExportSpecularMap_DEPRECATED(false)
		, bCreateMergedMaterial_DEPRECATED(false)
		, MergedMaterialAtlasResolution_DEPRECATED(1024)
		, ExportSpecificLOD_DEPRECATED(0)
		, bGenerateNaniteEnabledMesh_DEPRECATED(false)
		, NaniteFallbackTrianglePercent_DEPRECATED(100)
#endif
		, MergeType(EMeshMergeType::MeshMergeType_Default)
	{
		for(EUVOutput& OutputUV : OutputUVs)
		{
			OutputUV = EUVOutput::OutputChannel;
		}
	}

#if WITH_EDITORONLY_DATA
	/** Handles deprecated properties */
	void PostSerialize(const FArchive& Ar);
#endif
};

template<>
struct TStructOpsTypeTraits<FMeshMergingSettings> : public TStructOpsTypeTraitsBase2<FMeshMergingSettings>
{
#if WITH_EDITORONLY_DATA
	enum
	{
		WithPostSerialize = true,
	};
#endif
};

/** Struct to store per section info used to populate data after (multiple) meshes are merged together */
struct FSectionInfo
{
	FSectionInfo() : Material(nullptr), MaterialSlotName(NAME_None), MaterialIndex(INDEX_NONE), StartIndex(INDEX_NONE), EndIndex(INDEX_NONE), bProcessed(false)	{}

	/** Material used by the section */
	class UMaterialInterface* Material;
	/** Name value for the section */
	FName MaterialSlotName;
	/** List of properties enabled for the section (collision, cast shadow etc) */
	TArray<FName> EnabledProperties;
	/** Original index of Material in the source data */
	int32 MaterialIndex;
	/** Index pointing to the start set of mesh indices that belong to this section */
	int32 StartIndex;
	/** Index pointing to the end set of mesh indices that belong to this section */
	int32 EndIndex;
	/** Used while baking out materials, to check which sections are and aren't being baked out */
	bool bProcessed;

	bool operator==(const FSectionInfo& Other) const
	{
		return Material == Other.Material && EnabledProperties == Other.EnabledProperties;
	}
};

/** Mesh instance-replacement settings */
USTRUCT(Blueprintable)
struct FMeshInstancingSettings
{
	GENERATED_BODY()

	ENGINE_API FMeshInstancingSettings();

	/** The actor class to attach new instance static mesh components to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, NoClear, Category="Instancing")
	TSubclassOf<AActor> ActorClassToUse;

	/** The number of static mesh instances needed before a mesh is replaced with an instanced version */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Instancing", meta=(ClampMin=1))
	int32 InstanceReplacementThreshold;

	/**
	 * Whether to skip the conversion to an instanced static mesh for meshes with vertex colors.
	 * Instanced static meshes do not support vertex colors per-instance, so conversion will lose
	 * this data.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Instancing")
	bool bSkipMeshesWithVertexColors;

	/**
	 * Whether split up instanced static mesh components based on their intersection with HLOD volumes
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Instancing", meta=(DisplayName="Use HLOD Volumes"))
	bool bUseHLODVolumes;

	/**
	 * Whether to use the Instanced Static Mesh Compoment or the Hierarchical Instanced Static Mesh Compoment
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Instancing", meta = (DisplayName = "Select the type of Instanced Component", DisallowedClasses = "/Script/Foliage.FoliageInstancedStaticMeshComponent"))
	TSubclassOf<UInstancedStaticMeshComponent> ISMComponentToUse;
};


UENUM()
enum class EMeshApproximationType : uint8
{
	MeshAndMaterials,
	MeshShapeOnly
};

UENUM()
enum class EMeshApproximationBaseCappingType : uint8
{
	NoBaseCapping = 0,
	ConvexPolygon = 1,
	ConvexSolid = 2
};


UENUM()
enum class EOccludedGeometryFilteringPolicy : uint8
{
	NoOcclusionFiltering = 0,
	VisibilityBasedFiltering = 1
};

UENUM()
enum class EMeshApproximationSimplificationPolicy : uint8
{
	FixedTriangleCount = 0,
	TrianglesPerArea = 1,
	GeometricTolerance = 2
};

UENUM()
enum class EMeshApproximationGroundPlaneClippingPolicy : uint8
{
	NoGroundClipping = 0,
	DiscardWithZPlane = 1,
	CutWithZPlane = 2,
	CutAndFillWithZPlane = 3
};


UENUM()
enum class EMeshApproximationUVGenerationPolicy : uint8
{
	PreferUVAtlas = 0,
	PreferXAtlas = 1,
	PreferPatchBuilder = 2
};


USTRUCT(Blueprintable)
struct FMeshApproximationSettings
{
	GENERATED_BODY()

	/** Type of output from mesh approximation process */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings)
	EMeshApproximationType OutputType = EMeshApproximationType::MeshAndMaterials;


	//
	// Mesh Generation Settings
	//

	/** Approximation Accuracy in Meters, will determine (eg) voxel resolution */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings, meta = (DisplayName = "Approximation Accuracy (meters)", ClampMin = "0.001"))
	float ApproximationAccuracy = 1.0f;

	/** Maximum allowable voxel count along main directions. This is a limit on ApproximationAccuracy. Max of 1290 (1290^3 is the last integer < 2^31, using a bigger number results in failures in TArray code & probably elsewhere) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = ShapeSettings, meta = (ClampMin = "64", ClampMax = "1290"))
	int32 ClampVoxelDimension = 1024;

	/** if enabled, we will attempt to auto-thicken thin parts or flat sheets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings)
	bool bAttemptAutoThickening = true;

	/** Multiplier on Approximation Accuracy used for auto-thickening */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings, meta = (ClampMin = "0.001", EditCondition = "bAttemptAutoThickening"))
	float TargetMinThicknessMultiplier = 1.5f;

	/** If enabled, tiny parts will be excluded from the mesh merging, which can improve performance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings)
	bool bIgnoreTinyParts = true;

	/** Multiplier on Approximation Accuracy used to define tiny-part threshold, using maximum bounding-box dimension */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings, meta = (ClampMin = "0.001", EditCondition = "bIgnoreTinyParts"))
	float TinyPartSizeMultiplier = 0.05f;


	/** Optional methods to attempt to close off the bottom of open meshes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings)
	EMeshApproximationBaseCappingType BaseCapping = EMeshApproximationBaseCappingType::NoBaseCapping;


	/** Winding Threshold controls hole filling at open mesh borders. Smaller value means "more/rounder" filling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = ShapeSettings, meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float WindingThreshold = 0.5f;

	/** If true, topological expand/contract is used to try to fill small gaps between objects. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings)
	bool bFillGaps = true;

	/** Distance in Meters to expand/contract to fill gaps */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = ShapeSettings, meta = (DisplayName = "Gap Filling Distance (meters)", ClampMin = "0.001", EditCondition = "bFillGaps"))
	float GapDistance = 0.1f;


	//
	// Output Mesh Filtering and Simplification Settings
	//

	/** Type of hidden geometry removal to apply */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SimplifySettings)
	EOccludedGeometryFilteringPolicy OcclusionMethod = EOccludedGeometryFilteringPolicy::VisibilityBasedFiltering;

	/** If true, then the OcclusionMethod computation is configured to try to consider downward-facing "bottom" geometry as occluded */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SimplifySettings)
	bool bOccludeFromBottom = true;

	/** Mesh Simplification criteria */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SimplifySettings)
	EMeshApproximationSimplificationPolicy SimplifyMethod = EMeshApproximationSimplificationPolicy::GeometricTolerance;

	/** Target triangle count for Mesh Simplification, for SimplifyMethods that use a Count*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SimplifySettings, meta = (ClampMin = "16", EditCondition = "SimplifyMethod == EMeshApproximationSimplificationPolicy::FixedTriangleCount" ))
	int32 TargetTriCount = 2000;

	/** Approximate Number of triangles per Square Meter, for SimplifyMethods that use such a constraint */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SimplifySettings, meta = (ClampMin = "0.01", EditCondition = "SimplifyMethod == EMeshApproximationSimplificationPolicy::TrianglesPerArea" ))
	float TrianglesPerM = 2.0f;

	/** Allowable Geometric Deviation in Meters when SimplifyMethod incorporates a Geometric Tolerance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SimplifySettings, meta = (DisplayName = "Geometric Deviation (meters)", ClampMin = "0.0001", EditCondition = "SimplifyMethod == EMeshApproximationSimplificationPolicy::GeometricTolerance"))
	float GeometricDeviation = 0.1f;

	/** Configure how the final mesh should be clipped with a ground plane, if desired */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SimplifySettings)
	EMeshApproximationGroundPlaneClippingPolicy GroundClipping = EMeshApproximationGroundPlaneClippingPolicy::NoGroundClipping;

	/** Z-Height for the ground clipping plane, if enabled */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SimplifySettings, meta = (EditCondition = "GroundClipping != EMeshApproximationGroundPlaneClippingPolicy::NoGroundClipping"))
	float GroundClippingZHeight = 0.0f;


	//
	// Mesh Normals and Tangents Settings
	//

	/** If true, normal angle will be used to estimate hard normals */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = NormalsSettings)
	bool bEstimateHardNormals = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = NormalsSettings, meta = (ClampMin = "0.0", ClampMax = "90.0", EditCondition = "bEstimateHardNormals"))
	float HardNormalAngle = 60.0f;


	//
	// Mesh UV Generation Settings
	//

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UVSettings)
	EMeshApproximationUVGenerationPolicy UVGenerationMethod = EMeshApproximationUVGenerationPolicy::PreferXAtlas;


	/** Number of initial patches mesh will be split into before computing island merging */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UVSettings, AdvancedDisplay, meta = (UIMin = "1", UIMax = "1000", ClampMin = "1", ClampMax = "99999999", EditCondition = "UVGenerationMethod == EMeshApproximationUVGenerationPolicy::PreferPatchBuilder"))
	int InitialPatchCount = 250;

	/** This parameter controls alignment of the initial patches to creases in the mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UVSettings, AdvancedDisplay, meta = (UIMin = "0.1", UIMax = "2.0", ClampMin = "0.01", ClampMax = "100.0", EditCondition = "UVGenerationMethod == EMeshApproximationUVGenerationPolicy::PreferPatchBuilder"))
	float CurvatureAlignment = 1.0f;

	/** Distortion/Stretching Threshold for island merging - larger values increase the allowable UV stretching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UVSettings, AdvancedDisplay, meta = (UIMin = "1.0", UIMax = "5.0", ClampMin = "1.0", EditCondition = "UVGenerationMethod == EMeshApproximationUVGenerationPolicy::PreferPatchBuilder"))
	float MergingThreshold = 1.5f;

	/** UV islands will not be merged if their average face normals deviate by larger than this amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = UVSettings, AdvancedDisplay, meta = (UIMin = "0.0", UIMax = "90.0", ClampMin = "0.0", ClampMax = "180.0", EditCondition = "UVGenerationMethod == EMeshApproximationUVGenerationPolicy::PreferPatchBuilder"))
	float MaxAngleDeviation = 45.0f;

	//
	// Output Static Mesh Settings
	//

	/** Whether to generate a nanite-enabled mesh */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSettings)
	bool bGenerateNaniteEnabledMesh = false;

	/** Which heuristic to use when generating the Nanite fallback mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = MeshSettings, meta = (EditConditionHides, EditCondition = "bGenerateNaniteEnabledMesh"))
	ENaniteFallbackTarget NaniteFallbackTarget = ENaniteFallbackTarget::Auto;

	/** Percentage of triangles to keep from source Nanite mesh for fallback. 1.0 = no reduction, 0.0 = no triangles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = MeshSettings, meta = (EditConditionHides, EditCondition = "bGenerateNaniteEnabledMesh && NaniteFallbackTarget == ENaniteFallbackTarget::PercentTriangles", ClampMin = 0, ClampMax = 1))
	float NaniteFallbackPercentTriangles = 1.0f;

	/** Reduce Nanite fallback mesh until at least this amount of error is reached relative to size of the mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = MeshSettings, meta = (EditConditionHides, EditCondition = "bGenerateNaniteEnabledMesh && NaniteFallbackTarget == ENaniteFallbackTarget::RelativeError", ClampMin = 0))
	float NaniteFallbackRelativeError = 1.0f;

	/** Whether ray tracing will be supported on this mesh. Disable this to save memory if the generated mesh will only be rendered in the distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSettings)
	bool bSupportRayTracing = true;

	/** Whether to allow distance field to be computed for this mesh. Disable this to save memory if the generated mesh will only be rendered in the distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSettings)
	bool bAllowDistanceField = true;


	//
	// Material Baking Settings
	//

	/** If Value is > 1, Multisample output baked textures by this amount in each direction (eg 4 == 16x supersampling) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MaterialSettings, meta = (ClampMin = "0", ClampMax = "8", UIMin = "0", UIMax = "4"))
	int32 MultiSamplingAA = 0;

	/** If Value is zero, use MaterialSettings resolution, otherwise override the render capture resolution */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MaterialSettings, meta = (ClampMin = "0"))
	int32 RenderCaptureResolution = 2048;

	/** Material generation settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MaterialSettings)
	FMaterialProxySettings MaterialSettings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = MaterialSettings, meta = (ClampMin = "5.0", ClampMax = "160.0"))
	float CaptureFieldOfView = 30.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = MaterialSettings, meta = (ClampMin = "0.001", ClampMax = "1000.0"))
	float NearPlaneDist = 1.0f;


	//
	// Performance Settings
	//


	/** If true, LOD0 Render Meshes (or Nanite Fallback meshes) are used instead of Source Mesh data. This can significantly reduce computation time and memory usage, but potentially at the cost of lower quality output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PerformanceSettings)
	bool bUseRenderLODMeshes = false;

	/** If true, a faster mesh simplfication strategy will be used. This can significantly reduce computation time and memory usage, but potentially at the cost of lower quality output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PerformanceSettings)
	bool bEnableSimplifyPrePass = true;

	/** If false, texture capture and baking will be done serially after mesh generation, rather than in parallel when possible. This will reduce the maximum memory requirements of the process.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PerformanceSettings)
	bool bEnableParallelBaking = true;

	//
	// Debug Output Settings
	//


	/** If true, print out debugging messages */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = DebugSettings)
	bool bPrintDebugMessages = false;

	/** If true, write the full mesh triangle set (ie flattened, non-instanced) used for mesh generation. Warning: this asset may be extremely large!! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = DebugSettings)
	bool bEmitFullDebugMesh = false;


	/** Equality operator. */
	bool operator==(const FMeshApproximationSettings& Other) const
	{
		return OutputType == Other.OutputType
			&& ApproximationAccuracy == Other.ApproximationAccuracy
			&& ClampVoxelDimension == Other.ClampVoxelDimension
			&& bAttemptAutoThickening == Other.bAttemptAutoThickening
			&& TargetMinThicknessMultiplier == Other.TargetMinThicknessMultiplier
			&& BaseCapping == Other.BaseCapping
			&& WindingThreshold == Other.WindingThreshold
			&& bFillGaps == Other.bFillGaps
			&& GapDistance == Other.GapDistance
			&& OcclusionMethod == Other.OcclusionMethod
			&& SimplifyMethod == Other.SimplifyMethod
			&& TargetTriCount == Other.TargetTriCount
			&& TrianglesPerM == Other.TrianglesPerM
			&& GeometricDeviation == Other.GeometricDeviation
			&& bGenerateNaniteEnabledMesh == Other.bGenerateNaniteEnabledMesh
			&& NaniteFallbackTarget == Other.NaniteFallbackTarget
			&& NaniteFallbackPercentTriangles == Other.NaniteFallbackPercentTriangles
			&& NaniteFallbackRelativeError == Other.NaniteFallbackRelativeError
			&& bSupportRayTracing == Other.bSupportRayTracing
			&& bAllowDistanceField == Other.bAllowDistanceField
			&& MultiSamplingAA == Other.MultiSamplingAA
			&& RenderCaptureResolution == Other.RenderCaptureResolution
			&& MaterialSettings == Other.MaterialSettings
			&& CaptureFieldOfView == Other.CaptureFieldOfView
			&& NearPlaneDist == Other.NearPlaneDist
			&& bPrintDebugMessages == Other.bPrintDebugMessages
			&& bEmitFullDebugMesh == Other.bEmitFullDebugMesh;
	}

	/** Inequality. */
	bool operator!=(const FMeshApproximationSettings& Other) const
	{
		return !(*this == Other);
	}

#if WITH_EDITORONLY_DATA
	/** Handles deprecated properties */
	void PostSerialize(const FArchive& Ar);
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	float NaniteProxyTrianglePercent_DEPRECATED = 0;
#endif
};

template<>
struct TStructOpsTypeTraits<FMeshApproximationSettings> : public TStructOpsTypeTraitsBase2<FMeshApproximationSettings>
{
#if WITH_EDITORONLY_DATA
	enum
	{
		WithPostSerialize = true,
	};
#endif
};
