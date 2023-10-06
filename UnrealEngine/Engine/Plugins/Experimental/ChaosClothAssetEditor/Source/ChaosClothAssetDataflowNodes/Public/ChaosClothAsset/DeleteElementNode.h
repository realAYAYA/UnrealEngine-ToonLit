// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "DeleteElementNode.generated.h"

UENUM()
enum class EChaosClothAssetElementType : uint8
{
	None,
	SimMesh,
	RenderMesh,
	SimPattern,
	RenderPattern,
	SimVertex2D,
	SimVertex3D,
	RenderVertex,
	SimFace,
	RenderFace,
	Seam
};

USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetDeleteElementNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetDeleteElementNode, "DeleteElement", "Cloth", "Cloth Simulation Delete Element")

public:

	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** Element type to delete.*/
	UPROPERTY(EditAnywhere, Category = "Delete Element")
	EChaosClothAssetElementType ElementType = EChaosClothAssetElementType::None;

	/** List of Elements to apply the operation on. All Elements will be used if left empty. */
	UPROPERTY(EditAnywhere, Category = "Delete Element", Meta = (EditCondition = "ElementType != EChaosClothAssetElementType::SimMesh && ElementType != EChaosClothAssetElementType::RenderMesh"))
	TArray<int32> Elements;

	FChaosClothAssetDeleteElementNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
