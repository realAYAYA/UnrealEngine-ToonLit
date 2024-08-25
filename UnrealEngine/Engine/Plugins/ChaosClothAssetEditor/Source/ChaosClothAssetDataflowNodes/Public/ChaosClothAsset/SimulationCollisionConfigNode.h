// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "SimulationCollisionConfigNode.generated.h"

/** Physics mesh collision properties configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationCollisionConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationCollisionConfigNode, "SimulationCollisionConfig", "Cloth", "Cloth Simulation Collision Config")

public:
	/** The added thickness of collision shapes. */
	UPROPERTY(EditAnywhere, Category = "Collision Properties", DisplayName = "Collision Thickness", Meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	FChaosClothAssetImportedFloatValue CollisionThicknessImported = {UE::Chaos::ClothAsset::FDefaultFabric::CollisionThickness};

	/** Friction coefficient for cloth - collider interaction. */
	UPROPERTY(EditAnywhere, Category = "Collision Properties", DisplayName = "Friction Coefficient", Meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "10"))
	FChaosClothAssetImportedFloatValue FrictionCoefficientImported = {UE::Chaos::ClothAsset::FDefaultFabric::Friction};

	/** Stiffness for proximity repulsion forces (Force-based solver only). Units = kg cm/ s^2 (same as XPBD springs)*/
	UPROPERTY(EditAnywhere, Category = "Proximity Force Properties", Meta = (UIMin = "0", UIMax = "10000", ClampMin = "0", ClampMax = "10000000"))
	float ProximityStiffness = 100.f;

	/**
	 * Use continuous collision detection (CCD) to prevent any missed collisions between fast moving particles and colliders.
	 * This has a negative effect on performance compared to when resolving collision without using CCD.
	 */
	UPROPERTY(EditAnywhere, Category = "Collision Properties")
	bool bUseCCD = false;

	FChaosClothAssetSimulationCollisionConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Serialize(FArchive& Ar) override;

private:
	virtual void AddProperties(FPropertyHelper& PropertyHelper) const override;

	// Deprecated properties
#if WITH_EDITORONLY_DATA

	UPROPERTY()
	float CollisionThickness_DEPRECATED = UE::Chaos::ClothAsset::FDefaultFabric::CollisionThickness;

	UPROPERTY()
	float FrictionCoefficient_DEPRECATED  = UE::Chaos::ClothAsset::FDefaultFabric::Friction;
#endif
};
