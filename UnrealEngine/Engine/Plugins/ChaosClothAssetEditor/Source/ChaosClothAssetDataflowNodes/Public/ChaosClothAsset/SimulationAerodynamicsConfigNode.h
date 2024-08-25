// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "SimulationAerodynamicsConfigNode.generated.h"

/** Aerodynamics properties configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationAerodynamicsConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationAerodynamicsConfigNode, "SimulationAerodynamicsConfig", "Cloth", "Cloth Simulation Aerodynamics Config")

public:
	/**
	 * The density of the medium in which the aerodynamic forces take place, usually air.
	 * The fluid density is given in kg/m^3.
	 * Air density is considered to be around 1.225 kg/m^3 in average atmospheric conditions.
	 */
	UPROPERTY(EditAnywhere, Category = "Aerodynamics", Meta = (UIMin = "0", ClampMin = "0", ClampMax = "10"))
	float FluidDensity = 1.225f;
	
	/**
	 * The fixed wind velocity [m/s] for this asset.
	 * For reference a wind gust is above 8m/s (18mph).
	 */
	UPROPERTY(EditAnywhere, Category = "Aerodynamics", Meta = (UIMin = "0", UIMax = "10"))
	FVector3f WindVelocity = { 0.f, 0.f, 0.f };

	/**
	 * The aerodynamic coefficient of drag applying on each particle.
	 * If a valid weight map is found with the given Weight Map name, then both Low and High values
	 * are interpolated with the per particle weight to make the final value used for the simulation.
	 * Otherwise all particles are considered to have a zero weight, and only the Low value is meaningful.
	 */
	UPROPERTY(EditAnywhere, Category = "Aerodynamics", Meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "10"))
	FChaosClothAssetWeightedValue Drag = { true, 0.035f, 1.f, TEXT("Drag"), true };

	/**
	 * The aerodynamic coefficient of lift applying on each particle.
	 * If a valid weight map is found with the given Weight Map name, then both Low and High values
	 * are interpolated with the per particle weight to make the final value used for the simulation.
	 * Otherwise all particles are considered to have a zero weight, and only the Low value is meaningful.
	 */
	UPROPERTY(EditAnywhere, Category = "Aerodynamics", Meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "10"))
	FChaosClothAssetWeightedValue Lift = { true, 0.035f, 1.f, TEXT("Lift"), true };
	
	FChaosClothAssetSimulationAerodynamicsConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void AddProperties(FPropertyHelper& PropertyHelper) const override;
};
