// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Joint/PBDJointContainerSolver.h"
#include "Chaos/Joint/ChaosJointLog.h"
#include "Chaos/Island/IslandManager.h"

namespace Chaos
{
	namespace Private
	{
		///////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////

		// NOTE: Particles are passed to the solvers in reverse order to what they are in the container...
		FGeometryParticleHandle* GetJointParticle(FPBDJointConstraints& Constraints, const int32 ContainerConstraintIndex, const int32 ParticleIndex)
		{
			check(ParticleIndex >= 0);
			check(ParticleIndex < 2);

			const int32 SwappedIndex = 1 - ParticleIndex;
			return Constraints.GetConstrainedParticles(ContainerConstraintIndex)[SwappedIndex];
		}

		const FRigidTransform3& GetJointFrame(FPBDJointConstraints& Constraints, const int32 ContainerConstraintIndex, const int32 ParticleIndex)
		{
			check(ParticleIndex >= 0);
			check(ParticleIndex < 2);

			const int32 SwappedIndex = 1 - ParticleIndex;
			return Constraints.GetConstraintSettings(ContainerConstraintIndex).ConnectorTransforms[SwappedIndex];
		}


		// @todo(chaos): ShockPropagation needs to handle the parent/child being in opposite order
		FReal GetJointShockPropagationInvMassScale(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FPBDJointSolverSettings& Settings, const FPBDJointSettings& JointSettings, const int32 It, const int32 NumIts)
		{
			// Shock propagation is only enabled for the last iteration, and only for the QPBD solver.
			// The standard PBD solver runs projection in the second solver phase which is mostly the same thing.
			if (JointSettings.bShockPropagationEnabled && (It >= (NumIts - Settings.NumShockPropagationIterations)))
			{
				if (Body0.IsDynamic() && Body1.IsDynamic())
				{
					return FPBDJointUtilities::GetShockPropagationInvMassScale(Settings, JointSettings);
				}
			}
			return FReal(1);
		}

		FReal GetJointIterationStiffness(const FPBDJointSolverSettings& Settings, int32 It, int32 NumIts)
		{
			// Linearly interpolate betwwen MinStiffness and MaxStiffness over the first few iterations,
			// then clamp at MaxStiffness for the final NumIterationsAtMaxStiffness
			FReal IterationStiffness = Settings.MaxSolverStiffness;
			if (NumIts > Settings.NumIterationsAtMaxSolverStiffness)
			{
				const FReal Interpolant = FMath::Clamp((FReal)It / (FReal)(NumIts - Settings.NumIterationsAtMaxSolverStiffness), 0.0f, 1.0f);
				IterationStiffness = FMath::Lerp(Settings.MinSolverStiffness, Settings.MaxSolverStiffness, Interpolant);
			}
			return FMath::Clamp(IterationStiffness, 0.0f, 1.0f);
		}

		bool GetJointShouldBreak(const FPBDJointSettings& JointSettings, const FReal Dt, const FVec3& LinearImpulse, const FVec3& AngularImpulse)
		{
			// NOTE: LinearImpulse/AngularImpulse are not really impulses - they are mass-weighted position/rotation delta, or (impulse x dt).
			// The Threshold is a force limit, so we need to convert it to a position delta caused by that force in one timestep

			bool bBreak = false;
			if (!bBreak && JointSettings.LinearBreakForce != FLT_MAX)
			{
				const FReal LinearForceSq = LinearImpulse.SizeSquared() / (Dt * Dt * Dt * Dt);
				const FReal LinearThresholdSq = FMath::Square(JointSettings.LinearBreakForce);
				bBreak = LinearForceSq > LinearThresholdSq;
			}

			if (!bBreak && JointSettings.AngularBreakTorque != FLT_MAX)
			{
				const FReal AngularForceSq = AngularImpulse.SizeSquared() / (Dt * Dt * Dt * Dt);
				const FReal AngularThresholdSq = FMath::Square(JointSettings.AngularBreakTorque);
				bBreak = AngularForceSq > AngularThresholdSq;
			}

			return bBreak;
		}

		///////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////


