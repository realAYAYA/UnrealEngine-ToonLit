// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportTestFunctionsBase.h"
#include "InterchangeTestFunction.h"
#include "StaticMeshImportTestFunctions.generated.h"

struct FInterchangeTestFunctionResult;
class UStaticMesh;


UENUM(meta = (Bitmask))
enum class EStaticMeshImportTestGroundTruthBitflags : uint32
{
	CheckLodCountEqual                = (1 << 0),
	CheckVertexCountEqual             = (1 << 1),
	CheckTriangleCountEqual           = (1 << 2),
	CheckUVChannelCountEqual          = (1 << 3),
	CheckCollisionPrimitiveCountEqual = (1 << 4),
	CheckVertexPositionsEqual         = (1 << 5),
	CheckNormalsEqual                 = (1 << 6)
};
ENUM_CLASS_FLAGS(EStaticMeshImportTestGroundTruthBitflags);


UCLASS()
class INTERCHANGETESTS_API UStaticMeshImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the expected number of static meshes are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckImportedStaticMeshCount(const TArray<UStaticMesh*>& Meshes, int32 ExpectedNumberOfImportedStaticMeshes);

	/** Check whether the vertex count for the given LOD is as expected */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckVertexCount(UStaticMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfVertices);

	/** Check whether the vertex count in the built render data for the given LOD is as expected */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckRenderVertexCount(UStaticMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfRenderVertices);

	/** Check whether the mesh has the expected number of LODs */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLodCount(UStaticMesh* Mesh, int32 ExpectedNumberOfLods);

	/** Check whether the mesh has the expected number of material slots */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckMaterialSlotCount(UStaticMesh* Mesh, int32 ExpectedNumberOfMaterialSlots);

	/** Check whether the mesh has the expected number of polygon groups for the given LOD */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckPolygonGroupCount(UStaticMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfPolygonGroups);

	/** Check whether the built render data for the given mesh LOD has the expected number of sections */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSectionCount(UStaticMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfSections);

	/** Check whether the mesh has the expected number of triangles for the given LOD */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckTotalTriangleCount(UStaticMesh* Mesh, int32 LodIndex, int32 ExpectedTotalNumberOfTriangles);

	/** Check whether the mesh has the expected number of polygons for the given LOD */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckTotalPolygonCount(UStaticMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfPolygons);

	/** Check whether the mesh contains any quads or ngons */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckThatMeshHasQuadsOrNgons(UStaticMesh* Mesh, int32 LodIndex, bool bMeshHasQuadsOrNgons);

	/** Check whether the given polygon group of the given LOD has the expected number of triangles */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckTriangleCountInPolygonGroup(UStaticMesh* Mesh, int32 LodIndex, int32 PolygonGroupIndex, int32 ExpectedNumberOfTriangles);

	/** Check whether the given polygon group of the given LOD has the expected number of polygons */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckPolygonCountInPolygonGroup(UStaticMesh* Mesh, int32 LodIndex, int32 PolygonGroupIndex, int32 ExpectedNumberOfPolygons);

	/** Check whether the mesh has the expected number of UV channels */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckUVChannelCount(UStaticMesh* Mesh, int32 LodIndex, int32 ExpectedNumberOfUVChannels);

	/** Check whether the material slot name for the given polygon group in the given LOD is as expected */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckPolygonGroupImportedMaterialSlotName(UStaticMesh* Mesh, int32 LodIndex, int32 PolygonGroupIndex, const FString& ExpectedImportedMaterialSlotName);

	/** Check whether the section index in the built render data for the given LOD is referencing the expected material index */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSectionMaterialIndex(UStaticMesh* Mesh, int32 LodIndex, int32 SectionIndex, int32 ExpectedMaterialIndex);

	/** Check whether the section index in the built render data for the given LOD is referencing the expected material */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSectionMaterialName(UStaticMesh* Mesh, int32 LodIndex, int32 SectionIndex, const FString& ExpectedMaterialName);

	/** Check whether the vertex of the given index is at the expected position */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckVertexIndexPosition(UStaticMesh* Mesh, int32 LodIndex, int32 VertexIndex, const FVector& ExpectedVertexPosition);

	/** Check whether the expected number of simple collision primitives were imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSimpleCollisionPrimitiveCount(UStaticMesh* Mesh, int32 ExpectedSphereElementCount, int32 ExpectedBoxElementCount, int32 ExpectedCapsuleElementCount, int32 ExpectedConvexElementCount, int32 ExpectedTaperedCapsuleElementCount);

	/** Check whether the expected number of sockets were imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSocketCount(UStaticMesh* Mesh, int32 ExpectedSocketCount);

	/** Check whether the given socket index has the expected name */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSocketName(UStaticMesh* Mesh, int32 SocketIndex, const FString& ExpectedSocketName);

	/** Check whether the given socket index has the expected location */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSocketLocation(UStaticMesh* Mesh, int32 SocketIndex, const FVector& ExpectedSocketLocation);

	/** Check whether the mesh is equivalent to a ground truth asset */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckAgainstGroundTruth(UStaticMesh* Mesh, TSoftObjectPtr<UStaticMesh> MeshToCompare,
		bool bCheckVertexCountEqual = true,
		bool bCheckTriangleCountEqual = true,
		bool bCheckUVChannelCountEqual = true,
		bool bCheckCollisionPrimitiveCountEqual = true,
		bool bCheckVertexPositionsEqual = true,
		bool bCheckNormalsEqual = true
	);

	/** Check whether the mesh source model has the expected build settings */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckBuildSettings(UStaticMesh* Mesh, int32 LodIndex, const FMeshBuildSettings& ExpectedBuildSettings);

	/** Check whether the mesh has the expected Nanite settings */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckNaniteSettings(UStaticMesh* Mesh, const FMeshNaniteSettings& ExpectedNaniteSettings);
};
