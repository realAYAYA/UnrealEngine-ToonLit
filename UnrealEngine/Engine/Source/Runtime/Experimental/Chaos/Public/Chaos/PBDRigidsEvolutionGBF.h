// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ChaosPerfTest.h"
#include "Chaos/Collision/SpatialAccelerationBroadPhase.h"
#include "Chaos/Collision/SpatialAccelerationCollisionDetector.h"
#include "Chaos/Evolution/SolverBodyContainer.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDRigidClustering.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "Chaos/PerParticleAddImpulses.h"
#include "Chaos/PerParticleEtherDrag.h"
#include "Chaos/PerParticleEulerStepVelocity.h"
#include "Chaos/PerParticleExternalForces.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/PerParticleInitForce.h"
#include "Chaos/PerParticlePBDEulerStep.h"
#include "Chaos/CCDUtilities.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/PBDSuspensionConstraints.h"
#include "Chaos/ChaosDebugDraw.h"

namespace Chaos
{
	class FCollisionConstraintAllocator;
	class FChaosArchive;
	class IResimCacheBase;
	class FEvolutionResimCache;

	namespace CVars
	{
		CHAOS_API extern FRealSingle HackMaxAngularVelocity;
		CHAOS_API extern FRealSingle HackMaxVelocity;
		CHAOS_API extern FRealSingle SmoothedPositionLerpRate;
		CHAOS_API extern bool bChaosCollisionCCDUseTightBoundingBox;
		CHAOS_API extern int32 ChaosCollisionCCDConstraintMaxProcessCount;
		CHAOS_API extern int32 ChaosSolverDrawCCDThresholds;
	}

	using FPBDRigidsEvolutionCallback = TFunction<void()>;

	using FPBDRigidsEvolutionIslandCallback = TFunction<void(int32 Island)>;

	using FPBDRigidsEvolutionInternalHandleCallback = TFunction<void(
		const FGeometryParticleHandle* OldParticle,
		FGeometryParticleHandle* NewParticle)>;

	class FPBDRigidsEvolutionGBF : public FPBDRigidsEvolutionBase
	{
	public:
		using Base = FPBDRigidsEvolutionBase;

		using FGravityForces = FPerParticleGravity;
		using FCollisionConstraints = FPBDCollisionConstraints;
		using FCollisionDetector = FSpatialAccelerationCollisionDetector;
		using FExternalForces = FPerParticleExternalForces;
		using FJointConstraints = FPBDJointConstraints;

		// Default iteration counts
		static constexpr int32 DefaultNumPositionIterations = 8;
		static constexpr int32 DefaultNumVelocityIterations = 1;
		static constexpr int32 DefaultNumProjectionIterations = 1;
		static constexpr FRealSingle DefaultCollisionMarginFraction = 0.05f;
		static constexpr FRealSingle DefaultCollisionMarginMax = 10.0f;
		static constexpr FRealSingle DefaultCollisionCullDistance = 3.0f;
		static constexpr FRealSingle DefaultCollisionMaxPushOutVelocity = 1000.0f;
		static constexpr int32 DefaultRestitutionThreshold = 1000;

		CHAOS_API FPBDRigidsEvolutionGBF(FPBDRigidsSOAs& InParticles, THandleArray<FChaosPhysicsMaterial>& SolverPhysicsMaterials, const TArray<ISimCallbackObject*>* InCollisionModifiers = nullptr, bool InIsSingleThreaded = false);
		CHAOS_API ~FPBDRigidsEvolutionGBF();

		FORCEINLINE void SetPostIntegrateCallback(const FPBDRigidsEvolutionCallback& Cb)
		{
			PostIntegrateCallback = Cb;
		}

		FORCEINLINE void SetPostDetectCollisionsCallback(const FPBDRigidsEvolutionCallback& Cb)
		{
			PostDetectCollisionsCallback = Cb;
		}

		FORCEINLINE void SetPreApplyCallback(const FPBDRigidsEvolutionCallback& Cb)
		{
			PreApplyCallback = Cb;
		}

		FORCEINLINE void SetInternalParticleInitilizationFunction(const FPBDRigidsEvolutionInternalHandleCallback& Cb)
		{ 
			InternalParticleInitilization = Cb;
		}

		FORCEINLINE void DoInternalParticleInitilization(const FGeometryParticleHandle* OldParticle, FGeometryParticleHandle* NewParticle) 
		{ 
			if(InternalParticleInitilization)
			{
				InternalParticleInitilization(OldParticle, NewParticle);
			}
		}

