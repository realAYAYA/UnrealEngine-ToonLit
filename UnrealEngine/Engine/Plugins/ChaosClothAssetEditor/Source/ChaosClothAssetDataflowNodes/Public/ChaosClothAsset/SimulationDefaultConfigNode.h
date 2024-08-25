// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "SimulationDefaultConfigNode.generated.h"

class UChaosClothConfig;
class UChaosClothSharedSimConfig;

/** Add default simulation properties to the cloth collection in the format of the skeletal mesh cloth editor. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationDefaultConfigNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationDefaultConfigNode, "SimulationDefaultConfig", "Cloth", "Cloth Simulation Default Config")

public:
	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** Cloth Simulation Properties. */
	UPROPERTY(EditAnywhere, Instanced, NoClear, Category = "Simulation Default Config")
	TObjectPtr<UChaosClothConfig> SimulationConfig;

	/** Cloth Shared Simulation Properties. */
	UPROPERTY(EditAnywhere, Instanced, NoClear, Category = "Simulation Default Config")
	TObjectPtr<UChaosClothSharedSimConfig> SharedSimulationConfig;

	FChaosClothAssetSimulationDefaultConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode Interface
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FDataflowNode Interface

	UObject* OwningObject;  // For backward compatibility when null SimulationConfig and SharedSimulationConfig are reloaded
};
