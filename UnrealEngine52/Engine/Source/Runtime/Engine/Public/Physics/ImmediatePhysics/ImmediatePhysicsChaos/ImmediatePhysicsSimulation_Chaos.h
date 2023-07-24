// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsCore_Chaos.h"

#include "Engine/EngineTypes.h"
#include "Templates/UniquePtr.h"

namespace ImmediatePhysics_Chaos
{
	/** Owns all the data associated with the simulation. Can be considered a single scene or world */
	struct ENGINE_API FSimulation
	{
	public:
		FSimulation();
		~FSimulation();

		int32 NumActors() const;

		FActorHandle* GetActorHandle(int32 ActorHandleIndex);
		const FActorHandle* GetActorHandle(int32 ActorHandleIndex) const;

		/** Create a static body and add it to the simulation */
		FActorHandle* CreateStaticActor(FBodyInstance* BodyInstance);

		/** Create a kinematic body and add it to the simulation */
		FActorHandle* CreateKinematicActor(FBodyInstance* BodyInstance, const FTransform& Transform);

		/** Create a dynamic body and add it to the simulation */
		FActorHandle* CreateDynamicActor(FBodyInstance* BodyInstance, const FTransform& Transform);

		FActorHandle* CreateActor(EActorType ActorType, FBodyInstance* BodyInstance, const FTransform& Transform);
		void DestroyActor(FActorHandle* ActorHandle);

		void DestroyActorCollisions(FActorHandle* ActorHandle);

		/** Create a physical joint and add it to the simulation */
		FJointHandle* CreateJoint(FConstraintInstance* ConstraintInstance, FActorHandle* Body1, FActorHandle* Body2);
		void DestroyJoint(FJointHandle* JointHandle);

		/** Sets the number of active bodies. This number is reset any time a new simulated body is created */
		void SetNumActiveBodies(int32 NumActiveBodies, TArray<int32> ActiveBodyIndices);

		/** An array of actors to ignore. */
		struct FIgnorePair
		{
			FActorHandle* A;
			FActorHandle* B;
		};

		/** Set pair of bodies to ignore collision for */
		void SetIgnoreCollisionPairTable(const TArray<FIgnorePair>& InIgnoreCollisionPairTable);

		/** Set bodies that require no collision */
		void SetIgnoreCollisionActors(const TArray<FActorHandle*>& InIgnoreCollisionActors);

		/** Set up potential collisions between the actor and all other dynamic actors */
		void AddToCollidingPairs(FActorHandle* ActorHandle);

		/** Advance the simulation by DeltaTime */
		void Simulate(FReal DeltaTime, FReal MaxStepTime, int32 MaxSubSteps, const FVector& InGravity);
		void Simulate_AssumesLocked(FReal DeltaTime, FReal MaxStepTime, int32 MaxSubSteps, const FVector& InGravity) { Simulate(DeltaTime, MaxStepTime, MaxSubSteps, InGravity); }

		void InitSimulationSpace(
			const FTransform& Transform);

		void UpdateSimulationSpace(
			const FTransform& Transform,
			const FVector& LinearVel,
			const FVector& AngularVel,
			const FVector& LinearAcc,
			const FVector& AngularAcc);

		void SetSimulationSpaceSettings(
			const FReal Alpha, 
			const FVector& ExternalLinearEtherDrag);

		/** Set settings. Invalid (negative) values with leave that value unchanged from defaults */
		void SetSolverSettings(
			const FReal FixedDt,
			const FReal CullDistance,
			const FReal MaxDepenetrationVelocity,
			const int32 UseLinearJointSolver,
			const int32 PositionIts,
			const int32 VelocityIts,
			const int32 ProjectionIts);

		/** Explicit debug draw path if the use case needs it to happen at a point outside of the simulation **/
		void DebugDraw();

	private:
		void RemoveFromCollidingPairs(FActorHandle* ActorHandle);
		void UpdateInertiaConditioning(const FVector& Gravity);
		void PackCollidingPairs();
		void UpdateActivePotentiallyCollidingPairs();
		void EnableDisableJoints();
		FReal UpdateStepTime(const FReal DeltaTime, const FReal MaxStepTime);

		void UpdateStatCounters();
		void DebugDrawStaticParticles();
		void DebugDrawKinematicParticles();
		void DebugDrawDynamicParticles();
		void DebugDrawConstraints();
		void DebugDrawSimulationSpace();

		struct FImplementation;
		TUniquePtr<FImplementation> Implementation;
	};

}
