// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "SimulationGravityConfigNode.generated.h"

/** Gravity properties configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationGravityConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationGravityConfigNode, "SimulationGravityConfig", "Cloth", "Cloth Simulation Gravity Config")

public:
	/** Use the config gravity value instead of world gravity. */
	UPROPERTY(EditAnywhere, Category = "Gravity Properties", Meta = (InlineEditConditionToggle))
	bool bUseGravityOverride = false;

	/** Scale factor applied to the world gravity and also to the clothing simulation interactor gravity. Does not affect the gravity if set using the override below. */
	UPROPERTY(EditAnywhere, Category = "Gravity Properties", Meta = (UIMin = "0", UIMax = "10", EditCondition = "!bUseGravityOverride"))
	float GravityScale = 1.f;

	/** The gravitational acceleration vector [cm/s^2]. */
	UPROPERTY(EditAnywhere, Category = "Gravity Properties", Meta = (UIMin = "0", UIMax = "10", EditCondition = "bUseGravityOverride"))
	FVector3f GravityOverride = { 0.f, 0.f, -980.665f };  // TODO: Should we make this a S.I. unit?

	FChaosClothAssetSimulationGravityConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const override;
};
