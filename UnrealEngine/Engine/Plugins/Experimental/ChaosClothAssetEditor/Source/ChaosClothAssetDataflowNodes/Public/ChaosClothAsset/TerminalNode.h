// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowTerminalNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "ChaosClothAsset/ClothLodTransitionDataCache.h"
#include "TerminalNode.generated.h"

/** Cloth terminal node to generate a cloth asset from a cloth collection. */
USTRUCT(Meta = (DataflowCloth, DataflowTerminal))
struct FChaosClothAssetTerminalNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetTerminalNode, "ClothAssetTerminal", "Cloth", "Cloth Terminal")  // TODO: Should the category be Terminal instead like all other terminal nodes

public:
	static constexpr int32 MaxLods = 6;  // Hardcoded number of LODs since it is currently not possible to use arrays for optional inputs

	/** LOD 0 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DataflowInput, DisplayName = "Collection LOD 0"))
	FManagedArrayCollection CollectionLod0;
	/** LOD 1 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 1"))
	FManagedArrayCollection CollectionLod1;
	/** LOD 2 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 2"))
	FManagedArrayCollection CollectionLod2;
	/** LOD 3 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 3"))
	FManagedArrayCollection CollectionLod3;
	/** LOD 4 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 4"))
	FManagedArrayCollection CollectionLod4;
	/** LOD 5 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 5"))
	FManagedArrayCollection CollectionLod5;
	/** The number of LODs currently exposed to the node UI. */
	UPROPERTY()
	int32 NumLods = 1;

	FChaosClothAssetTerminalNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override;
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override {}
	virtual Dataflow::FPin AddPin() override;
	virtual bool CanAddPin() const override { return NumLods < MaxLods; }
	virtual bool CanRemovePin() const override { return NumLods > 1; }
	virtual Dataflow::FPin RemovePin() override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FDataflowNode interface

	TArray<const FManagedArrayCollection*> GetCollectionLods() const;

	UPROPERTY()
	mutable TArray<FChaosClothAssetLodTransitionDataCache> LODTransitionDataCache;
};
