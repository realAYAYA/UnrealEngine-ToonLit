// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "SimulationLongRangeAttachmentConfigNode.generated.h"

/** Long range attachment constraint property configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationLongRangeAttachmentConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationLongRangeAttachmentConfigNode, "SimulationLongRangeAttachmentConfig", "Cloth", "Cloth Simulation Long Range Attachment Config")

public:

	/**
	 * The tethers' stiffness of the long range attachment constraints.
	 * The long range attachment connects each of the cloth particles to its closest fixed point with a spring constraint.
	 * This can be used to compensate for a lack of stretch resistance when the iterations count is kept low for performance reasons.
	 * Can lead to an unnatural pull string puppet like behavior.
	 * If a valid weight map is found with the given Weight Map name, then both Low and High values
	 * are interpolated with the per particle weight to make the final value used for the simulation.
	 * Otherwise all particles are considered to have a zero weight, and only the Low value is meaningful.
	 */
	UPROPERTY(EditAnywhere, Category = "Long Range Attachment Properties", Meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	FChaosClothAssetWeightedValue TetherStiffness = { true, 1.f, 1.f, TEXT("TetherStiffness") };

	/**
	 * The limit scale of the long range attachment constraints (aka tether limit).
	 * If a valid weight map is found with the given Weight Map name, then both Low and High values
	 * are interpolated with the per particle weight to make the final value used for the simulation.
	 * Otherwise all particles are considered to have a zero weight, and only the Low value is meaningful.
	*/
	UPROPERTY(EditAnywhere, Category = "Long Range Attachment Properties", Meta = (UIMin = "1.", UIMax = "1.1", ClampMin = "0.01", ClampMax = "10"))
	FChaosClothAssetWeightedValue TetherScale = { true, 1.f, 1.f, TEXT("TetherScale") };

	/**
	 * Use geodesic instead of euclidean distance calculations for the Long Range Attachment constraint,
	 * which is slower at setup but more accurate at establishing the correct position and length of the tethers,
	 * and therefore is less prone to artifacts during the simulation.
	 */
	UPROPERTY(EditAnywhere, Category = "Long Range Attachment Properties")
	bool bUseGeodesicTethers = true;

	/** The name of the weight map used to calculate fixed tether ends. All vertices with weight = 0 will be considered fixed. */
	UPROPERTY(EditAnywhere, Category = "Long Range Attachment Properties", Meta = (DataflowInput))
	FString FixedEndWeightMap = TEXT("MaxDistance");

	FChaosClothAssetSimulationLongRangeAttachmentConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void AddProperties(FPropertyHelper& PropertyHelper) const override;

	virtual void EvaluateClothCollection(Dataflow::FContext& Context, const TSharedRef<FManagedArrayCollection>& ClothCollection) const override;
};
