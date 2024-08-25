// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "SimulationDampingConfigNode.generated.h"

/** Damping properties configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationDampingConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationDampingConfigNode, "SimulationDampingConfig", "Cloth", "Cloth Simulation Damping Config")

public:
	/**
	 * The amount of global damping applied to the cloth velocities, also known as point damping.
	 * Point damping improves simulation stability, but can also cause an overall slow-down effect and therefore is best left to very small percentage amounts.
	 */
	UPROPERTY(EditAnywhere, Category = "Damping Properties", Meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	FChaosClothAssetWeightedValue DampingCoefficientWeighted = {true, 0.01f, 0.01f, TEXT("DampingCoefficient")};

	/**
	 * The amount of local damping applied to the cloth velocities.
	 * This type of damping only damps individual deviations of the particles velocities from the global motion.
	 * It makes the cloth deformations more cohesive and reduces jitter without affecting the overall movement.
	 * It can also produce synchronization artifacts where part of the cloth mesh are disconnected (which might well be desirable, or not), and is more expensive than global damping.
	 */
	UPROPERTY(EditAnywhere, Category = "Damping Properties", Meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float LocalDampingCoefficient = 0.f;

	FChaosClothAssetSimulationDampingConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Serialize(FArchive& Ar) override;
private:
	virtual void AddProperties(FPropertyHelper& PropertyHelper) const override;

	// Deprecated properties
#if WITH_EDITORONLY_DATA
	static constexpr float DeprecatedDampingCoefficientValue = -1.f; // This is outside the settable range when it wasn't deprecated.
	UPROPERTY()
	float DampingCoefficient_DEPRECATED = DeprecatedDampingCoefficientValue;
#endif 

};
