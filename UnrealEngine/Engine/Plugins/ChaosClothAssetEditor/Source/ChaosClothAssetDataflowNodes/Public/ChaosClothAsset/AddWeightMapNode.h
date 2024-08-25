// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowTerminalNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "AddWeightMapNode.generated.h"

UENUM()
enum class EChaosClothAssetWeightMapTransferType : uint8
{
	/** Transfer weight maps from the 2D simulation mesh (pattern against pattern). */
	Use2DSimMesh UMETA(DisplayName = "Use 2D Sim Mesh"),
	/** Transfer weight maps from the 3D simulation mesh (rest mesh against rest mesh). */
	Use3DSimMesh UMETA(DisplayName = "Use 3D Sim Mesh"),
};

/** Which mesh to update with the corresponding weight map */
UENUM()
enum class EChaosClothAssetWeightMapMeshType : uint8
{
	Simulation,
	Render,
	Both
};


/** Painted weight map attributes node. */
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // For deprecated VertexWeights in copy constructor (Clang)
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetAddWeightMapNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetAddWeightMapNode, "AddWeightMap", "Cloth", "Cloth Add Weight Map")

public:

	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/**
	 * The collection used to transfer weight map from.
	 * Connecting a collection containing a weight map sharing the same name as the one
	 * created with this node will transfer the weights to the input collection vertices.
	 * Note this operation only happens once when the TransferCollection is first connected, or updated.
	 * Changing the Name or the TransferType will also redo the transfer operation.
	 */
	UPROPERTY(Meta = (DataflowInput))
	FManagedArrayCollection TransferCollection;

	/** The name to be set as a weight map attribute. */
	UPROPERTY(EditAnywhere, Category = "Add Weight Map", Meta = (DataflowOutput))
	FString Name;

	/**
	 * The type of transfer used to transfer the weight map when a TransferCollection is connected.
	 * This property is disabled when no TransferCollection input has been conencted.
	 */
	UPROPERTY(EditAnywhere, Category = "Add Weight Map", Meta = (EditCondition = "TransferCollectionHash != 0"))
	EChaosClothAssetWeightMapTransferType TransferType = EChaosClothAssetWeightMapTransferType::Use2DSimMesh;

	UE_DEPRECATED(5.4, "This property will be made private.")
	UPROPERTY()
	TArray<float> VertexWeights;

	UPROPERTY(EditAnywhere, Category = "Add Weight Map")
	EChaosClothAssetWeightMapMeshType MeshTarget = EChaosClothAssetWeightMapMeshType::Simulation;

	FChaosClothAssetAddWeightMapNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	friend class UClothEditorWeightMapPaintTool;

	const TArray<float>& GetVertexWeights() const { return VertexWeights; }
	TArray<float>& GetVertexWeights() { return VertexWeights; }

	const TArray<float>& GetRenderVertexWeights() const { return RenderVertexWeights; }
	TArray<float>& GetRenderVertexWeights() { return RenderVertexWeights; }

	UPROPERTY()
	TArray<float> RenderVertexWeights;

	//~ Begin FDataflowNode interface
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override;
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface

	UPROPERTY()
	uint32 TransferCollectionHash = 0;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
