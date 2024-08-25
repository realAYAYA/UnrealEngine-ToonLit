// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Deformable/ChaosDeformableSolver.h"
#include "Chaos/Deformable/ChaosDeformableSolverProxy.h"
#include "Chaos/Deformable/ChaosDeformableSolverTypes.h"
#include "ChaosFlesh/ChaosDeformableSolverThreading.h"
#include "ChaosFlesh/ChaosDeformableTypes.h"
#include "ChaosFlesh/FleshComponent.h"
#include "DeformableInterface.h"
#include "Components/SceneComponent.h"
#include "UObject/ObjectMacros.h"

#include "ChaosDeformableSolverComponent.generated.h"

class UDeformablePhysicsComponent;
class UDeformableCollisionsComponent;

USTRUCT(BlueprintType)
struct FConnectedObjectsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ConnectedObjects", meta = (EditCondition = "false"))
	TArray< TObjectPtr<UDeformablePhysicsComponent> > DeformableComponents;
};

USTRUCT(BlueprintType)
struct FSolverTimingGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		int32 NumSubSteps = 2;

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		int32 NumSolverIterations = 5;

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		bool FixTimeStep = false;

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		float TimeStepSize = 0.05;

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		bool bDoThreadedAdvance = true;

	/** ObjectType defines how to initialize the rigid objects state, Kinematic, Sleeping, Dynamic. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SolverTiming")
		EDeformableExecutionModel ExecutionModel = EDeformableExecutionModel::Chaos_Deformable_PostPhysics;
};


USTRUCT(BlueprintType)
struct FSolverDebuggingGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Physics")
	bool CacheToFile = false;
};


USTRUCT(BlueprintType)
struct FSolverQuasistaticsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Quasistatics")
	bool bDoQuasistatics = false;
};


USTRUCT(BlueprintType)
struct FSolverEvolutionGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Evolution")
	FSolverQuasistaticsGroup SolverQuasistatics;

};


USTRUCT(BlueprintType)
struct FSolverGridBasedCollisionsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "GridBasedCollisions")
	bool bUseGridBasedConstraints = false;

	UPROPERTY(EditAnywhere, Category = "GridBasedCollisions")
	float GridDx = 25.;
};

USTRUCT(BlueprintType)
struct FCollisionSpringGroup
{
	GENERATED_USTRUCT_BODY()
	/**
	* Search radius for point triangle pairs
	*/
	UPROPERTY(EditAnywhere, Category = "CollisionSpring")
	float CollisionSearchRadius = 10.f;
	/**
	* Collision spring stiffness; larger value will stop penetration better
	*/
	UPROPERTY(EditAnywhere, Category = "CollisionSpring")
	float CollisionSpringStiffness = 500.f;
	/**
	* Anisotropic springs will allow sliding on the triangle
	*/
	UPROPERTY(EditAnywhere, Category = "CollisionSpring")
	bool bAllowSliding = true;
};

USTRUCT(BlueprintType)
struct FSolverGaussSeidelConstraintsGroup
{
	GENERATED_USTRUCT_BODY()

	/**
	* Enable the Gauss Seidel solver instead of the existing XPBD.
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	bool bUseGaussSeidelConstraints = false;

	/**
	* Enable another model that runs simulation faster.
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	bool bUseGSNeohookean = false;

	/**
	* Enable acceleration technique for Gauss Seidel solver to make simulation look better within a limited budget.
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	bool bUseSOR = true;

	/**
	* Acceleration related parameter. Tune it down if simulation becomes unstable. 
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	float OmegaSOR = 1.6f;

	/**
	* Collsion detection radius and stiffness
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GaussSeidelConstraints")
	FCollisionSpringGroup CollisionSpring;
};

USTRUCT(BlueprintType)
struct FSolverCollisionsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Collisions")
	bool bDoSelfCollision = false;

	UPROPERTY(EditAnywhere, Category = "Physics")
	bool bUseFloor = true;

	//UPROPERTY(EditAnywhere, Category = "Collisions")
	//FSolverGridBasedCollisionsGroup SolverGridBasedCollisions;
};

USTRUCT(BlueprintType)
struct FSolverCorotatedConstraintsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Corotated")
	bool bEnableCorotatedConstraint = true;

	UPROPERTY(EditAnywhere, Category = "Corotated")
	bool bDoBlended = false;

	UPROPERTY(EditAnywhere, Category = "Corotated")
	float BlendedZeta = 0;
};

USTRUCT(BlueprintType)
struct FSolverConstraintsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Constraints")
	bool bEnablePositionTargets = true;

	UPROPERTY(EditAnywhere, Category = "Constraints")
	bool bEnableKinematics = true;

	UPROPERTY(EditAnywhere, Category = "Constraints")
	FSolverCorotatedConstraintsGroup CorotatedConstraints;

	/**
	* These are options for another solver. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Constraints")
	FSolverGaussSeidelConstraintsGroup GaussSeidelConstraints;
};


USTRUCT(BlueprintType)
struct FSolverForcesGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Forces")
	float YoungModulus = 100000;

	UPROPERTY(EditAnywhere, Category = "Forces")
	float Damping = 0;

	UPROPERTY(EditAnywhere, Category = "Forces")
	bool bEnableGravity = true;
};

/**
*	UDeformableSolverComponent
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class CHAOSFLESHENGINE_API UDeformableSolverComponent : public USceneComponent, public IDeformableInterface
{
	GENERATED_UCLASS_BODY()

public:
	typedef Chaos::Softs::FThreadingProxy FThreadingProxy;
	typedef Chaos::Softs::FFleshThreadingProxy FFleshThreadingProxy;
	typedef Chaos::Softs::FDeformablePackage FDeformablePackage;
	typedef Chaos::Softs::FDataMapValue FDataMapValue;
	typedef Chaos::Softs::FDeformableSolver FDeformableSolver;

	~UDeformableSolverComponent();
	void UpdateTickGroup();

	/* Solver API */
	FDeformableSolver::FGameThreadAccess GameThreadAccess();

	bool IsSimulating(UDeformablePhysicsComponent*) const;
	bool IsSimulatable() const;
	void Reset();
	void AddDeformableProxy(UDeformablePhysicsComponent* InComponent);
	void Simulate(float DeltaTime);
	void UpdateFromGameThread(float DeltaTime);
	void UpdateFromSimulation(float DeltaTime);

	/* Component Thread Management */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//bool ShouldWaitForDeformableInTickFunction() const;
	void UpdateDeformableEndTickState(bool bRegister);

	/* Properties : Do NOT place ungrouped properties in this class */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (EditCondition = "false"))
	FConnectedObjectsGroup ConnectedObjects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverTimingGroup SolverTiming;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverEvolutionGroup SolverEvolution;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverCollisionsGroup SolverCollisions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverConstraintsGroup SolverConstraints;

	

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverForcesGroup SolverForces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverDebuggingGroup SolverDebugging;

	// Simulation Variables
	TUniquePtr<FDeformableSolver> Solver;

#if WITH_EDITOR
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
#endif

protected:

	/** Ref for the deformable solvers parallel task, so we can detect whether or not a sim is running */
	FGraphEventRef ParallelDeformableTask;
	FDeformableEndTickFunction DeformableEndTickFunction;
};

