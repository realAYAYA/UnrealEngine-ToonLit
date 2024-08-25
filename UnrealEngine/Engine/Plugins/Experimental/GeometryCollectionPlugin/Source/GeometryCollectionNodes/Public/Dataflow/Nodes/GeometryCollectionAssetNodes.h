// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollectionObject.h"

#include "GeometryCollectionAssetNodes.generated.h"

class UGeometryCollection;
class UStaticMesh;

USTRUCT(meta = (DataflowGeometryCollection, DataflowTerminal))
struct FGeometryCollectionTerminalDataflowNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGeometryCollectionTerminalDataflowNode, "GeometryCollectionTerminal", "Terminal", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:

	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection", DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	/** Materials array to use for this asset */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Materials", DisplayName = "Materials"))
	TArray<TObjectPtr<UMaterial>> Materials;

	/** array of instanced meshes*/
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "InstancedMeshes", DisplayName = "InstancedMeshes"))
	TArray<FGeometryCollectionAutoInstanceMesh> InstancedMeshes;

	FGeometryCollectionTerminalDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context) const override;
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override;
};


/**
 * Get Current geometry collection asset 
 * Note : Use with caution as this may get replaced in a near future for a more generic getAsset node
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetGeometryCollectionAssetDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetGeometryCollectionAssetDataflowNode, "GetGeometryCollectionAsset", "GeometryCollection|Asset", "")

public:
	FGetGeometryCollectionAssetDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

	/** Asset this data flow graph instance is assigned to */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Asset"))
	TObjectPtr<UGeometryCollection> Asset;
};


/**
 * Get the list of the original mesh information used to create a specific geometryc collection asset
 * each entry contains a mesh, a transform and a list of override materials
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGetGeometryCollectionSourcesDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetGeometryCollectionSourcesDataflowNode, "GetGeometryCollectionSources", "GeometryCollection|Asset", "")

public:
	FGetGeometryCollectionSourcesDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	
	/** Asset to get geometry sources from */
	UPROPERTY(meta = (DataflowInput, DisplayName = "Asset"))
	TObjectPtr<UGeometryCollection> Asset;
	
	/** array of geometry sources */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Sources"))
	TArray<FGeometryCollectionSource> Sources;
};


/**
 * create a geometry collection from a set of geometry sources    
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FCreateGeometryCollectionFromSourcesDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FCreateGeometryCollectionFromSourcesDataflowNode, "CreateGeometryCollectionFromSources", "GeometryCollection|Asset", "")

public:
	FCreateGeometryCollectionFromSourcesDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	
	/** array of geometry sources */
	UPROPERTY(meta = (DataflowInput, DisplayName = "Sources"))
	TArray<FGeometryCollectionSource> Sources;

	/** Geometry collection newly created */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	/** Materials array to use for this asset */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Materials"))
	TArray<TObjectPtr<UMaterial>> Materials;

	/** array of instanced meshes*/
	UPROPERTY(meta = (DataflowOutput, DisplayName = "InstancedMeshes"))
	TArray<FGeometryCollectionAutoInstanceMesh> InstancedMeshes;
};


/**
 * Create a geometry collection from a UStaticMesh
 */
USTRUCT(meta = (DataflowContext = "GeometryCollection", DataflowGeometryCollection, DataflowTerminal))
struct FStaticMeshToCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FStaticMeshToCollectionDataflowNode, "StaticMeshToCollection", "GeometryCollection|Asset", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	/** Asset input */
	UPROPERTY(EditAnywhere, Category = "Asset");
	TObjectPtr<UStaticMesh> StaticMesh;

	/** Set the internal faces from material index */
	UPROPERTY(EditAnywhere, Category = "Asset");
	bool bSetInternalFromMaterialIndex = true;

	/** Split components */
	UPROPERTY(EditAnywhere, Category = "Asset");
	bool bSplitComponents = false;

	/** Geometry collection newly created */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	/** Materials array to use for this asset */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Materials"))
	TArray<TObjectPtr<UMaterial>> Materials;

	/** Array of instanced meshes*/
	UPROPERTY(meta = (DataflowOutput, DisplayName = "InstancedMeshes"))
	TArray<FGeometryCollectionAutoInstanceMesh> InstancedMeshes;

	FStaticMeshToCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

namespace Dataflow
{
	void GeometryCollectionEngineAssetNodes();
}