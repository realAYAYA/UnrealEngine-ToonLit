// Copyright Epic Games, Inc. All Rights Reserved.
#include "MLDeformerGeomCacheModel.h"
#include "MLDeformerGeomCacheVizSettings.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "MLDeformerComponent.h"
#include "MLDeformerAsset.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "GeometryCache.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerGeomCacheModel)

#define LOCTEXT_NAMESPACE "MLDeformerGeomCacheModel"

void UMLDeformerGeomCacheModel::Serialize(FArchive& Archive)
{
	#if WITH_EDITOR
		if (Archive.IsSaving() && Archive.IsCooking())
		{
			GeometryCache = nullptr;
		}
	#endif

	Super::Serialize(Archive);
}

#if WITH_EDITOR
void UMLDeformerGeomCacheModel::UpdateNumTargetMeshVertices()
{
	UGeometryCache* GeomCache = GetGeometryCache();
	if (GeomCache)
	{
		SetNumTargetMeshVerts(UE::MLDeformer::ExtractNumImportedGeomCacheVertices(GeomCache));
	}
}

UMLDeformerGeomCacheVizSettings* UMLDeformerGeomCacheModel::GetGeomCacheVizSettings() const
{
	return Cast<UMLDeformerGeomCacheVizSettings>(GetVizSettings());
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UMLDeformerGeomCacheModel::SampleGroundTruthPositions(float SampleTime, TArray<FVector3f>& OutPositions)
{
	const UMLDeformerGeomCacheVizSettings* GeomCacheVizSettings = GetGeomCacheVizSettings();
	check(GeomCacheVizSettings);

	UGeometryCache* GeomCache = GeomCacheVizSettings->GetTestGroundTruth();
	if (GeomCache == nullptr)
	{
		OutPositions.Reset();
		return;
	}

	if (MeshMappings.IsEmpty())
	{
		TArray<FString> FailedImportedMeshNames;
		TArray<FString> VertexMisMatchNames;
		UE::MLDeformer::GenerateGeomCacheMeshMappings(GetSkeletalMesh(), GeomCache, MeshMappings, FailedImportedMeshNames, VertexMisMatchNames, /**bSuppressLog*/true);
	}

	UE::MLDeformer::SampleGeomCachePositions(
		0,
		SampleTime,
		MeshMappings,
		GetSkeletalMesh(),
		GeomCache,
		GetAlignmentTransform(),
		OutPositions);
}
#endif

#undef LOCTEXT_NAMESPACE
