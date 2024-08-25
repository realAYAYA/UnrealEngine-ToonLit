// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "ChaosClothAsset/ConnectableValue.h"
#include "SimulationSelfCollisionConfigNode.generated.h"

/** Self-collision repulsion forces (point-face) properties configuration node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationSelfCollisionConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationSelfCollisionConfigNode, "SimulationSelfCollisionConfig", "Cloth", "Cloth Simulation Self Collision Config")

public:
	/** The self collision offset per side. Total thickness of cloth is 2x this value. */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties", meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	FChaosClothAssetWeightedValue SelfCollisionThicknessWeighted = {true, UE::Chaos::ClothAsset::FDefaultFabric::SelfCollisionThickness,
		UE::Chaos::ClothAsset::FDefaultFabric::SelfCollisionThickness, TEXT("SelfCollisionThickness"), true};

	/** The stiffness of the springs used to control self collision (PBD Solver). */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "!bSelfCollideAgainstKinematicCollidersOnly"))
	float SelfCollisionStiffness = 0.5f;

	/** Friction coefficient for cloth - cloth interaction. */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties", DisplayName = "Self Collision Friction", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "!bSelfCollideAgainstKinematicCollidersOnly"))
	FChaosClothAssetImportedFloatValue SelfCollisionFrictionImported = {UE::Chaos::ClothAsset::FDefaultFabric::SelfFriction};

	/** Disabled neighbor collision ring. Collisions are disabled between vertices within this N-ring connectivity distance.*/
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties", meta = (UIMin = "1", UIMax = "5", ClampMin = "1", EditCondition = "!bSelfCollideAgainstKinematicCollidersOnly"))
	int32 SelfCollisionDisableNeighborDistance = 5;

	/** Self collision layers face int map. Generate this map using the SelectionsToIntMap node with SimFace Selections.
	* Faces labeled with -1 will collide normally without any layering behavior.
	* Faces labeled with any other number will keep higher layer numbers outside lower layer numbers (outside = front facing normal direction).
	*/
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties", meta = (EditCondition = "!bSelfCollideAgainstKinematicCollidersOnly"))
	FChaosClothAssetConnectableIStringValue SelfCollisionLayers = { TEXT("SelfCollisionLayers"), true };

	/** Sim face selection set of faces which should not self collide */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties")
	FChaosClothAssetConnectableIStringValue SelfCollisionDisabledFaces = { TEXT("SelfCollisionDisabledFaces") };
	
	/** Collide only against kinematic colliders (no dynamic self collisions). Kinematic colliders do not do Self Intersections. They always collide against the front-face.*/
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties - Kinematic Colliders")
	bool bSelfCollideAgainstKinematicCollidersOnly = false;

	/** Sim face selection set of kinematic faces which should self collide. Kinematic colliders do not do Self Intersections. They always collide against the front-face. */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties - Kinematic Colliders", meta = (EditCondition = "!bSelfCollideAgainstAllKinematicVertices"))
	FChaosClothAssetConnectableIStringValue SelfCollisionEnabledKinematicFaces = { TEXT("SelfCollisionEnabledKinematicFaces") };

	/** Thickness of kinematic colliders. Total offset between cloth and kinematic colliders is SelfCollisionThickness + SelfCollisionKinematicColliderThickness. */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties - Kinematic Colliders", meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	float SelfCollisionKinematicColliderThickness = 0.f;

	/** The stiffness of the springs used to control self collision (PBD Solver). */
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties - Kinematic Colliders", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float SelfCollisionKinematicColliderStiffness = 1.f;

	/** Friction coefficient for cloth - kinematic cloth interaction. Weight map is on the dynamic cloth, not the collider.*/
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties - Kinematic Colliders", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	FChaosClothAssetWeightedValue SelfCollisionKinematicColliderFrictionWeighted = {true, 0.0f, 0.f, TEXT("SelfCollisionKinematicColliderFriction")};

	/** Self collide against all kinematic vertices. Kinematic colliders do not do Self Intersections. They always collide against the front-face.*/
	UPROPERTY(EditAnywhere, Category = "Self-Collision Properties - Kinematic Colliders")
	bool bSelfCollideAgainstAllKinematicVertices = false;

	/** Enable self intersection resolution. This will try to fix any cloth intersections that are not handled by collision repulsions. */
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (EditCondition = "!bSelfCollideAgainstKinematicCollidersOnly"))
	bool bUseSelfIntersections = false;

	/** Do global intersection analysis to determine the correct normals for the collision springs */
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (EditCondition = "bUseSelfIntersections && !bSelfCollideAgainstKinematicCollidersOnly"))
	bool bUseGlobalIntersectionAnalysis = true;

	/** Do a step of contour minimization at the beginning of the timestep. */
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (EditCondition = "bUseSelfIntersections && !bSelfCollideAgainstKinematicCollidersOnly"))
	bool bUseContourMinimization = true;

	/** Number of post timestep contour minimization steps to do. (Expensive!)*/
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (ClampMin = "0", EditCondition = "bUseSelfIntersections && !bSelfCollideAgainstKinematicCollidersOnly"))
	int32 NumContourMinimizationPostSteps = 0;

	/** Use global contour gradients when doing post timestep contour minimization */
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (EditCondition = "bUseSelfIntersections && NumContourMinimizationPostSteps > 0 && !bSelfCollideAgainstKinematicCollidersOnly"))
	bool bUseGlobalPostStepContours = true;

	/** The stiffness of the proximity repulsions used to control self collision (Force-based Solver). Units = kg cm/ s^2 (same as XPBD springs) */
	UPROPERTY(EditAnywhere, Category = "Experimental", meta = (UIMin = "0", UIMax = "10000", ClampMin = "0", ClampMax = "10000000", EditCondition = "!bSelfCollideAgainstAllKinematicVertices"))
	float SelfCollisionProximityStiffness = 1.f;

	FChaosClothAssetSimulationSelfCollisionConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Serialize(FArchive& Ar) override;
private:
	// Deprecated properties
#if WITH_EDITORONLY_DATA
	static constexpr float FrictionDeprecatedValue = -1.f;
	UPROPERTY()
	float SelfCollisionKinematicColliderFriction_DEPRECATED = FrictionDeprecatedValue;

	UPROPERTY()
	float SelfCollisionFriction_DEPRECATED = UE::Chaos::ClothAsset::FDefaultFabric::SelfFriction;

	static constexpr float SelfCollisionThicknessDeprecatedValue = -1.f;
	UPROPERTY()
	float SelfCollisionThickness_DEPRECATED = SelfCollisionThicknessDeprecatedValue;
#endif
	virtual void AddProperties(FPropertyHelper& PropertyHelper) const override;
};