		void SetIsDeterministic(const bool bInIsDeterministic);

		CHAOS_API void Advance(const FReal Dt, const FReal MaxStepDt, const int32 MaxSteps);
		CHAOS_API void AdvanceOneTimeStep(const FReal dt, const FSubStepInfo& SubStepInfo = FSubStepInfo());

		FORCEINLINE FCollisionConstraints& GetCollisionConstraints() { return CollisionConstraints; }
		FORCEINLINE const FCollisionConstraints& GetCollisionConstraints() const { return CollisionConstraints; }

		FORCEINLINE FCollisionDetector& GetCollisionDetector() { return CollisionDetector; }
		FORCEINLINE const FCollisionDetector& GetCollisionDetector() const { return CollisionDetector; }

		FORCEINLINE FGravityForces& GetGravityForces() { return GravityForces; }
		FORCEINLINE const FGravityForces& GetGravityForces() const { return GravityForces; }

		FORCEINLINE const FRigidClustering& GetRigidClustering() const { return Clustering; }
		FORCEINLINE FRigidClustering& GetRigidClustering() { return Clustering; }

		FORCEINLINE FJointConstraints& GetJointConstraints() { return JointConstraints; }
		FORCEINLINE const FJointConstraints& GetJointConstraints() const { return JointConstraints; }

		FORCEINLINE FPBDSuspensionConstraints& GetSuspensionConstraints() { return SuspensionConstraints; }
		FORCEINLINE const FPBDSuspensionConstraints& GetSuspensionConstraints() const { return SuspensionConstraints; }


		//
		// Particle API (most of the particle API is in the base class)
		//

		/**
		 * User has moved a particle
		 */
		CHAOS_API void SetParticleTransform(FGeometryParticleHandle* InParticle, const FVec3& InPos, const FRotation3& InRot, const bool bIsTeleport)
		{
			const FVec3 PrevX = InParticle->X();
			const FRotation3 PrevR = InParticle->R();

			FGenericParticleHandle(InParticle)->SetTransform(InPos, InRot);

			OnParticleMoved(InParticle, PrevX, PrevR, bIsTeleport);
		}

		CHAOS_API void OnParticleMoved(FGeometryParticleHandle* InParticle, const FVec3& PrevX, const FRotation3& PrevR, const bool bIsTeleport);


		/**
		 * Reload the particles cache for all particles where appropriate
		 */
		void ReloadParticlesCache();

		/**
		 * Build the list of disables particles and update the sleeping flag on the island
		 * @param Island Index of the island in which the cache will be used
		 * @param DisabledParticles List of islands disabled particles
		 * @param SleepedIslands List of islands sleeping state 
		 */
		void BuildDisabledParticles(const int32 Island, TArray<TArray<FPBDRigidParticleHandle*>>& DisabledParticles, TArray<bool>& SleepedIslands);

		void DestroyParticleCollisionsInAllocator(FGeometryParticleHandle* Particle);

		virtual void DestroyTransientConstraints(FGeometryParticleHandle* Particle) override;

		CHAOS_API inline void EndFrame(FReal Dt)
		{
			Particles.GetNonDisabledDynamicView().ParallelFor([&](auto& Particle, int32 Index) {
				Particle.Acceleration() = FVec3(0);
				Particle.AngularAcceleration() = FVec3(0);
			});
		}

		// Called when a the material changes one or more shapes on a particle. Required because collisions cache material properties
		void ParticleMaterialChanged(FGeometryParticleHandle* Particle);

