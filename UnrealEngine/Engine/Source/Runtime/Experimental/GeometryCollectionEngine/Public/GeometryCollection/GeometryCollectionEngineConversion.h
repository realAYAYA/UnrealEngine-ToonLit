// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/World.h"

class UStaticMesh;
class USkeletalMesh;
class USkeletalMeshComponent;
class UGeometryCollection;
class UMaterialInterface;
class UGeometryCollectionComponent;
class FGeometryCollection;
class UBodySetup;
class FSkeletalMeshLODRenderData;
struct FMeshDescription;

typedef TTuple<const UStaticMesh *, const UStaticMeshComponent *, FTransform> GeometryCollectionStaticMeshConversionTuple;
typedef TTuple<const USkeletalMesh *, const USkeletalMeshComponent *, FTransform> GeometryCollectionSkeletalMeshConversionTuple;

/**
 * The public interface to this module
 */
class GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionEngineConversion
{
public:

	/**
	 * Appends materials to a GeometryCollectionComponent.
	 * @param Materials : Materials fetched from the StaticMeshComponent used to configure this geometry
	 * @param GeometryCollection  : Collection to append the mesh into.
	 */
	static int32 AppendMaterials(const TArray<UMaterialInterface*>& Materials, UGeometryCollection* GeometryCollectionObject, bool bAddInteriorCopy);

	/**
	 * Appends a MeshDescription to a GeometryCollection.
	 * @param MeshDescription : Const mesh description to read vertex/normals/index data from
	 * @param Name : A name to use for the Mesh, e.g. from the source UObject's GetName()
	 * @param StartMaterialIndex : Index of materials to be used, e.g. as returned by AppendMaterials() (see below)
	 * @param StaticMeshTransform : Mesh transform.
	 * @param GeometryCollection  : Collection to append the mesh into.
	 * @param BodySetup : Optional collision setup to transfer to the geometry collection
	 * @param bReindexMaterials	: Whether to reindex materials -- if appending multiple meshes, pass false and call ReindexMaterials afterwards
	 */
	static void AppendMeshDescription(const FMeshDescription* MeshDescription, const FString& Name, int32 StartMaterialIndex, const FTransform& StaticMeshTransform,
		FGeometryCollection* GeometryCollection, UBodySetup* BodySetup = nullptr, bool bReindexMaterials = true, bool bAddInternalMaterials = true);

	/**
	 * Get a HiRes (or LOD 0 if no HiRes available) MeshDescription for the given static mesh, and make sure it includes normals and tangents.
	 * (i.e., to pass to AppendMeshDescription)
	 */
	static FMeshDescription* GetMaxResMeshDescriptionWithNormalsAndTangents(const UStaticMesh* StaticMesh);

	/**
	 * Appends a static mesh to a GeometryCollectionComponent.
	 * @param StaticMesh : Const mesh to read vertex/normals/index data from
	 * @param Materials : Materials fetched from the StaticMeshComponent used to configure this geometry
	 * @param StaticMeshTransform : Mesh transform.
	 * @param GeometryCollection  : Collection to append the mesh into.
	 * @param bReindexMaterials	: Whether to reindex materials -- if appending multiple meshes, pass false and call ReindexMaterials afterwards
	 * @return true if succeeded in appending a mesh
	 */
	static bool AppendStaticMesh(const UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>& Materials, const FTransform& StaticMeshTransform, 
		UGeometryCollection* GeometryCollectionObject, bool bReindexMaterials = true, bool bAddInternalMaterials = true, bool bSplitComponents = false);

	// TODO: consider having the below fn return bool, and have a helper that returns the StartMaterialIndex from UGeometryCollection
	//  before any materials are actually added, so we can call the AppendMaterial function after, and not need to use its return value ...
	//  This would let the is-there-a-valid-mesh logic live inside this function
	// NOTE: and similar change will need to propagate to similar places ...

	/**
	 * Appends a static mesh to a GeometryCollectionComponent.
	 * @param StaticMesh : Const mesh to read vertex/normals/index data from
	 * @param StartMaterialIndex : Index of materials to be used, e.g. as returned by AppendMaterials() (see below)
	 * @param StaticMeshTransform : Mesh transform.
	 * @param GeometryCollection  : Collection to append the mesh into.
	 * @param bReindexMaterials	: Whether to reindex materials -- if appending multiple meshes, pass false and call ReindexMaterials afterwards
	 * @return true if succeeded in appending a mesh
	 */
	static bool AppendStaticMesh(const UStaticMesh* StaticMesh, int32 StartMaterialIndex, const FTransform& StaticMeshTransform, 
		FGeometryCollection* GeometryCollection, bool bReindexMaterials = true, bool bAddInternalMaterials = true, bool bSplitComponents = false);

