// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "SetPhysicsAssetNode.generated.h"

class UPhysicsAsset;

/** Replace the current physics assets to collide the simulation mesh against. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSetPhysicsAssetNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSetPhysicsAssetNode, "SetPhysicsAsset", "Cloth", "Cloth Set Physics Asset")

public:
	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** The physics asset to assign to the Cloth Collection. */
	UPROPERTY(EditAnywhere, Category = "Set Physics Asset")
	TObjectPtr<UPhysicsAsset> PhysicsAsset;

	FChaosClothAssetSetPhysicsAssetNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