		template<typename TParticleView>
		void Integrate(const TParticleView& InParticles, FReal Dt)
		{
			//SCOPE_CYCLE_COUNTER(STAT_Integrate);
			CHAOS_SCOPED_TIMER(Integrate);

			const FReal BoundsThickness = GetCollisionDetector().GetSettings().BoundsExpansion;
			const FReal MaxAngularSpeedSq = CVars::HackMaxAngularVelocity * CVars::HackMaxAngularVelocity;
			const FReal MaxSpeedSq = CVars::HackMaxVelocity * CVars::HackMaxVelocity;
			InParticles.ParallelFor([&](auto& GeomParticle, int32 Index) 
			{
				//question: can we enforce this at the API layer? Right now islands contain non dynamic which makes this hard
				auto PBDParticle = GeomParticle.CastToRigidParticle();
				if (PBDParticle && PBDParticle->ObjectState() == EObjectStateType::Dynamic)
				{
					auto& Particle = *PBDParticle;

					//save off previous velocities
					Particle.PreV() = Particle.V();
					Particle.PreW() = Particle.W();

					for (FForceRule ForceRule : ForceRules)
					{
						ForceRule(Particle, Dt);
					}

					//EulerStepVelocityRule.Apply(Particle, Dt);
					Particle.V() += Particle.Acceleration() * Dt;
					Particle.W() += Particle.AngularAcceleration() * Dt;


					//AddImpulsesRule.Apply(Particle, Dt);
					Particle.V() += Particle.LinearImpulseVelocity();
					Particle.W() += Particle.AngularImpulseVelocity();
					Particle.LinearImpulseVelocity() = FVec3(0);
					Particle.AngularImpulseVelocity() = FVec3(0);
					

					//EtherDragRule.Apply(Particle, Dt);
					{
						FVec3& V = Particle.V();
						FVec3& W = Particle.W();

						const FReal LinearDrag = LinearEtherDragOverride >= 0 ? LinearEtherDragOverride : Particle.LinearEtherDrag() * Dt;
						const FReal LinearMultiplier = FMath::Max(FReal(0), FReal(1) - LinearDrag);
						V *= LinearMultiplier;

						const FReal AngularDrag = AngularEtherDragOverride >= 0 ? AngularEtherDragOverride : Particle.AngularEtherDrag() * Dt;
						const FReal AngularMultiplier = FMath::Max(FReal(0), FReal(1) - AngularDrag);
						W *= AngularMultiplier;

						const FReal LinearSpeedSq = V.SizeSquared();
						const FReal AngularSpeedSq = W.SizeSquared();

						if (LinearSpeedSq > Particle.MaxLinearSpeedSq())
						{
							V *= FMath::Sqrt(Particle.MaxLinearSpeedSq() / LinearSpeedSq);
						}

						if (AngularSpeedSq > Particle.MaxAngularSpeedSq())
						{
							W *= FMath::Sqrt(Particle.MaxAngularSpeedSq() / AngularSpeedSq);
						}
					}

					if (CVars::HackMaxAngularVelocity >= 0.f)
					{
						const FReal AngularSpeedSq = Particle.W().SizeSquared();
						if (AngularSpeedSq > MaxAngularSpeedSq)
						{
							Particle.W() = Particle.W() * (CVars::HackMaxAngularVelocity / FMath::Sqrt(AngularSpeedSq));
						}
					}

					if (CVars::HackMaxVelocity >= 0.f)
					{
						const FReal SpeedSq = Particle.V().SizeSquared();
						if (SpeedSq > MaxSpeedSq)
						{
							Particle.V() = Particle.V() * (CVars::HackMaxVelocity / FMath::Sqrt(SpeedSq));
						}
					}

					//EulerStepRule.Apply(Particle, Dt);
					FVec3 PCoM = FParticleUtilitiesXR::GetCoMWorldPosition(&Particle);
					FRotation3 QCoM = FParticleUtilitiesXR::GetCoMWorldRotation(&Particle);

					PCoM = PCoM + Particle.V() * Dt;
					QCoM = FRotation3::IntegrateRotationWithAngularVelocity(QCoM, Particle.W(), Dt);

					FParticleUtilitiesPQ::SetCoMWorldTransform(&Particle, PCoM, QCoM);

					if (!Particle.CCDEnabled())
					{
						// Expand bounds about P/Q by a small amount. This can still result in missed collisions, especially
						// when we have joints that pull the body back to X/R, if P-X is greater than the BoundsThickness
						Particle.UpdateWorldSpaceState(FRigidTransform3(Particle.P(), Particle.Q()), FVec3(BoundsThickness));
					}
					else
					{

#if CHAOS_DEBUG_DRAW
						if (CVars::ChaosSolverDrawCCDThresholds)
						{
							DebugDraw::DrawCCDAxisThreshold(Particle.X(), Particle.CCDAxisThreshold(), Particle.P() - Particle.X(), Particle.Q());
						}
#endif

						if (CCDHelpers::DeltaExceedsThreshold(Particle.CCDAxisThreshold(), Particle.P() - Particle.X(), Particle.Q()))
						{
							// We sweep the bounds from P back along the velocity and expand by a small amount.
							// If not using tight bounds we also expand the bounds in all directions by Velocity. This is necessary only for secondary CCD collisions
							// @todo(chaos): expanding the bounds by velocity is very expensive - revisit this
							const FVec3 VDt = Particle.V() * Dt;
							FReal CCDBoundsExpansion = BoundsThickness;
							if (!CVars::bChaosCollisionCCDUseTightBoundingBox && (CVars::ChaosCollisionCCDConstraintMaxProcessCount > 1))
							{
								CCDBoundsExpansion += VDt.GetAbsMax();
							}
							Particle.UpdateWorldSpaceStateSwept(FRigidTransform3(Particle.P(), Particle.Q()), FVec3(CCDBoundsExpansion), -VDt);
						}
						else
						{
							Particle.UpdateWorldSpaceState(FRigidTransform3(Particle.P(), Particle.Q()), FVec3(BoundsThickness));
						}
					}
				}
			});

			for (auto& Particle : InParticles)
			{
				Base::DirtyParticle(Particle);
			}
		}