		FPBDJointContainerSolver::FPBDJointContainerSolver(FPBDJointConstraints& InConstraintContainer, const int32 InPriority)
			: FConstraintContainerSolver(InPriority)
			, ConstraintContainer(InConstraintContainer)
		{
		}

		FPBDJointContainerSolver::~FPBDJointContainerSolver()
		{
		}

		bool FPBDJointContainerSolver::UseLinearSolver() const
		{
			return ConstraintContainer.GetSettings().bUseLinearSolver;
		}

		void FPBDJointContainerSolver::Reset(const int32 InMaxConstraints)
		{
			ContainerConstraintIndices.Reset(InMaxConstraints);

			// NOTE: We presize the solver arrays to avoid repeated calls to Add(). We resize down later if
			// the InMaxConstraints turns out to be an over-estimate (which is should never be, currently)
			if (UseLinearSolver())
			{
				LinearConstraintSolvers.SetNum(InMaxConstraints);
				NonLinearConstraintSolvers.Empty();
			}
			else
			{
				LinearConstraintSolvers.Empty();
				NonLinearConstraintSolvers.SetNum(InMaxConstraints);
			}
		}

		void FPBDJointContainerSolver::AddConstraints()
		{
			Reset(ConstraintContainer.GetNumConstraints());

			// @todo(chaos): we could eliminate the index array if we're solving all constraints in the scene (RBAN)
			for (int32 ContainerConstraintIndex = 0; ContainerConstraintIndex < ConstraintContainer.GetNumConstraints(); ++ContainerConstraintIndex)
			{
				if (ConstraintContainer.IsConstraintEnabled(ContainerConstraintIndex))
				{
					AddConstraint(ContainerConstraintIndex);
				}
			}
		}

		void FPBDJointContainerSolver::AddConstraints(const TArrayView<Private::FPBDIslandConstraint*>& IslandConstraints)
		{
			for (Private::FPBDIslandConstraint* IslandConstraint : IslandConstraints)
			{
				// We will only ever be given constraints from our container (asserts in non-shipping)
				const int32 ContainerConstraintIndex = IslandConstraint->GetConstraint()->AsUnsafe<FPBDJointConstraintHandle>()->GetConstraintIndex();

				AddConstraint(ContainerConstraintIndex);
			}
		}

		void FPBDJointContainerSolver::AddConstraint(const int32 InContainerConstraintIndex)
		{
			// If this triggers, Reset was called with the wrong constraint count
			check(ContainerConstraintIndices.Num() < ContainerConstraintIndices.Max());

			// Only add a constraint if it is working on at least one dynamic body
			const FGenericParticleHandle Particle0 = GetJointParticle(ConstraintContainer, InContainerConstraintIndex, 0);
			const FGenericParticleHandle Particle1 = GetJointParticle(ConstraintContainer, InContainerConstraintIndex, 1);

			if (Particle0->IsDynamic() || Particle1->IsDynamic())
			{
				ContainerConstraintIndices.Add(InContainerConstraintIndex);
			}
		}

		void FPBDJointContainerSolver::AddBodies(FSolverBodyContainer& SolverBodyContainer)
		{
			for (int32 SolverConstraintIndex = 0, SolverConstraintEndIndex = ContainerConstraintIndices.Num(); SolverConstraintIndex < SolverConstraintEndIndex; ++SolverConstraintIndex)
			{
				const int32 ContainerConstraintIndex = ContainerConstraintIndices[SolverConstraintIndex];

				FGenericParticleHandle Particle0 = GetJointParticle(ConstraintContainer, ContainerConstraintIndex, 0);
				FGenericParticleHandle Particle1 = GetJointParticle(ConstraintContainer, ContainerConstraintIndex, 1);

				FSolverBody* SolverBody0 = SolverBodyContainer.FindOrAdd(Particle0);
				FSolverBody* SolverBody1 = SolverBodyContainer.FindOrAdd(Particle1);

				if (UseLinearSolver())
				{
					LinearConstraintSolvers[SolverConstraintIndex].SetSolverBodies(SolverBody0, SolverBody1);
				}
				else
				{
					NonLinearConstraintSolvers[SolverConstraintIndex].SetSolverBodies(SolverBody0, SolverBody1);
				}
			}
		}

