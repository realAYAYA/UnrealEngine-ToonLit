// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "SimulationBaseConfigNode.generated.h"

namespace Chaos::Softs
{
	class FCollectionPropertyMutableFacade;
}

/**
 * Base abstract class for all cloth asset config nodes.
 * Inherited class must call RegisterCollectionConnections() in constructor to use this base class Collection.
 */
USTRUCT(meta = (Abstract))
struct FChaosClothAssetSimulationBaseConfigNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	FChaosClothAssetSimulationBaseConfigNode() = default;

	FChaosClothAssetSimulationBaseConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

protected:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

	virtual void AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
	PURE_VIRTUAL(FChaosClothAssetSimulationBaseConfigNode::AddProperties, );

	/* Override this to do additional node-specific evaluate on the cloth collection output. AddProperties has already been called when this is called. */
	virtual void EvaluateClothCollection(Dataflow::FContext& Context, const TSharedRef<FManagedArrayCollection>& ClothCollection) const {}

	void RegisterCollectionConnections();

	int32 AddPropertyHelper(
		::Chaos::Softs::FCollectionPropertyMutableFacade& Properties,
		const FName& PropertyName,
		bool bIsAnimatable = true,
		const TArray<FName>& SimilarPropertyNames = TArray<FName>()) const;
};

template<>
struct TStructOpsTypeTraits<FChaosClothAssetSimulationBaseConfigNode> : public TStructOpsTypeTraitsBase2<FChaosClothAssetSimulationBaseConfigNode>
{
	enum
	{
		WithPureVirtual = true,
	};
};
