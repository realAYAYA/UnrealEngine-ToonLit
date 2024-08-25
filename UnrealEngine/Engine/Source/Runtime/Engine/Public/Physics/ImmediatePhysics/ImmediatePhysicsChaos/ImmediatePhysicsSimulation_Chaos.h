// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVisualDebugger/ChaosVDContextProvider.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsCore_Chaos.h"

#include "Engine/EngineTypes.h"
#include "Templates/UniquePtr.h"

namespace Chaos
{
	class FPBDJointSettings;
}

namespace ImmediatePhysics_Chaos
{
	/** Owns all the data associated with the simulation. Can be considered a single scene or world */
	struct FSimulation
	{
	public:
		ENGINE_API FSimulation();
		ENGINE_API ~FSimulation();

		ENGINE_API int32 NumActors() const;

		ENGINE_API FActorHandle* GetActorHandle(int32 ActorHandleIndex);
		ENGINE_API const FActorHandle* GetActorHandle(int32 ActorHandleIndex) const;

		/** Create a static body and add it to the simulation */
		ENGINE_API FActorHandle* CreateStaticActor(FBodyInstance* BodyInstance);

		/** Create a kinematic body and add it to the simulation */
		ENGINE_API FActorHandle* CreateKinematicActor(FBodyInstance* BodyInstance, const FTransform& Transform);

		/** Create a dynamic body and add it to the simulation */
		ENGINE_API FActorHandle* CreateDynamicActor(FBodyInstance* BodyInstance, const FTransform& Transform);

		ENGINE_API FActorHandle* CreateActor(EActorType ActorType, FBodyInstance* BodyInstance, const FTransform& Transform);
		ENGINE_API void DestroyActor(FActorHandle* ActorHandle);

		ENGINE_API void DestroyActorCollisions(FActorHandle* ActorHandle);

		ENGINE_API void SetIsKinematic(FActorHandle* ActorHandle, bool bKinematic);

		ENGINE_API void SetEnabled(FActorHandle* ActorHandle, bool bEnable);

		ENGINE_API void SetHasCollision(FActorHandle* ActorHandle, bool bHasCollision);

		/** Create a physical joint and add it to the simulation */
		ENGINE_API FJointHandle* CreateJoint(FConstraintInstance* ConstraintInstance, FActorHandle* Body1, FActorHandle* Body2);

		/** Create a physical joint and add it to the simulation */
		ENGINE_API FJointHandle* CreateJoint(const Chaos::FPBDJointSettings& ConstraintSettings, FActorHandle* const Body1, FActorHandle* const Body2);

		ENGINE_API void DestroyJoint(FJointHandle* JointHandle);

		/** Sets the number of active bodies. This number is reset any time a new simulated body is created */
		ENGINE_API void SetNumActiveBodies(int32 NumActiveBodies, TArray<int32> ActiveBodyIndices);

		/** An array of actors to ignore. */
		struct FIgnorePair
		{
			FActorHandle* A;
			FActorHandle* B;
		};

		/** Set pair of bodies to ignore collision for */
		ENGINE_API void SetIgnoreCollisionPairTable(const TArray<FIgnorePair>& InIgnoreCollisionPairTable);

		/** Set bodies that require no collision */
		ENGINE_API void SetIgnoreCollisionActors(const TArray<FActorHandle*>& InIgnoreCollisionActors);

		/** Set up potential collisions between the actor and all other dynamic actors */
		ENGINE_API void AddToCollidingPairs(FActorHandle* ActorHandle);

		/** 
		 * Sets whether velocities should be rewound when simulating - this may happen when the requested 
		 * step size is smaller than the fixed simulation step.
		 */
		ENGINE_API void SetRewindVelocities(bool bRewindVelocities);

		/** Advance the simulation by DeltaTime */
		ENGINE_API void Simulate(FReal DeltaTime, FReal MaxStepTime, int32 MaxSubSteps, const FVector& InGravity);
		void Simulate_AssumesLocked(FReal DeltaTime, FReal MaxStepTime, int32 MaxSubSteps, const FVector& InGravity) { Simulate(DeltaTime, MaxStepTime, MaxSubSteps, InGravity); }

		ENGINE_API void InitSimulationSpace(
			const FTransform& Transform);

		ENGINE_API void UpdateSimulationSpace(
			const FTransform& Transform,
			const FVector& LinearVel,
			const FVector& AngularVel,
			const FVector& LinearAcc,
			const FVector& AngularAcc);

		ENGINE_API void SetSimulationSpaceSettings(
			const FReal Alpha, 
			const FVector& ExternalLinearEtherDrag);

		/** Set settings. Invalid (negative) values with leave that value unchanged from defaults */
		ENGINE_API void SetSolverSettings(
			const FReal FixedDt,
			const FReal CullDistance,
			const FReal MaxDepenetrationVelocity,
			const int32 UseLinearJointSolver,
			const int32 PositionIts,
			const int32 VelocityIts,
			const int32 ProjectionIts);

		/** Explicit debug draw path if the use case needs it to happen at a point outside of the simulation **/
		ENGINE_API void DebugDraw();

	private:
		ENGINE_API void RemoveFromCollidingPairs(FActorHandle* ActorHandle);
		ENGINE_API void UpdateInertiaConditioning(const FVector& Gravity);
		ENGINE_API void PackCollidingPairs();
		ENGINE_API void UpdateActivePotentiallyCollidingPairs();
		ENGINE_API void EnableDisableJoints();
		ENGINE_API FReal UpdateStepTime(const FReal DeltaTime, const FReal MaxStepTime);

		ENGINE_API void UpdateStatCounters();
		ENGINE_API void DebugDrawStaticParticles();
		ENGINE_API void DebugDrawKinematicParticles();
		ENGINE_API void DebugDrawDynamicParticles();
		ENGINE_API void DebugDrawConstraints();
		ENGINE_API void DebugDrawSimulationSpace();

		struct FImplementation;
		TUniquePtr<FImplementation> Implementation;

#if CHAOS_DEBUG_NAME
	public:
		void SetDebugName(const FName& Name)
		{
			DebugName = Name;
		}

		const FName& GetDebugName() const
		{
			return DebugName;
		}

	private:
	FName DebugName;
#endif

#if WITH_CHAOS_VISUAL_DEBUGGER
	private:
		FChaosVDContext CVDContextData;

	public:
		FChaosVDContext& GetChaosVDContextData()
		{
			return CVDContextData;
		};
#endif
	};

}