		void FPBDJointContainerSolver::GatherInput(const FReal Dt)
		{
			GatherInput(Dt, 0, ContainerConstraintIndices.Num());
		}

		template<typename SolverType>
		void GatherInputImpl(const FPBDJointContainerSolver& Container, TArray<SolverType>& Solvers, const FReal Dt, const int32 SolverConstraintBeginIndex, const int32 SolverConstraintEndIndex)
		{
			const FPBDJointSolverSettings& SolverSettings = Container.GetSettings();

			for (int32 SolverConstraintIndex = SolverConstraintBeginIndex; SolverConstraintIndex < SolverConstraintEndIndex; ++SolverConstraintIndex)
			{
				const FPBDJointSettings& JointSettings = Container.GetConstraintSettings(SolverConstraintIndex);

				const int32 ContainerConstraintIndex = Container.GetContainerConstraintIndex(SolverConstraintIndex);
				FGenericParticleHandle Particle0 = GetJointParticle(Container.GetContainer(), ContainerConstraintIndex, 0);
				FGenericParticleHandle Particle1 = GetJointParticle(Container.GetContainer(), ContainerConstraintIndex, 1);
				const FRigidTransform3& Frame0 = GetJointFrame(Container.GetContainer(), ContainerConstraintIndex, 0);
				const FRigidTransform3& Frame1 = GetJointFrame(Container.GetContainer(), ContainerConstraintIndex, 1);

				Solvers[SolverConstraintIndex].Init(Dt, SolverSettings, JointSettings, Particle0->GetComRelativeTransform(Frame0), Particle1->GetComRelativeTransform(Frame1));
			}
		}

		void FPBDJointContainerSolver::GatherInput(const FReal Dt, const int32 ConstraintBeginIndex, const int32 ConstraintEndIndex)
		{
			if (UseLinearSolver())
			{
				GatherInputImpl(*this, LinearConstraintSolvers, Dt, ConstraintBeginIndex, ConstraintEndIndex);
			}
			else
			{
				GatherInputImpl(*this, NonLinearConstraintSolvers, Dt, ConstraintBeginIndex, ConstraintEndIndex);
			}
		}

		void FPBDJointContainerSolver::ScatterOutput(const FReal Dt)
		{
			ScatterOutput(Dt, 0, ContainerConstraintIndices.Num());
		}

		template<typename SolverType>
		void ScatterOutputImpl(const FPBDJointContainerSolver& Container, TArray<SolverType>& Solvers, const FReal Dt, const int32 SolverConstraintBeginIndex, const int32 SolverConstraintEndIndex)
		{
			for (int32 SolverConstraintIndex = SolverConstraintBeginIndex; SolverConstraintIndex < SolverConstraintEndIndex; ++SolverConstraintIndex)
			{
				const int32 ContainerConstraintIndex = Container.GetContainerConstraintIndex(SolverConstraintIndex);
				if (Dt > UE_SMALL_NUMBER)
				{
					SolverType& Solver = Solvers[SolverConstraintIndex];

					// NOTE: Particle order was revered in the solver...
					// NOTE: Solver impulses are positional impulses
					const FVec3 LinearImpulse = -Solver.GetNetLinearImpulse() / Dt;
					const FVec3 AngularImpulse = -Solver.GetNetAngularImpulse() / Dt;
					const FSolverBody* SolverBody0 = &Solver.Body0().SolverBody();
					const FSolverBody* SolverBody1 = &Solver.Body1().SolverBody();
					const bool bIsBroken = Solver.IsBroken();

					Container.GetContainer().SetSolverResults(ContainerConstraintIndex, LinearImpulse, AngularImpulse, bIsBroken, SolverBody0, SolverBody1);

					Solver.Deinit();
				}
				else
				{
					Container.GetContainer().SetSolverResults(ContainerConstraintIndex, FVec3(0), FVec3(0), false, nullptr, nullptr);
				}
			}
		}

