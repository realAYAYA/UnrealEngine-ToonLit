// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerModel.h"
#include "MLDeformerVizSettings.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "GeometryCache.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "MLDeformerGeomCacheModel.generated.h"

class UMLDeformerAsset;
class UMLDeformerGeomCacheVizSettings;

/**
 * A ML Deformer model that has a geometry cache as target mesh.
 * Use this in combination with UMLDeformerGeomCacheVizSettings, FMLDeformerGeomCacheEditorModel, FMLDeformerGeomCacheVizSettingsDetails and FMLDeformerGeomCacheModelDetails.
 */
UCLASS()
class MLDEFORMERFRAMEWORK_API UMLDeformerGeomCacheModel
	: public UMLDeformerModel
{
	GENERATED_BODY()

public:
	// UObject overrides.
	virtual void Serialize(FArchive& Archive) override;
	// ~END UObject overrides.

	// UMLDeformerModel overrides.
#if WITH_EDITORONLY_DATA
	virtual bool HasTrainingGroundTruth() const override	{ return (GetGeometryCache() != nullptr); }
	virtual void SampleGroundTruthPositions(float SampleTime, TArray<FVector3f>& OutPositions) override;
#endif
#if WITH_EDITOR
	virtual void UpdateNumTargetMeshVertices() override;
	// ~END UMLDeformerModel overrides.

	/**
	 * Get the visualization settings, already cast to a UMLDeformerGeomCacheVizSettings type.
	 * @return A pointer to the visualization settings.
	 */
	UMLDeformerGeomCacheVizSettings* GetGeomCacheVizSettings() const;
#endif

#if WITH_EDITORONLY_DATA
	/** 
	 * Get the geometry cache that represents the target deformation.
	 * This is our training target.
	 * @return A pointer to the geometry cache, in read-only mode.
	 */
	const UGeometryCache* GetGeometryCache() const			{ return GeometryCache.LoadSynchronous(); }

	/** 
	 * Get the geometry cache that represents the target deformation.
	 * This is our training target.
	 * @return A pointer to the geometry cache.
	 */
	UGeometryCache* GetGeometryCache()						{ return GeometryCache.LoadSynchronous(); }

	/**
	 * Get the mapping between geometry cache tracks and meshes inside the skeletal mesh.
	 * This lets us know what parts of the skeletal mesh are related to what geometry cache tracks.
	 * Once we have that, we can calculate deltas between the two.
	 * @return The geometry cache mesh to skeletal mesh mappings.
	 */
	TArray<UE::MLDeformer::FMLDeformerGeomCacheMeshMapping>& GetGeomCacheMeshMappings()				{ return MeshMappings; }
	const TArray<UE::MLDeformer::FMLDeformerGeomCacheMeshMapping>& GetGeomCacheMeshMappings() const { return MeshMappings; }

	// Get property names.
	static FName GetGeometryCachePropertyName()				{ return GET_MEMBER_NAME_CHECKED(UMLDeformerGeomCacheModel, GeometryCache); }

private:
	/** The mappings between the geometry cache tracks and skeletal mesh imported meshes. */
	TArray<UE::MLDeformer::FMLDeformerGeomCacheMeshMapping> MeshMappings;

	/** The geometry cache that represents the target deformations. */
	UPROPERTY(EditAnywhere, Category = "Target Mesh")
	TSoftObjectPtr<UGeometryCache> GeometryCache = nullptr;
#endif // WITH_EDITORONLY_DATA
};
