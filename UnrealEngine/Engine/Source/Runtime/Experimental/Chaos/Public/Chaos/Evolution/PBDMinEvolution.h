// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/Collision/CollisionApplyType.h"
#include "Chaos/Evolution/ConstraintGroupSolver.h"
#include "Chaos/Evolution/SimulationSpace.h"
#include "Chaos/Evolution/SolverBodyContainer.h"
#include "Chaos/ParticleHandleFwd.h"


namespace Chaos
{
	class FBasicCollisionDetector;
	class FConstraintContainerSolver;
	class FPBDCollisionConstraints;
	class FSimpleConstraintRule;
	class FPBDRigidsSOAs;

	/**
	 * A minimal optimized evolution with support for
	 *	- PBD Rigids
	 *	- Joints
	 *	- Collisions
	 *
	 * It is single-threaded and does not use a constraint graph or partition the particles into islands.
	 */
	class CHAOS_API FPBDMinEvolution
	{
	public:
		using FCollisionDetector = FBasicCollisionDetector;
		using FEvolutionCallback = TFunction<void()>;
		using FRigidParticleSOAs = FPBDRigidsSOAs;

		FPBDMinEvolution(FRigidParticleSOAs& InParticles, TArrayCollectionArray<FVec3>& InPrevX, TArrayCollectionArray<FRotation3>& InPrevR, FCollisionDetector& InCollisionDetector, const FReal InBoundsExtension);
		~FPBDMinEvolution();

		void AddConstraintContainer(FPBDConstraintContainer& InContainer, const int32 Priority = 0);
		void SetConstraintContainerPriority(const int32 ContainerId, const int32 Priority);

		void Advance(const FReal StepDt, const int32 NumSteps, const FReal RewindDt);
		void AdvanceOneTimeStep(const FReal Dt, const FReal StepFraction);

		void SetNumPositionIterations(const int32 NumIts)
		{
			NumPositionIterations = NumIts;
		}

		void SetNumVelocityIterations(const int32 NumIts)
		{
			NumVelocityIterations = NumIts;
		}

		void SetNumProjectionIterations(const int32 NumIts)
		{
			NumProjectionIterations = NumIts;
		}

		void SetGravity(const FVec3& G)
		{
			Gravity = G;
		}

		void SetBoundsExtension(const FReal InBoundsExtension)
		{
			BoundsExtension = InBoundsExtension;
		}

		void SetSimulationSpace(const FSimulationSpace& InSimulationSpace)
		{
			SimulationSpace = InSimulationSpace;
		}

		FSimulationSpaceSettings& GetSimulationSpaceSettings()
		{
			return SimulationSpaceSettings;
		}

		const FSimulationSpaceSettings& GetSimulationSpaceSettings() const
		{
			return SimulationSpaceSettings;
		}

		void SetSimulationSpaceSettings(const FSimulationSpaceSettings& InSimulationSpaceSettings)
		{
			SimulationSpaceSettings = InSimulationSpaceSettings;
		}

	private:
		void PrepareTick();
		void UnprepareTick();
		void Rewind(FReal Dt, FReal RewindDt);
		void Integrate(FReal Dt);
		void ApplyKinematicTargets(FReal Dt, FReal StepFraction);
		void DetectCollisions(FReal Dt);
		void GatherInput(FReal Dt);
		void ScatterOutput(FReal Dt);
		void ApplyConstraintsPhase1(FReal Dt);
		void ApplyConstraintsPhase2(FReal Dt);
		void ApplyConstraintsPhase3(FReal Dt);

		FRigidParticleSOAs& Particles;
		FCollisionDetector& CollisionDetector;

		TArrayCollectionArray<FVec3>& ParticlePrevXs;
		TArrayCollectionArray<FRotation3>& ParticlePrevRs;

		TArray<FPBDConstraintContainer*> ConstraintContainers;
		FPBDSceneConstraintGroupSolver ConstraintSolver;

		int32 NumPositionIterations;
		int32 NumVelocityIterations;
		int32 NumProjectionIterations;
		FReal BoundsExtension;
		FVec3 Gravity;
		FSimulationSpaceSettings SimulationSpaceSettings;
		FSimulationSpace SimulationSpace;
	};
}