		void FPBDJointContainerSolver::ScatterOutput(const FReal Dt, const int32 ConstraintBeginIndex, const int32 ConstraintEndIndex)
		{
			if (UseLinearSolver())
			{
				ScatterOutputImpl(*this, LinearConstraintSolvers, Dt, ConstraintBeginIndex, ConstraintEndIndex);
			}
			else
			{
				ScatterOutputImpl(*this, NonLinearConstraintSolvers, Dt, ConstraintBeginIndex, ConstraintEndIndex);
			}
		}

		void FPBDJointContainerSolver::ResizeSolverArrays()
		{
			// We may have conservatively allocated the solver arrays. If so, reduce their size now
			const int32 NumConstraints = GetNumConstraints();
			if (UseLinearSolver())
			{
				check(LinearConstraintSolvers.Num() >= NumConstraints);
				LinearConstraintSolvers.SetNum(NumConstraints);
			}
			else
			{
				check(NonLinearConstraintSolvers.Num() >= NumConstraints);
				NonLinearConstraintSolvers.SetNum(NumConstraints);
			}
		}

		// Apply position constraints for linear or non-linear solvers
		template<typename SolverType>
		void ApplyPositionConstraintsImpl(const FPBDJointContainerSolver& Container, TArray<SolverType>& Solvers, const FReal Dt, const int32 It, const int32 NumIts)
		{
			const FPBDJointSolverSettings& Settings = Container.GetSettings();
			const FReal IterationStiffness = GetJointIterationStiffness(Settings, It, NumIts);

			for (int32 SolverConstraintIndex = 0; SolverConstraintIndex < Solvers.Num(); ++SolverConstraintIndex)
			{
				SolverType& Solver = Solvers[SolverConstraintIndex];
				if (!Solver.RequiresSolve())
				{
					continue;
				}

				const FPBDJointSettings& JointSettings = Container.GetConstraintSettings(SolverConstraintIndex);
				Solver.Update(Dt, Settings, JointSettings);

				// Set parent inverse mass scale based on current shock propagation state
				const FReal ShockPropagationInvMassScale = GetJointShockPropagationInvMassScale(Solver.Body0(), Solver.Body1(), Settings, JointSettings, It, NumIts);
				Solver.SetShockPropagationScales(ShockPropagationInvMassScale, FReal(1), Dt);

				Solver.ApplyConstraints(Dt, IterationStiffness, Settings, JointSettings);

				// @todo(ccaulfield): We should be clamping the impulse at this point. Maybe move breaking to the solver
				if ((JointSettings.LinearBreakForce != FLT_MAX || JointSettings.AngularBreakTorque != FLT_MAX) &&
					GetJointShouldBreak(JointSettings, Dt, Solver.GetNetLinearImpulse(), Solver.GetNetAngularImpulse()))
				{
					Solver.SetIsBroken(true);
				}
			}
		}

		void FPBDJointContainerSolver::ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts)
		{
			ResizeSolverArrays();

			ApplyPositionConstraintsImpl(*this, LinearConstraintSolvers, Dt, It, NumIts);
			ApplyPositionConstraintsImpl(*this, NonLinearConstraintSolvers, Dt, It, NumIts);
		}

		// Apply velocity constraints for linear or non-linear solvers
		template<typename SolverType>
		void ApplyVelocityConstraintsImpl(const FPBDJointContainerSolver& Container, TArray<SolverType>& Solvers, const FReal Dt, const int32 It, const int32 NumIts)
		{
			const FPBDJointSolverSettings& Settings = Container.GetSettings();
			const FReal IterationStiffness = GetJointIterationStiffness(Settings, It, NumIts);

			for (int32 SolverConstraintIndex = 0; SolverConstraintIndex < Solvers.Num(); ++SolverConstraintIndex)
			{
				SolverType& Solver = Solvers[SolverConstraintIndex];
				if (!Solver.RequiresSolve())
				{
					continue;
				}

				const FPBDJointSettings& JointSettings = Container.GetConstraintSettings(SolverConstraintIndex);
				Solver.Update(Dt, Settings, JointSettings);

				// Set parent inverse mass scale based on current shock propagation state
				const FReal ShockPropagationInvMassScale = GetJointShockPropagationInvMassScale(Solver.Body0(), Solver.Body1(), Settings, JointSettings, It, NumIts);
				Solver.SetShockPropagationScales(ShockPropagationInvMassScale, FReal(1), Dt);

				Solver.ApplyVelocityConstraints(Dt, IterationStiffness, Settings, JointSettings);

				// @todo(chaos): should also add to net impulse and run break logic
			}
		}

