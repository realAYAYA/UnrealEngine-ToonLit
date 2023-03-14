// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Evolution/PBDMinEvolution.h"
#include "Chaos/Collision/ParticlePairCollisionDetector.h"
#include "Chaos/Evolution/SolverConstraintContainer.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDConstraintContainer.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "Chaos/PerParticleAddImpulses.h"
#include "Chaos/PerParticleEtherDrag.h"
#include "Chaos/PerParticleEulerStepVelocity.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/PerParticleInitForce.h"
#include "Chaos/PerParticlePBDEulerStep.h"
#include "Chaos/PerParticlePBDGroundConstraint.h"
#include "Chaos/PerParticlePBDUpdateFromDeltaPosition.h"
#include "ChaosStats.h"

//PRAGMA_DISABLE_OPTIMIZATION

namespace Chaos
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosMinEvolution, Log, Warning);
#else
	CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosMinEvolution, Log, All);
#endif
	DEFINE_LOG_CATEGORY(LogChaosMinEvolution);

	DECLARE_CYCLE_STAT(TEXT("MinEvolution::Advance"), STAT_MinEvolution_Advance, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::PrepareTick"), STAT_MinEvolution_PrepareTick, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::UnprepareTick"), STAT_MinEvolution_UnprepareTick, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::Rewind"), STAT_MinEvolution_Rewind, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::AdvanceOneTimeStep"), STAT_MinEvolution_AdvanceOneTimeStep, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::Integrate"), STAT_MinEvolution_Integrate, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::KinematicTargets"), STAT_MinEvolution_KinematicTargets, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::Gather"), STAT_MinEvolution_Gather, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::Scatter"), STAT_MinEvolution_Scatter, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::ApplyConstraintsPhase1"), STAT_MinEvolution_ApplyConstraintsPhase1, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::ApplyConstraintsPhase2"), STAT_MinEvolution_ApplyConstraintsPhase2, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::ApplyConstraintsPhase3"), STAT_MinEvolution_ApplyConstraintsPhase3, STATGROUP_ChaosMinEvolution);
	DECLARE_CYCLE_STAT(TEXT("MinEvolution::DetectCollisions"), STAT_MinEvolution_DetectCollisions, STATGROUP_ChaosMinEvolution);

	//
	//
	//

	bool bChaos_MinEvolution_RewindLerp = true;
	FAutoConsoleVariableRef CVarChaosMinEvolutionRewindLerp(TEXT("p.Chaos.MinEvolution.RewindLerp"), bChaos_MinEvolution_RewindLerp, TEXT("If rewinding (fixed dt mode) use Backwards-Lerp as opposed to Backwards Velocity"));

	// Forced iteration count to evaluate worst-case behaviour for a given simulation
	bool Chaos_MinEvolution_ForceMaxConstraintIterations = false;
	FAutoConsoleVariableRef CVarChaosMinEvolutionForceMaxConstraintIterations(TEXT("p.Chaos.MinEvolution.ForceMaxConstraintIterations"), Chaos_MinEvolution_ForceMaxConstraintIterations, TEXT("Whether to force constraints to always use the worst-case maximum number of iterations"));

	//
	//
	//

	FPBDMinEvolution::FPBDMinEvolution(FRigidParticleSOAs& InParticles, TArrayCollectionArray<FVec3>& InPrevX, TArrayCollectionArray<FRotation3>& InPrevR, FCollisionDetector& InCollisionDetector, const FReal InBoundsExtension)
		: Particles(InParticles)
		, CollisionDetector(InCollisionDetector)
		, ParticlePrevXs(InPrevX)
		, ParticlePrevRs(InPrevR)
		, NumPositionIterations(0)
		, NumVelocityIterations(0)
		, NumProjectionIterations(0)
		, BoundsExtension(InBoundsExtension)
		, Gravity(FVec3(0))
		, SimulationSpaceSettings()
	{
	}

	FPBDMinEvolution::~FPBDMinEvolution()
	{
	}

	void FPBDMinEvolution::AddConstraintContainer(FPBDConstraintContainer& InContainer, const int32 Priority)
	{
		// Do not add twice
		check(InContainer.GetContainerId() == INDEX_NONE);

		const int32 ContainerId = ConstraintContainers.Add(&InContainer);
		InContainer.SetContainerId(ContainerId);

		ConstraintSolver.SetConstraintSolver(ContainerId, InContainer.CreateSceneSolver(Priority));
	}

	void FPBDMinEvolution::SetConstraintContainerPriority(const int32 ContainerId, const int32 Priority)
	{
		ConstraintSolver.SetConstraintSolverPriority(ContainerId, Priority);
	}

	void FPBDMinEvolution::Advance(const FReal StepDt, const int32 NumSteps, const FReal RewindDt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_Advance);

		PrepareTick();

		if (RewindDt > UE_SMALL_NUMBER)
		{
			Rewind(StepDt, RewindDt);
		}

		for (int32 Step = 0; Step < NumSteps; ++Step)
		{
			// StepFraction: how much of the remaining time this step represents, used to interpolate kinematic targets
			// E.g., for 4 steps this will be: 1/4, 1/2, 3/4, 1
			const FReal StepFraction = (FReal)(Step + 1) / (FReal)(NumSteps);

			UE_LOG(LogChaosMinEvolution, Verbose, TEXT("Advance dt = %f [%d/%d]"), StepDt, Step + 1, NumSteps);

			AdvanceOneTimeStep(StepDt, StepFraction);
		}

		for (TTransientPBDRigidParticleHandle<FReal, 3>& Particle : Particles.GetActiveParticlesView())
		{
			if (Particle.ObjectState() == EObjectStateType::Dynamic)
			{
				Particle.Acceleration() = FVec3(0);
				Particle.AngularAcceleration() = FVec3(0);
			}
		}

		UnprepareTick();
	}

	void FPBDMinEvolution::AdvanceOneTimeStep(const FReal Dt, const FReal StepFraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_AdvanceOneTimeStep);

		Integrate(Dt);

		ApplyKinematicTargets(Dt, StepFraction);

		DetectCollisions(Dt);

		if (Dt > 0)
		{
			GatherInput(Dt);

			ApplyConstraintsPhase1(Dt);

			ApplyConstraintsPhase2(Dt);

			ApplyConstraintsPhase3(Dt);

			ScatterOutput(Dt);
		}
	}

	// A opportunity for systems to allocate buffers for the duration of the tick, if they have enough info to do so
	void FPBDMinEvolution::PrepareTick()
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_PrepareTick);

		for (FPBDConstraintContainer* ConstraintContainer : ConstraintContainers)
		{
			ConstraintContainer->PrepareTick();
		}
	}

	void FPBDMinEvolution::UnprepareTick()
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_UnprepareTick);

		for (FPBDConstraintContainer* ConstraintContainer : ConstraintContainers)
		{
			ConstraintContainer->UnprepareTick();
		}
	}

	// Update X/R as if we started the next tick 'RewindDt' seconds ago.
	void FPBDMinEvolution::Rewind(FReal Dt, FReal RewindDt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_Rewind);

		if (bChaos_MinEvolution_RewindLerp)
		{
			const FReal T = (Dt - RewindDt) / Dt;
			UE_LOG(LogChaosMinEvolution, Verbose, TEXT("Rewind dt = %f; rt = %f; T = %f"), Dt, RewindDt, T);
			for (TTransientPBDRigidParticleHandle<FReal, 3>& Particle : Particles.GetActiveParticlesView())
			{
				if (Particle.ObjectState() == EObjectStateType::Dynamic)
				{
					Particle.X() = FVec3::Lerp(Particle.Handle()->AuxilaryValue(ParticlePrevXs), Particle.X(), T);
					Particle.R() = FRotation3::Slerp(Particle.Handle()->AuxilaryValue(ParticlePrevRs), Particle.R(), (decltype(FQuat::X))T);	// LWC_TODO: Remove decltype cast once FQuat supports variants
				}
			}
		}
		else
		{
			for (TTransientPBDRigidParticleHandle<FReal, 3>& Particle : Particles.GetActiveParticlesView())
			{
				if (Particle.ObjectState() == EObjectStateType::Dynamic)
				{
					const FVec3 XCoM = FParticleUtilitiesXR::GetCoMWorldPosition(&Particle);
					const FRotation3 RCoM = FParticleUtilitiesXR::GetCoMWorldRotation(&Particle);

					const FVec3 XCoM2 = XCoM - Particle.V() * RewindDt;
					const FRotation3 RCoM2 = FRotation3::IntegrateRotationWithAngularVelocity(RCoM, -Particle.W(), RewindDt);

					FParticleUtilitiesXR::SetCoMWorldTransform(&Particle, XCoM2, RCoM2);
				}
			}
		}

		for (auto& Particle : Particles.GetActiveKinematicParticlesView())
		{
			Particle.X() = Particle.X() - Particle.V() * RewindDt;
			Particle.R() = FRotation3::IntegrateRotationWithAngularVelocity(Particle.R(), -Particle.W(), RewindDt);
		}
	}

	void FPBDMinEvolution::Integrate(FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_Integrate);

		// Simulation space velocity and acceleration
		FVec3 SpaceV = FVec3(0);	// Velocity
		FVec3 SpaceW = FVec3(0);	// Angular Velocity
		FVec3 SpaceA = FVec3(0);	// Acceleration
		FVec3 SpaceB = FVec3(0);	// Angular Acceleration
		if (SimulationSpaceSettings.Alpha > 0.0f)
		{
			SpaceV = SimulationSpace.Transform.InverseTransformVector(SimulationSpace.LinearVelocity);
			SpaceW = SimulationSpace.Transform.InverseTransformVector(SimulationSpace.AngularVelocity);
			SpaceA = SimulationSpace.Transform.InverseTransformVector(SimulationSpace.LinearAcceleration);
			SpaceB = SimulationSpace.Transform.InverseTransformVector(SimulationSpace.AngularAcceleration);
		}

		for (TTransientPBDRigidParticleHandle<FReal, 3>& Particle : Particles.GetActiveParticlesView())
		{
			if (Particle.ObjectState() == EObjectStateType::Dynamic)
			{
				Particle.PreV() = Particle.V();
				Particle.PreW() = Particle.W();

				const FVec3 XCoM = FParticleUtilitiesXR::GetCoMWorldPosition(&Particle);
				const FRotation3 RCoM = FParticleUtilitiesXR::GetCoMWorldRotation(&Particle);
				
				// Forces and torques
				const FMatrix33 WorldInvI = Utilities::ComputeWorldSpaceInertia(RCoM, Particle.InvI());
				FVec3 DV = Particle.Acceleration() * Dt + Particle.LinearImpulseVelocity();
				FVec3 DW = Particle.AngularAcceleration() * Dt + Particle.AngularImpulseVelocity();
				FVec3 TargetV = FVec3(0);
				FVec3 TargetW = FVec3(0);

				// Gravity
				if (Particle.GravityEnabled())
				{
					DV += Gravity * Dt;
				}

				// Moving and accelerating simulation frame
				// https://en.wikipedia.org/wiki/Rotating_reference_frame
				if (SimulationSpaceSettings.Alpha > 0.0f)
				{
					const FVec3 CoriolisAcc = SimulationSpaceSettings.CoriolisAlpha * 2.0f * FVec3::CrossProduct(SpaceW, Particle.V());
					const FVec3 CentrifugalAcc = SimulationSpaceSettings.CentrifugalAlpha * FVec3::CrossProduct(SpaceW, FVec3::CrossProduct(SpaceW, XCoM));
					const FVec3 EulerAcc = SimulationSpaceSettings.EulerAlpha * FVec3::CrossProduct(SpaceB, XCoM);
					const FVec3 LinearAcc = SimulationSpaceSettings.LinearAccelerationAlpha * SpaceA;
					const FVec3 AngularAcc = SimulationSpaceSettings.AngularAccelerationAlpha * SpaceB;
					const FVec3 LinearDragAcc = SimulationSpaceSettings.ExternalLinearEtherDrag * SpaceV;
					DV -= SimulationSpaceSettings.Alpha * (LinearAcc + LinearDragAcc + CoriolisAcc + CentrifugalAcc + EulerAcc) * Dt;
					DW -= SimulationSpaceSettings.Alpha * AngularAcc * Dt;
					TargetV = -SimulationSpaceSettings.Alpha * SimulationSpaceSettings.LinearVelocityAlpha * SpaceV;
					TargetW = -SimulationSpaceSettings.Alpha * SimulationSpaceSettings.AngularVelocityAlpha * SpaceW;
				}

				// New velocity
				const FReal LinearDrag = FMath::Min(FReal(1), Particle.LinearEtherDrag() * Dt);
				const FReal AngularDrag = FMath::Min(FReal(1), Particle.AngularEtherDrag() * Dt);
				const FVec3 V = FMath::Lerp(Particle.V() + DV, TargetV, LinearDrag);
				const FVec3 W = FMath::Lerp(Particle.W() + DW, TargetW, AngularDrag);

				// New position
				const FVec3 PCoM = XCoM + V * Dt;
				const FRotation3 QCoM = FRotation3::IntegrateRotationWithAngularVelocity(RCoM, W, Dt);

				// Update particle state (forces are not zeroed until the end of the frame)
				FParticleUtilitiesPQ::SetCoMWorldTransform(&Particle, PCoM, QCoM);
				Particle.V() = V;
				Particle.W() = W;
				Particle.LinearImpulseVelocity() = FVec3(0);
				Particle.AngularImpulseVelocity() = FVec3(0);

				// Update cached world space state, including bounds. We use the Swept bounds update so that the bounds includes P,Q and X,Q.
				// This is because when we have joints, they often pull bodies back to their original positions, so we need to know if there
				// are contacts at that location.
				Particle.UpdateWorldSpaceStateSwept(FRigidTransform3(Particle.P(), Particle.Q()), FVec3(CollisionDetector.GetSettings().BoundsExpansion), -V * Dt);
			}
		}
	}

	// @todo(ccaulfield): dedupe (PBDRigidsEvolutionGBF)
	void FPBDMinEvolution::ApplyKinematicTargets(FReal Dt, FReal StepFraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_KinematicTargets);

		check(StepFraction > (FReal)0);
		check(StepFraction <= (FReal)1);

		// @todo(ccaulfield): optimize. Depending on the number of kinematics relative to the number that have 
		// targets set, it may be faster to process a command list rather than iterate over them all each frame. 
		const FReal MinDt = 1e-6f;
		for (auto& Particle : Particles.GetActiveKinematicParticlesView())
		{
			TKinematicTarget<FReal, 3>& KinematicTarget = Particle.KinematicTarget();
			const FVec3 CurrentX = Particle.X();
			const FRotation3 CurrentR = Particle.R();

			switch (KinematicTarget.GetMode())
			{
			case EKinematicTargetMode::None:
				// Nothing to do
				break;

			case EKinematicTargetMode::Reset:
			{
				// Reset velocity and then switch to do-nothing mode
				Particle.V() = FVec3(0);
				Particle.W() = FVec3(0);
				KinematicTarget.SetMode(EKinematicTargetMode::None);
				break;
			}

			case EKinematicTargetMode::Position:
			{
				// Move to kinematic target and update velocities to match
				// Target positions only need to be processed once, and we reset the velocity next frame (if no new target is set)
				FVec3 NewX;
				FRotation3 NewR;
				if (FMath::IsNearlyEqual(StepFraction, (FReal)1, (FReal)UE_KINDA_SMALL_NUMBER))
				{
					NewX = KinematicTarget.GetTarget().GetLocation();
					NewR = KinematicTarget.GetTarget().GetRotation();
					KinematicTarget.SetMode(EKinematicTargetMode::Reset);
				}
				else
				{
					// as a reminder, stepfraction is the remaing fraction of the step from the remaining steps
					// for total of 4 steps and current step of 2, this will be 1/3 ( 1 step passed, 3 steps remains )
					NewX = FVec3::Lerp(CurrentX, KinematicTarget.GetTarget().GetLocation(), StepFraction);
					NewR = FRotation3::Slerp(CurrentR, KinematicTarget.GetTarget().GetRotation(), (decltype(FQuat::X))StepFraction);		// LWC_TODO: Remove decltype cast once FQuat supports variants
				}
				if (Dt > MinDt)
				{
					Particle.V() = FVec3::CalculateVelocity(CurrentX, NewX, Dt);
					Particle.W() = FRotation3::CalculateAngularVelocity(CurrentR, NewR, Dt);
				}
				Particle.X() = NewX;
				Particle.R() = NewR;
				break;
			}

			case EKinematicTargetMode::Velocity:
			{
				// Move based on velocity
				Particle.X() = Particle.X() + Particle.V() * Dt;
				FRotation3::IntegrateRotationWithAngularVelocity(Particle.R(), Particle.W(), Dt);
				break;
			}
			}

			Particle.UpdateWorldSpaceState(FRigidTransform3(Particle.X(), Particle.R()), FVec3(BoundsExtension));
		}
	}

	void FPBDMinEvolution::DetectCollisions(FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_DetectCollisions);

		for (FPBDConstraintContainer* ConstraintContainer : ConstraintContainers)
		{
			ConstraintContainer->UpdatePositionBasedState(Dt);
		}

		CollisionDetector.DetectCollisions(Dt, nullptr);
		CollisionDetector.GetCollisionContainer().GetConstraintAllocator().PruneExpiredItems();
		CollisionDetector.GetCollisionContainer().GetConstraintAllocator().SortConstraintsHandles();
	}

	void FPBDMinEvolution::GatherInput(FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_Gather);

		ConstraintSolver.Reset();
		ConstraintSolver.AddConstraintsAndBodies();
		ConstraintSolver.GatherBodies(Dt);
		ConstraintSolver.GatherConstraints(Dt);
	}

	void FPBDMinEvolution::ScatterOutput(FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_Scatter);

		ConstraintSolver.ScatterConstraints(Dt);
		ConstraintSolver.ScatterBodies(Dt);

		for (auto& Particle : Particles.GetActiveParticlesView())
		{
			Particle.Handle()->AuxilaryValue(ParticlePrevXs) = Particle.X();
			Particle.Handle()->AuxilaryValue(ParticlePrevRs) = Particle.R();
			Particle.X() = Particle.P();
			Particle.R() = Particle.Q();
		}
	}

	void FPBDMinEvolution::ApplyConstraintsPhase1(FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_ApplyConstraintsPhase1);

		ConstraintSolver.ApplyPositionConstraints(Dt, NumPositionIterations);
	}

	void FPBDMinEvolution::ApplyConstraintsPhase2(FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_ApplyConstraintsPhase2);

		ConstraintSolver.ApplyVelocityConstraints(Dt, NumVelocityIterations);
	}

	void FPBDMinEvolution::ApplyConstraintsPhase3(FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_MinEvolution_ApplyConstraintsPhase3);

		ConstraintSolver.ApplyProjectionConstraints(Dt, NumProjectionIterations);
	}
}
