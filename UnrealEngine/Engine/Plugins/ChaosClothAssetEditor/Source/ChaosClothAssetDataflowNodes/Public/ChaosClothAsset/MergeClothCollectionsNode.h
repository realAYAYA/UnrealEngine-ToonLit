// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "MergeClothCollectionsNode.generated.h"

/** Merge multiple cloth collections into a single cloth collection of multiple patterns. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetMergeClothCollectionsNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetMergeClothCollectionsNode, "MergeClothCollections", "Cloth", "Cloth Merge Collection")

public:
	static constexpr int32 MaxInputs = 6;  // Hardcoded number of inputs since it is currently not possible to use arrays for optional inputs

	/** Input 0, right click on the node and add pins to add more merge inputs. */
	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;
	/** Input 1, right click on the node and add pins to add more merge inputs. */
	UPROPERTY(Meta = (DisplayName = "Collection 1"))
	FManagedArrayCollection Collection1;
	/** Input 2, right click on the node and add pins to add more merge inputs. */
	UPROPERTY(Meta = (DisplayName = "Collection 2"))
	FManagedArrayCollection Collection2;
	/** Input 3, right click on the node and add pins to add more merge inputs. */
	UPROPERTY(Meta = (DisplayName = "Collection 3"))
	FManagedArrayCollection Collection3;
	/** Input 4, right click on the node and add pins to add more merge inputs. */
	UPROPERTY(Meta = (DisplayName = "Collection 4"))
	FManagedArrayCollection Collection4;
	/** Input 5, right click on the node and add pins to add more merge inputs. */
	UPROPERTY(Meta = (DisplayName = "Collection 5"))
	FManagedArrayCollection Collection5;
	/** The number of inputs currently exposed to the node UI. */
	UPROPERTY()
	int32 NumInputs = 1;

	FChaosClothAssetMergeClothCollectionsNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual Dataflow::FPin AddPin() override;
	virtual bool CanAddPin() const override { return NumInputs < MaxInputs; }
	virtual bool CanRemovePin() const override { return NumInputs > 1; }
	virtual Dataflow::FPin GetPinToRemove() const override;
	virtual void OnPinRemoved(const Dataflow::FPin& Pin) override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FDataflowNode interface

	TArray<const FManagedArrayCollection*> GetCollections() const;
};