		void FPBDJointContainerSolver::ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts)
		{
			ApplyVelocityConstraintsImpl(*this, LinearConstraintSolvers, Dt, It, NumIts);
			ApplyVelocityConstraintsImpl(*this, NonLinearConstraintSolvers, Dt, It, NumIts);
		}

		void FPBDJointContainerSolver::ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts)
		{
			ApplyLinearProjectionConstraints(Dt, It, NumIts);
			ApplyNonLinearProjectionConstraints(Dt, It, NumIts);
		}

		void FPBDJointContainerSolver::ApplyLinearProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts)
		{
			const FPBDJointSolverSettings& Settings = GetSettings();

			if (It == 0)
			{
				// Collect all the data for projection prior to the first iteration. 
				// This must happen for all joints before we project any joints so the the initial state for each joint is not polluted by any earlier projections.
				// @todo(chaos): if we ever support projection on other constraint types, we will need a PrepareProjection phase so that all constraint types
				// can initialize correctly before any constraints apply their projection. For now we can just check the iteration count is zero.
				for (int32 SolverConstraintIndex = 0; SolverConstraintIndex < LinearConstraintSolvers.Num(); ++SolverConstraintIndex)
				{
					FPBDJointCachedSolver& Solver = LinearConstraintSolvers[SolverConstraintIndex];
					if (!Solver.RequiresSolve())
					{
						continue;
					}

					const FPBDJointSettings& JointSettings = GetConstraintSettings(SolverConstraintIndex);
					if (!JointSettings.bProjectionEnabled)
					{
						continue;
					}

					Solver.InitProjection(Dt, Settings, JointSettings);
				}
			}

			for (int32 SolverConstraintIndex = 0; SolverConstraintIndex < LinearConstraintSolvers.Num(); ++SolverConstraintIndex)
			{
				FPBDJointCachedSolver& Solver = LinearConstraintSolvers[SolverConstraintIndex];
				if (!Solver.RequiresSolve())
				{
					continue;
				}

				const FPBDJointSettings& JointSettings = GetConstraintSettings(SolverConstraintIndex);
				if (!JointSettings.bProjectionEnabled)
				{
					continue;
				}

				if (It == 0)
				{
					Solver.ApplyTeleports(Dt, Settings, JointSettings);
				}

				const bool bLastIteration = (It == (NumIts - 1));
				Solver.ApplyProjections(Dt, Settings, JointSettings, bLastIteration);
			}
		}

		void FPBDJointContainerSolver::ApplyNonLinearProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts)
		{
			const FPBDJointSolverSettings& Settings = GetSettings();

			for (int32 SolverConstraintIndex = 0; SolverConstraintIndex < NonLinearConstraintSolvers.Num(); ++SolverConstraintIndex)
			{
				FPBDJointSolver& Solver = NonLinearConstraintSolvers[SolverConstraintIndex];
				if (!Solver.RequiresSolve())
				{
					continue;
				}

				const FPBDJointSettings& JointSettings = GetConstraintSettings(SolverConstraintIndex);
				if (!JointSettings.bProjectionEnabled)
				{
					continue;
				}

				Solver.Update(Dt, Settings, JointSettings);

				if (It == 0)
				{
					// @todo(chaos): support reverse parent/child
					Solver.Body1().UpdateRotationDependentState();
					Solver.UpdateMasses(FReal(0), FReal(1));
				}

				const bool bLastIteration = (It == (NumIts - 1));
				Solver.ApplyProjections(Dt, Settings, JointSettings, bLastIteration);
			}
		}

	}	// namespace Private
}	// namespace Chaos