		CHAOS_API void Serialize(FChaosArchive& Ar);

		CHAOS_API TUniquePtr<IResimCacheBase> CreateExternalResimCache() const;
		CHAOS_API void SetCurrentStepResimCache(IResimCacheBase* InCurrentStepResimCache);

		CHAOS_API FSpatialAccelerationBroadPhase& GetBroadPhase() { return BroadPhase; }

		CHAOS_API void TransferJointConstraintCollisions();

		// Resets VSmooth value to something plausible based on external forces to prevent object from going back to sleep if it was just impulsed.
		template <bool bPersistent>
		void ResetVSmoothFromForces(TPBDRigidParticleHandleImp<FReal, 3, bPersistent>& Particle)
		{
			const FReal SmoothRate = FMath::Clamp(CVars::SmoothedPositionLerpRate, 0.0f, 1.0f);
	
			// Reset VSmooth to something roughly in the same direction as what V will be after integration.
			// This is temp fix, if this is only re-computed after solve, island will get incorrectly put back to sleep even if it was just impulsed.
			FReal FakeDT = (FReal)1. / (FReal)30.;
			if (Particle.LinearImpulseVelocity().IsNearlyZero() == false || Particle.Acceleration().IsNearlyZero() == false)
			{
				const FVec3 PredictedLinearVelocity = Particle.V() + Particle.Acceleration() * FakeDT + Particle.LinearImpulseVelocity();
				Particle.VSmooth() =FMath::Lerp(Particle.VSmooth(), PredictedLinearVelocity, SmoothRate);
			}
			if (Particle.AngularImpulseVelocity().IsNearlyZero() == false || Particle.AngularAcceleration().IsNearlyZero() == false)
			{
				const FVec3 PredictedAngularVelocity = Particle.W() + Particle.AngularAcceleration() * FakeDT + Particle.AngularImpulseVelocity();
				Particle.WSmooth() = FMath::Lerp(Particle.WSmooth(), PredictedAngularVelocity, SmoothRate);
			}
		}
		
	protected:

		CHAOS_API void AdvanceOneTimeStepImpl(const FReal dt, const FSubStepInfo& SubStepInfo);

		void UpdateInertiaConditioning();

		FEvolutionResimCache* GetCurrentStepResimCache()
		{
			return CurrentStepResimCacheImp;
		}

		FRigidClustering Clustering;

		FPBDJointConstraints JointConstraints;
		FPBDSuspensionConstraints SuspensionConstraints;

		FGravityForces GravityForces;
		FCollisionConstraints CollisionConstraints;
		FSpatialAccelerationBroadPhase BroadPhase;
		FSpatialAccelerationCollisionDetector CollisionDetector;

		FPBDRigidsEvolutionCallback PostIntegrateCallback;
		FPBDRigidsEvolutionCallback PostDetectCollisionsCallback;
		FPBDRigidsEvolutionCallback PreApplyCallback;
		FPBDRigidsEvolutionInternalHandleCallback InternalParticleInitilization;
		FEvolutionResimCache* CurrentStepResimCacheImp;
		const TArray<ISimCallbackObject*>* CollisionModifiers;

		FCCDManager CCDManager;

		bool bIsDeterministic;

		// Test Mode for Collision issues
		// @todo(chaos): remove this when no longer needed
		void TestModeResetParticles();
		void TestModeResetCollisions();

		struct FTestModeParticleData
		{
			FVec3 X, P, V, W;
			FRotation3 R, Q;
		};
		TMap<FPBDRigidParticleHandle*, FTestModeParticleData> TestModeData;
		// End Test Mode
	};

}