	/**
	*  Appends a static mesh to a GeometryCollectionComponent.
	*  @param StaticMeshComponent : Const mesh to read vertex/normals/index data from
	*  @param StaticMeshTransform : Mesh transform.
	*  @param GeometryCollection  : Collection to append the mesh into.
	*/
	static void AppendStaticMesh(const UStaticMesh* StaticMesh, const UStaticMeshComponent *StaticMeshComponent, const FTransform& StaticMeshTransform, UGeometryCollection* GeometryCollection,
		bool bReindexMaterials = true, bool bAddInternalMaterials = true, bool bSplitComponents = false);

	/**
	*  Appends a skeletal mesh to a GeometryCollectionComponent.
	*  @param SkeletalMeshComponent : Const mesh to read vertex/normals/index data from
	*  @param SkeletalMeshTransform : Mesh transform.
	*  @param GeometryCollection    : Collection to append the mesh into.
	*/
	static void AppendSkeletalMesh(const USkeletalMesh* SkeletalMesh, const USkeletalMeshComponent *SkeletalMeshComponent, const FTransform& SkeletalMeshTransform, UGeometryCollection* GeometryCollection, bool bReindexMaterials = true);

	/**
	*  Appends a skeletal mesh to a GeometryCollection.
	*  @param SkeletalMesh : Const mesh to read vertex/normals/index data from
	*  @param MaterialStartIndex : First index of materials to use, as returned by AppendSkeletalMeshMaterials
	*  @param SkeletalMeshTransform : Mesh transform.
	*  @param GeometryCollection    : Collection to append the mesh into.
	*/
	static bool AppendSkeletalMesh(const USkeletalMesh* SkeletalMesh, int32 MaterialStartIndex, const FTransform& SkeletalMeshTransform, FGeometryCollection* GeometryCollection, bool bReindexMaterials = true);

	/**
	*  Appends a skeletal mesh to a GeometryCollectionComponent.
	*  @param SkeletalMeshComponent : Const mesh to read vertex/normals/index data from
	*  @param SkeletalMeshTransform : Mesh transform.
	*  @param GeometryCollection    : Collection to append the mesh into.
	*/
	static int32 AppendSkeletalMeshMaterials(const USkeletalMesh* SkeletalMesh, const USkeletalMeshComponent* SkeletalMeshComponent, UGeometryCollection* GeometryCollectionObject);

	static const FSkeletalMeshLODRenderData* GetSkeletalMeshLOD(const USkeletalMesh* SkeletalMesh, int32 LOD);

	static int32 AppendGeometryCollectionMaterials(const UGeometryCollection* SourceGeometryCollection, const UGeometryCollectionComponent* GeometryCollectionComponent, UGeometryCollection* TargetGeometryCollectionObject);

	static void AppendGeometryCollectionInstancedMeshes(const UGeometryCollection* SourceGeometryCollectionObject, UGeometryCollection* TargetGeometryCollectionObject, int32 TargetTransformStartIndex);

	/**
	 * Appends a GeometryCollection to another GeometryCollection.
	 * @param SourceGeometryCollection : Const GeometryCollection to read vertex/normals/index data from
	 * @param MaterialStartIndex : First index of materials to use
	 * @param GeometryCollectionTransform : GeometryCollection transform.
	 * @param TargetGeometryCollection  : Collection to append the GeometryCollection into.
	 * @param bReindexMaterials	: Whether to reindex materials -- if appending multiple meshes, pass false and call ReindexMaterials afterwards
	 * @return true if succeeded in appending a geometry collection
	 */
	static bool AppendGeometryCollection(const FGeometryCollection* SourceGeometryCollection, int32 MaterialStartIndex, const FTransform& GeometryCollectionTransform, FGeometryCollection* TargetGeometryCollection, bool bReindexMaterials = true);

	/**
	*  Appends a GeometryCollection to a GeometryCollectionComponent.
	*  @param SourceGeometryCollection : Const GeometryCollection to read vertex/normals/index data from
	*  @param Materials : Materials fetched from the GeometryCollectionComponent used to configure this geometry
	*  @param GeometryCollectionTransform : GeometryCollection transform.
	*  @param TargetGeometryCollection  : Collection to append the GeometryCollection into.
	*/
	static void AppendGeometryCollection(const UGeometryCollection* SourceGeometryCollection, const TArray<UMaterialInterface*>& Materials, const FTransform& GeometryCollectionTransform, UGeometryCollection* TargetGeometryCollectionObject, bool ReindexMaterials = true);

	/**
	*  Appends a GeometryCollection to a GeometryCollectionComponent.
	*  @param GeometryCollectionComponent : Const GeometryCollection to read vertex/normals/index data from
	*  @param GeometryCollectionTransform : GeometryCollection transform.
	*  @param TargetGeometryCollection  : Collection to append the GeometryCollection into.
	*/
	static void AppendGeometryCollection(const UGeometryCollection* SourceGeometryCollection, const UGeometryCollectionComponent* GeometryCollectionComponent, const FTransform& GeometryCollectionTransform, UGeometryCollection* TargetGeometryCollectionObject, bool ReindexMaterials = true);

};