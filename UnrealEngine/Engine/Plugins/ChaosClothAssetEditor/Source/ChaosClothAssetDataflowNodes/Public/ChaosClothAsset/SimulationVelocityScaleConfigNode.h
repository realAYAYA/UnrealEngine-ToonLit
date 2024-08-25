// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "SimulationVelocityScaleConfigNode.generated.h"

/** Velocity scale properties configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationVelocityScaleConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationVelocityScaleConfigNode, "SimulationVelocityScaleConfig", "Cloth", "Cloth Simulation Velocity Scale Config")

public:

	/**
	 * The amount of linear velocities sent to the local cloth space from the reference bone
	 * (the closest bone to the root on which the cloth section has been skinned, or the root itself if the cloth isn't skinned).
	 * This value will be clamped by "Max Velocity Scale". A velocity scale of > 1 will amplify the velocities from the reference bone.
	 */
	UPROPERTY(EditAnywhere, Category = "Velocity Scale Properties", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "100"))
	FVector3f LinearVelocityScale = { 0.75f, 0.75f, 0.75f };

	/**
	 * The amount of angular velocities sent to the local cloth space from the reference bone
	 * (the closest bone to the root on which the cloth section has been skinned, or the root itself if the cloth isn't skinned).
	 * This value will be clamped by "Max Velocity Scale". A velocity scale of > 1 will amplify the velocities from the reference bone.
	 */
	UPROPERTY(EditAnywhere, Category = "Velocity Scale Properties", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "100"))
	float AngularVelocityScale = 0.75f;

	/**
	 * Clamp on Linear and Angular Velocity Scale. The final velocity scale (e.g., including contributions from blueprints) will be clamped to this value.
	 */
	UPROPERTY(EditAnywhere, Category = "Velocity Scale Properties", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "100"))
	float MaxVelocityScale = 1.f;

	/**
	 * The portion of the angular velocity that is used to calculate the strength of all fictitious forces (e.g. centrifugal force).
	 * This parameter is only having an effect on the portion of the reference bone's angular velocity that has been removed from the
	 * simulation via the Angular Velocity Scale parameter. This means it has no effect when AngularVelocityScale is set to 1 in which
	 * case the cloth is simulated with full world space angular velocities and subjected to the true physical world inertial forces.
	 * Values range from 0 to 2, with 0 showing no centrifugal effect, 1 full centrifugal effect, and 2 an overdriven centrifugal effect.
	 */
	UPROPERTY(EditAnywhere, Category = "Velocity Scale Properties", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "2"))
	float FictitiousAngularScale = 1.f;

	FChaosClothAssetSimulationVelocityScaleConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void AddProperties(FPropertyHelper& PropertyHelper) const override;
};
