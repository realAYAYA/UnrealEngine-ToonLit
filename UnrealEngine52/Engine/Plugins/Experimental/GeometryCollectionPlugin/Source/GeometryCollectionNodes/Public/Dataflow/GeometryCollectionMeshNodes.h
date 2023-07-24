// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Dataflow/DataflowSelection.h"
#include "DynamicMesh/DynamicMesh3.h"

#include "GeometryCollectionMeshNodes.generated.h"

class FGeometryCollection;
class UStaticMesh;
class UDynamicMesh;

/**
 *
 * Converts points into a DynamicMesh
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FPointsToMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FPointsToMeshDataflowNode, "PointsToMesh", "Mesh|Utilities", "")

public:
	/** Points input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TArray<FVector> Points;

	/** Mesh output */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Mesh triangle count */
	UPROPERTY(meta = (DataflowOutput))
	int32 TriangleCount = 0;

	FPointsToMeshDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Points);
		RegisterOutputConnection(&Mesh);
		RegisterOutputConnection(&TriangleCount);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a BoundingBox into a DynamicMesh
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FBoxToMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBoxToMeshDataflowNode, "BoxToMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FDynamicMesh3"), "Mesh")

public:
	/** BoundingBox input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FBox Box = FBox(ForceInit);

	/** Mesh output */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Mesh triangle count */
	UPROPERTY(meta = (DataflowOutput))
	int32 TriangleCount = 0;

	FBoxToMeshDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Box);
		RegisterOutputConnection(&Mesh);
		RegisterOutputConnection(&TriangleCount);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Collects information from the DynamicMesh and outputs it into a formatted string
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshInfoDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshInfoDataflowNode, "MeshInfo", "Mesh|Utilities", "")

public:
	/** DynamicMesh for the information */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Formatted output string */
	UPROPERTY(meta = (DataflowOutput))
	FString InfoString = FString("");

	FMeshInfoDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh);
		RegisterOutputConnection(&InfoString);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a DynamicMesh to a Collection
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshToCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshToCollectionDataflowNode, "MeshToCollection", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	/** DynamicMesh to convert */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Output Collection */
	UPROPERTY(meta = (DataflowOutput))
	FManagedArrayCollection Collection;

	FMeshToCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh);
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a Collection to a DynamicMesh
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FCollectionToMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCollectionToMeshDataflowNode, "CollectionToMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FDynamicMesh3"), "Mesh")

public:
	/** Collection to convert*/
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "General", meta = (DisplayName = "Center Pivot"));
	bool bCenterPivot = false;

	/** Output DynamicMesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	FCollectionToMeshDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Mesh);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Converts a StaticMesh into a DynamicMesh
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FStaticMeshToMeshDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FStaticMeshToMeshDataflowNode, "StaticMeshToMesh", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FDynamicMesh3"), "Mesh")

public:
	/** StaticMesh to convert */
	UPROPERTY(EditAnywhere, Category = "StaticMesh");
	TObjectPtr<UStaticMesh> StaticMesh;

	/** Output the HiRes representation, if set to true and HiRes doesn't exist it will output empty mesh */
	UPROPERTY(EditAnywhere, Category = "StaticMesh", meta = (DisplayName = "Use HiRes"));
	bool bUseHiRes = false;

	/** Specifies the LOD level to use */
	UPROPERTY(EditAnywhere, Category = "StaticMesh", meta = (DisplayName = "LOD Level"));
	int32 LODLevel = 0;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	FStaticMeshToMeshDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Mesh);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Appends two meshes
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshAppendDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshAppendDataflowNode, "MeshAppend", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FDynamicMesh3"), "Mesh")

public:
	/** Mesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh1;

	/** Mesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh2;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	FMeshAppendDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh1);
		RegisterInputConnection(&Mesh2);
		RegisterOutputConnection(&Mesh);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


UENUM(BlueprintType)
enum class EMeshBooleanOperationEnum : uint8
{
	Dataflow_MeshBoolean_Union UMETA(DisplayName = "Union"),
	Dataflow_MeshBoolean_Intersect UMETA(DisplayName = "Intersect"),
	Dataflow_MeshBoolean_Difference UMETA(DisplayName = "Difference"),
	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

/**
 *
 * Mesh boolean (Union, Intersect, Difference) between two meshes
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshBooleanDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshBooleanDataflowNode, "MeshBoolean", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FDynamicMesh3"), "Mesh")

public:
	/** Boolean operation */
	UPROPERTY(EditAnywhere, Category = "Boolean");
	EMeshBooleanOperationEnum Operation = EMeshBooleanOperationEnum::Dataflow_MeshBoolean_Intersect;

	/** Mesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh1;

	/** Mesh input */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh2;

	/** Output mesh */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	FMeshBooleanDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh1);
		RegisterInputConnection(&Mesh2);
		RegisterOutputConnection(&Mesh);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Copies the same mesh with scale onto points
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FMeshCopyToPointsDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMeshCopyToPointsDataflowNode, "MeshCopyToPoints", "Mesh|Utilities", "")
	DATAFLOW_NODE_RENDER_TYPE(FName("FDynamicMesh3"), "Mesh")

public:
	/** Points to copy meshes onto */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TArray<FVector> Points;

	/** Mesh to copy onto points */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> MeshToCopy;

	/** Scale appied to the mesh */
	UPROPERTY(EditAnywhere, Category = "Copy");
	float Scale = 1.f;

	/** Copied meshes */
	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UDynamicMesh> Mesh;

	FMeshCopyToPointsDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Points);
		RegisterInputConnection(&MeshToCopy);
		RegisterOutputConnection(&Mesh);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


/**
 *
 * Outputs Mesh data
 *
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetMeshDataDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetMeshDataDataflowNode, "GetMeshData", "Mesh|Utilities", "")

public:
	/** Mesh for the data */
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	TObjectPtr<UDynamicMesh> Mesh;

	/** Number of vertices */
	UPROPERTY(meta = (DataflowOutput))
	int32 VertexCount = 0;

	/** Number of edges */
	UPROPERTY(meta = (DataflowOutput))
	int32 EdgeCount = 0;

	/** Number of triangles */
	UPROPERTY(meta = (DataflowOutput))
	int32 TriangleCount = 0;

	FGetMeshDataDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Mesh);
		RegisterOutputConnection(&VertexCount);
		RegisterOutputConnection(&EdgeCount);
		RegisterOutputConnection(&TriangleCount);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

};


namespace Dataflow
{
	void GeometryCollectionMeshNodes();
}

