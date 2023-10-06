// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Collision/PBDCollisionContainerSolver.h"

#include "Chaos/Collision/CollisionApplyType.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/Collision/PBDCollisionSolverUtilities.h"
#include "Chaos/CollisionResolution.h"
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/Evolution/SolverBodyContainer.h"
#include "Chaos/Island/IslandManager.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDCollisionConstraintsContact.h"
#include "Chaos/Utilities.h"
#include "Templates/AlignmentTemplates.h"

// Private includes

#include "ChaosLog.h"

//PRAGMA_DISABLE_OPTIMIZATION

namespace Chaos
{
	namespace CVars
	{
		extern bool bChaos_PBDCollisionSolver_Position_SolveEnabled;
		extern bool bChaos_PBDCollisionSolver_Velocity_SolveEnabled;
		extern float Chaos_PBDCollisionSolver_Position_MinInvMassScale;
		extern float Chaos_PBDCollisionSolver_Velocity_MinInvMassScale;

		// If one body is more than MassRatio1 times the mass of the other, adjust the solver stiffness when the lighter body is underneath.
		// Solver stiffness will be equal to 1 when the mass ratio is MassRatio1.
		// Solver stiffness will be equal to 0 when the mass ratio is MassRatio2.
		FRealSingle Chaos_PBDCollisionSolver_AutoStiffness_MassRatio1 = 0;
		FRealSingle Chaos_PBDCollisionSolver_AutoStiffness_MassRatio2 = 0;
		FAutoConsoleVariableRef CVarChaosPBDCollisionSolverAutoStiffnessMassRatio1(TEXT("p.Chaos.PBDCollisionSolver.AutoStiffness.MassRatio1"), Chaos_PBDCollisionSolver_AutoStiffness_MassRatio1, TEXT(""));
		FAutoConsoleVariableRef CVarChaosPBDCollisionSolverAutoStiffnessMassRatio2(TEXT("p.Chaos.PBDCollisionSolver.AutoStiffness.MassRatio2"), Chaos_PBDCollisionSolver_AutoStiffness_MassRatio2, TEXT(""));

		// Jacobi solver stiffness
		// @todo(chaos): to be tuned
		FRealSingle Chaos_PBDCollisionSolver_JacobiStiffness = 0.5f;
		FAutoConsoleVariableRef CVarChaosPBDCollisionSolverJacobiStiffness(TEXT("p.Chaos.PBDCollisionSolver.JacobiStiffness"), Chaos_PBDCollisionSolver_JacobiStiffness, TEXT(""));

		// Jacobi position tolerance. Position corrections below this are zeroed.
		// @todo(chaos): to be tuned
		FRealSingle Chaos_PBDCollisionSolver_JacobiPositionTolerance = 1.e-6f;
		FAutoConsoleVariableRef CVarChaosPBDCollisionSolverJacobiPositionTolerance(TEXT("p.Chaos.PBDCollisionSolver.JacobiPositionTolerance"), Chaos_PBDCollisionSolver_JacobiPositionTolerance, TEXT(""));

		// Jacobi rotation tolerance. Rotation corrections below this are zeroed.
		// @todo(chaos): to be tuned
		FRealSingle Chaos_PBDCollisionSolver_JacobiRotationTolerance = 1.e-8f;
		FAutoConsoleVariableRef CVarChaosPBDCollisionSolverJacobiRotationTolerance(TEXT("p.Chaos.PBDCollisionSolver.JacobiRotationTolerance"), Chaos_PBDCollisionSolver_JacobiRotationTolerance, TEXT(""));
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////

	// Transform the Constraint's local-space data into world space for use by  the collision solver and also calculate tangents, errors, etc
	void UpdateCollisionSolverContactPointFromConstraint(
		Private::FPBDCollisionSolver& Solver, 
		const int32 SolverPointIndex, 
		const FPBDCollisionConstraint* Constraint, 
		const int32 ConstraintPointIndex, 
		const FRealSingle Dt, 
		const FConstraintSolverBody& Body0,
		const FConstraintSolverBody& Body1)
	{
		const FManifoldPoint& ManifoldPoint = Constraint->GetManifoldPoint(ConstraintPointIndex);

		const FRealSingle Restitution = FRealSingle(Constraint->GetRestitution());
		const FRealSingle RestitutionVelocityThreshold = FRealSingle(Constraint->GetRestitutionThreshold()) * Dt;

		// World-space shape transforms. Manifold data is currently relative to these spaces
		const FRigidTransform3& ShapeWorldTransform0 = Constraint->GetShapeWorldTransform0();
		const FRigidTransform3& ShapeWorldTransform1 = Constraint->GetShapeWorldTransform1();

		// World-space contact points on each shape
		const FVec3 WorldContact0 = ShapeWorldTransform0.TransformPositionNoScale(FVec3(ManifoldPoint.ContactPoint.ShapeContactPoints[0]));
		const FVec3 WorldContact1 = ShapeWorldTransform1.TransformPositionNoScale(FVec3(ManifoldPoint.ContactPoint.ShapeContactPoints[1]));
		const FVec3 WorldContact = FReal(0.5) * (WorldContact0 + WorldContact1);
		const FVec3f WorldRelativeContact0 = FVec3f(WorldContact - Body0.P());
		const FVec3f WorldRelativeContact1 = FVec3f(WorldContact - Body1.P());

		// World-space normal
		const FVec3f WorldContactNormal = FVec3f(ShapeWorldTransform1.TransformVectorNoScale(FVec3(ManifoldPoint.ContactPoint.ShapeContactNormal)));

		// World-space tangents
		FVec3f WorldContactTangentU = FVec3f::CrossProduct(FVec3f(0, 1, 0), WorldContactNormal);
		if (!WorldContactTangentU.Normalize(FRealSingle(UE_KINDA_SMALL_NUMBER)))
		{
			WorldContactTangentU = FVec3f::CrossProduct(FVec3f(1, 0, 0), WorldContactNormal);
			WorldContactTangentU = WorldContactTangentU.GetUnsafeNormal();
		}
		const FVec3f WorldContactTangentV = FVec3f::CrossProduct(WorldContactNormal, WorldContactTangentU);

		// Calculate contact velocity if we will need it below (restitution and/or frist-contact friction)
		const bool bNeedContactVelocity = (!ManifoldPoint.Flags.bHasStaticFrictionAnchor) || (Restitution > FRealSingle(0));
		FVec3f ContactVel = FVec3(0);
		if (bNeedContactVelocity)
		{
			const FVec3f ContactVel0 = Body0.V() + FVec3f::CrossProduct(Body0.W(), WorldRelativeContact0);
			const FVec3f ContactVel1 = Body1.V() + FVec3f::CrossProduct(Body1.W(), WorldRelativeContact1);
			ContactVel = ContactVel0 - ContactVel1;
		}

		// If we have contact data from a previous tick, use it to calculate the lateral position delta we need
		// to apply to move the contacts back to their original relative locations (i.e., to enforce static friction).
		// Otherwise, estimate the friction correction from the contact velocity.
		// NOTE: quadratic shapes use the velocity-based path most of the time, unless the relative motion is very small.
		FVec3f WorldFrictionDelta = FVec3f(0);
		if (ManifoldPoint.Flags.bHasStaticFrictionAnchor)
		{
			const FVec3f FrictionDelta0 = ShapeWorldTransform0.TransformPositionNoScale(FVec3(ManifoldPoint.ShapeAnchorPoints[0]));
			const FVec3f FrictionDelta1 = ShapeWorldTransform1.TransformPositionNoScale(FVec3(ManifoldPoint.ShapeAnchorPoints[1]));
			WorldFrictionDelta = FrictionDelta0 - FrictionDelta1;
		}
		else
		{
			// @todo(chaos): consider adding a multiplier to the initial contact friction
			WorldFrictionDelta = ContactVel * Dt;
		}

		// The contact point error we are trying to correct in this solver
		const FRealSingle TargetPhi = ManifoldPoint.TargetPhi;
		const FVec3f WorldContactDelta = FVec3f(WorldContact0 - WorldContact1);
		const FRealSingle WorldContactDeltaNormal = FVec3f::DotProduct(WorldContactDelta, WorldContactNormal) - TargetPhi;
		const FRealSingle WorldContactDeltaTangentU = FVec3f::DotProduct(WorldContactDelta + WorldFrictionDelta, WorldContactTangentU);
		const FRealSingle WorldContactDeltaTangentV = FVec3f::DotProduct(WorldContactDelta + WorldFrictionDelta, WorldContactTangentV);

		// The target contact velocity, taking restitution into account
		FRealSingle WorldContactTargetVelocityNormal = FRealSingle(0);
		if (Restitution > FRealSingle(0))
		{
			const FRealSingle ContactVelocityNormal = FVec3f::DotProduct(ContactVel, WorldContactNormal);
			if (ContactVelocityNormal < -RestitutionVelocityThreshold)
			{
				WorldContactTargetVelocityNormal = -Restitution * ContactVelocityNormal;
			}
		}

		Solver.InitManifoldPoint(
			SolverPointIndex,
			Dt,
			WorldRelativeContact0,
			WorldRelativeContact1,
			WorldContactNormal,
			WorldContactTangentU,
			WorldContactTangentV,
			WorldContactDeltaNormal,
			WorldContactDeltaTangentU,
			WorldContactDeltaTangentV,
			WorldContactTargetVelocityNormal);
	}

	void UpdateCollisionSolverManifoldFromConstraint(
		Private::FPBDCollisionSolver& Solver, 
		const FPBDCollisionConstraint* Constraint, 
		const FSolverReal Dt, 
		const int32 ConstraintPointBeginIndex, 
		const int32 ConstraintPointEndIndex)
	{
		const FConstraintSolverBody& Body0 = Solver.SolverBody0();
		const FConstraintSolverBody& Body1 = Solver.SolverBody1();

		// Only calculate state for newly added contacts. Normally this is all of them, but maybe not if incremental collision is used by RBAN.
		// Also we only add active points to the solver's manifold points list
		for (int32 ConstraintManifoldPointIndex = ConstraintPointBeginIndex; ConstraintManifoldPointIndex < ConstraintPointEndIndex; ++ConstraintManifoldPointIndex)
		{
			if (!Constraint->GetManifoldPoint(ConstraintManifoldPointIndex).Flags.bDisabled)
			{
				const int32 SolverManifoldPointIndex = Solver.AddManifoldPoint();

				// Transform the constraint contact data into world space for use by the solver
				// We build this data directly into the solver's world-space contact data which looks a bit odd with "Init" called after but there you go
				UpdateCollisionSolverContactPointFromConstraint(Solver, SolverManifoldPointIndex, Constraint, ConstraintManifoldPointIndex, Dt, Body0, Body1);
			}
		}

		Solver.FinalizeManifold();
	}

	void UpdateCollisionSolverFromConstraint(
		Private::FPBDCollisionSolver& Solver, 
		const FPBDCollisionConstraint* Constraint, 
		const FSolverReal Dt, 
		const FPBDCollisionSolverSettings& SolverSettings, 
		bool& bOutPerIterationCollision)
	{
		// Friction values. Static and Dynamic friction are applied in the position solve for most shapes.
		// We can also run in a mode without static friction at all. This is faster but stacking is not possible.
		const FSolverReal StaticFriction = FSolverReal(Constraint->GetStaticFriction());
		const FSolverReal DynamicFriction = FSolverReal(Constraint->GetDynamicFriction());
		FSolverReal PositionStaticFriction = FSolverReal(0);
		FSolverReal PositionDynamicFriction = FSolverReal(0);
		FSolverReal VelocityDynamicFriction = FSolverReal(0);
		if (SolverSettings.NumPositionFrictionIterations > 0)
		{
			PositionStaticFriction = StaticFriction;
			if (!Constraint->HasQuadraticShape())
			{
				PositionDynamicFriction = DynamicFriction;
			}
			else
			{
				// Quadratic shapes don't use PBD dynamic friction - it has issues at slow speeds where the WxR is
				// less than the position tolerance for friction point matching
				// @todo(chaos): fix PBD dynamic friction on quadratic shapes
				VelocityDynamicFriction = DynamicFriction;
			}
		}
		else
		{
			VelocityDynamicFriction = DynamicFriction;
		}

		Solver.SetFriction(PositionStaticFriction, PositionDynamicFriction, VelocityDynamicFriction);

		const FReal SolverStiffness = Constraint->GetStiffness();
		Solver.SetStiffness(FSolverReal(SolverStiffness));

		Solver.SolverBody0().SetInvMScale(Constraint->GetInvMassScale0());
		Solver.SolverBody0().SetInvIScale(Constraint->GetInvInertiaScale0());
		Solver.SolverBody0().SetShockPropagationScale(FReal(1));
		Solver.SolverBody1().SetInvMScale(Constraint->GetInvMassScale1());
		Solver.SolverBody1().SetInvIScale(Constraint->GetInvInertiaScale1());
		Solver.SolverBody1().SetShockPropagationScale(FReal(1));

		bOutPerIterationCollision = (!Constraint->GetUseManifold() || Constraint->GetUseIncrementalCollisionDetection());

		UpdateCollisionSolverManifoldFromConstraint(Solver, Constraint, Dt, 0, Constraint->NumManifoldPoints());
	}

	FORCEINLINE_DEBUGGABLE void UpdateCollisionConstraintFromSolver(FPBDCollisionConstraint* Constraint, const Private::FPBDCollisionSolver& Solver, const FSolverReal Dt)
	{
		Constraint->ResetSolverResults();

		// NOTE: We only put the non-pruned manifold points into the solver so the ManifoldPointIndex and
		// SolverManifoldPointIndex do not necessarily match. See GatherManifoldPoints
		int32 SolverManifoldPointIndex = 0;
		for (int32 ManifoldPointIndex = 0; ManifoldPointIndex < Constraint->NumManifoldPoints(); ++ManifoldPointIndex)
		{
			FSolverVec3 NetPushOut = FSolverVec3(0);
			FSolverVec3 NetImpulse = FSolverVec3(0);
			FSolverReal StaticFrictionRatio = FSolverReal(0);

			if (!Constraint->GetManifoldPoint(ManifoldPointIndex).Flags.bDisabled)
			{
				const Private::FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = Solver.GetManifoldPoint(SolverManifoldPointIndex);

				NetPushOut =
					SolverManifoldPoint.NetPushOutNormal * SolverManifoldPoint.ContactNormal +
					SolverManifoldPoint.NetPushOutTangentU * SolverManifoldPoint.ContactTangentU +
					SolverManifoldPoint.NetPushOutTangentV * SolverManifoldPoint.ContactTangentV;

				NetImpulse =
					SolverManifoldPoint.NetImpulseNormal * SolverManifoldPoint.ContactNormal +
					SolverManifoldPoint.NetImpulseTangentU * SolverManifoldPoint.ContactTangentU +
					SolverManifoldPoint.NetImpulseTangentV * SolverManifoldPoint.ContactTangentV;

				StaticFrictionRatio = SolverManifoldPoint.StaticFrictionRatio;

				++SolverManifoldPointIndex;
			}

			// NOTE: We call this even for points we did not run the solver for (but with zero results)
			Constraint->SetSolverResults(ManifoldPointIndex,
				NetPushOut,
				NetImpulse,
				StaticFrictionRatio,
				Dt);
		}

		Constraint->EndTick();
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	FPBDCollisionContainerSolver::FPBDCollisionContainerSolver(const FPBDCollisionConstraints& InConstraintContainer, const int32 InPriority)
		: FConstraintContainerSolver(InPriority)
		, ConstraintContainer(InConstraintContainer)
		, CollisionConstraints()
		, AppliedShockPropagation(1)
		, Scratch()
		, CollisionSolvers()
		, bCollisionConstraintPerIterationCollisionDetection()
		, bPerIterationCollisionDetection(false)
	{
	}

	FPBDCollisionContainerSolver::~FPBDCollisionContainerSolver()
	{
	}

	void FPBDCollisionContainerSolver::Reset(const int32 MaxCollisions)
	{
		AppliedShockPropagation = FSolverReal(1);

		// A over-allocation policy to avoid reallocation every frame in the common case where a pile of objects is dropped
		// and the number of contacts increases every tick.
		int CollisionBufferNum = MaxCollisions;
		if (CollisionBufferNum > CollisionConstraints.Max())
		{
			CollisionBufferNum = (5 * MaxCollisions) / 4; // +25%
		}

		CollisionConstraints.Reset(CollisionBufferNum);
		bCollisionConstraintPerIterationCollisionDetection.Reset(CollisionBufferNum);

		// Just set the array size for these right away - all data will be initialized later
		bCollisionConstraintPerIterationCollisionDetection.SetNumUninitialized(MaxCollisions, false);

		// Prepare the scratch buffer
		const size_t AlignedSolverSize = Align(sizeof(Private::FPBDCollisionSolver), alignof(Private::FPBDCollisionSolver));
		const size_t AlignedPointSize = Align(sizeof(Private::FPBDCollisionSolverManifoldPoint), alignof(Private::FPBDCollisionSolverManifoldPoint));
		const size_t ScratchSize = CollisionBufferNum * (AlignedSolverSize + Private::FPBDCollisionSolver::MaxPointsPerConstraint * AlignedPointSize);
		Scratch.Reset(ScratchSize);

		// Allocate scratch space for the collision solvers and manifold points
		CollisionSolvers = Scratch.AllocArray<Private::FPBDCollisionSolver>(MaxCollisions);
		CollisionSolverManifoldPoints = Scratch.AllocArray<Private::FPBDCollisionSolverManifoldPoint>(MaxCollisions);
		NumCollisionSolverManifoldPoints = 0;
	}

	void FPBDCollisionContainerSolver::AddConstraints()
	{
		Reset(ConstraintContainer.NumConstraints());

		for (FPBDCollisionConstraintHandle* ConstraintHandle : ConstraintContainer.GetConstraintHandles())
		{
			check(ConstraintHandle != nullptr);
			FPBDCollisionConstraint& Constraint = ConstraintHandle->GetContact();

			AddConstraint(Constraint);
		}
	}

	void FPBDCollisionContainerSolver::AddConstraints(const TArrayView<Private::FPBDIslandConstraint*>& IslandConstraints)
	{
		for (Private::FPBDIslandConstraint* IslandConstraint : IslandConstraints)
		{
			// We will only ever be given constraints from our container (asserts in non-shipping)
			FPBDCollisionConstraint& Constraint = IslandConstraint->GetConstraint()->AsUnsafe<FPBDCollisionConstraintHandle>()->GetContact();

			AddConstraint(Constraint);
		}
	}

	void FPBDCollisionContainerSolver::AddConstraint(FPBDCollisionConstraint& Constraint)
	{
		// NOTE: No need to add to CollisionSolvers or bCollisionConstraintPerIterationCollisionDetection
		const int32 Index = CollisionConstraints.Add(&Constraint);

		// Allocate the manifold points for this constraints solver
		// NOTE: we don't know how many points we will create if collision detection is deferred or incrememntal, so just allocate space for the max allowed
		// @todo(chaos): see if we can do better - we don't want to pay for this logic when it is only used rarely
		const bool bDeferredCollisionDetection = ConstraintContainer.GetDetectorSettings().bDeferNarrowPhase;
		const int32 ManifoldPointMax = (bDeferredCollisionDetection || Constraint.GetUseIncrementalManifold()) ? Private::FPBDCollisionSolver::MaxPointsPerConstraint : Constraint.NumManifoldPoints();

		if (ManifoldPointMax > 0)
		{
			GetSolver(Index).SetManifoldPointsBuffer(&CollisionSolverManifoldPoints[NumCollisionSolverManifoldPoints], ManifoldPointMax);
			NumCollisionSolverManifoldPoints += ManifoldPointMax;
		}
		else
		{
			GetSolver(Index).Reset();
		}
	}


	void FPBDCollisionContainerSolver::AddBodies(FSolverBodyContainer& SolverBodyContainer)
	{
		for (int32 SolverIndex = 0, SolverEndIndex = NumSolvers(); SolverIndex < SolverEndIndex; ++SolverIndex)
		{
			Private::FPBDCollisionSolver& CollisionSolver = GetSolver(SolverIndex);
			FPBDCollisionConstraint* Constraint = GetConstraint(SolverIndex);
			check(Constraint != nullptr);

			// Find the solver bodies for the particles we constrain. This will add them to the container
			// if they aren't there already, and ensure that they are populated with the latest data.
			FSolverBody* Body0 = SolverBodyContainer.FindOrAdd(Constraint->GetParticle0());
			FSolverBody* Body1 = SolverBodyContainer.FindOrAdd(Constraint->GetParticle1());
			check(Body0 != nullptr);
			check(Body1 != nullptr);

			CollisionSolver.SetSolverBodies(*Body0, *Body1);
		}
	}

	void FPBDCollisionContainerSolver::GatherInput(const FReal Dt)
	{
		GatherInput(Dt, 0, NumSolvers());
	}

	void FPBDCollisionContainerSolver::CachePrefetchSolver(const int32 ConstraintIndex) const
	{
		if (ConstraintIndex < NumSolvers())
		{
			FPlatformMisc::PrefetchBlock(GetConstraint(ConstraintIndex), sizeof(FPBDCollisionConstraint));
			FPlatformMisc::PrefetchBlock(&GetSolver(ConstraintIndex), sizeof(Private::FPBDCollisionSolver));
		}
	}

	void FPBDCollisionContainerSolver::GatherInput(const FReal InDt, const int32 BeginIndex, const int32 EndIndex)
	{
		// NOTE: may be called in parallel. Should not change the container or any elements outside of [BeginIndex, EndIndex)

		check(BeginIndex >= 0);
		check(EndIndex <= NumSolvers());

		const FSolverReal Dt = FSolverReal(InDt);

		const int32 PrefetchCount = 2;
		for (int32 PrefetchIndex = 0; PrefetchIndex < PrefetchCount; ++PrefetchIndex)
		{
			CachePrefetchSolver(PrefetchIndex);
		}

		bool bWantPerIterationCollisionDetection = false;
		for (int32 ConstraintIndex = BeginIndex; ConstraintIndex < EndIndex; ++ConstraintIndex)
		{
			CachePrefetchSolver(ConstraintIndex + PrefetchCount);

			Private::FPBDCollisionSolver& CollisionSolver = GetSolver(ConstraintIndex);
			FPBDCollisionConstraint* Constraint = GetConstraint(ConstraintIndex);
			bool& bPerIterationCollision = bCollisionConstraintPerIterationCollisionDetection[ConstraintIndex];

			UpdateCollisionSolverFromConstraint(CollisionSolver, Constraint, Dt, ConstraintContainer.GetSolverSettings(), bPerIterationCollision);

			// We need to run collision every iteration if we are not using manifolds, or are using incremental manifolds
			bWantPerIterationCollisionDetection = bWantPerIterationCollisionDetection || bPerIterationCollision;
		}

		if (bWantPerIterationCollisionDetection)
		{
			// We should lock here? We only ever set to true or do nothing so I think it doesn't matter if this happens on multiple threads...
			bPerIterationCollisionDetection = true;
		}
	}

	void FPBDCollisionContainerSolver::ScatterOutput(const FReal Dt)
	{
		ScatterOutput(Dt, 0, NumSolvers());
	}

	void FPBDCollisionContainerSolver::ScatterOutput(const FReal InDt, const int32 BeginIndex, const int32 EndIndex)
	{
		// NOTE: may be called in parallel. Should not change the container or any elements outside of [BeginIndex, EndIndex)

		check(BeginIndex >= 0);
		check(EndIndex <= NumSolvers());

		const FSolverReal Dt = FSolverReal(InDt);

		for (int32 ConstraintIndex = BeginIndex; ConstraintIndex < EndIndex; ++ConstraintIndex)
		{
			Private::FPBDCollisionSolver& CollisionSolver = GetSolver(ConstraintIndex);
			FPBDCollisionConstraint* Constraint = GetConstraint(ConstraintIndex);

			UpdateCollisionConstraintFromSolver(Constraint, CollisionSolver, Dt);

			// Reset the collision solver here as the pointers will be invalid on the next tick
			// @todo(chaos): Pointers are always reinitalized before use next tick so this isn't strictly necessary
			CollisionSolver.Reset();
		}
	}

	void FPBDCollisionContainerSolver::ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts)
	{
		SolvePositionImpl(Dt, It, NumIts, 0, NumSolvers(), ConstraintContainer.GetSolverSettings());
	}

	void FPBDCollisionContainerSolver::ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts)
	{
		SolveVelocityImpl(Dt, It, NumIts, 0, NumSolvers(), ConstraintContainer.GetSolverSettings());
	}

	void FPBDCollisionContainerSolver::ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts)
	{
		// Not supported for collisions
	}

	void FPBDCollisionContainerSolver::ApplyShockPropagation(const FSolverReal ShockPropagation)
	{
		// @todo(chaos): cache the mass scales so we don't have to look in the constraint again
		if (ShockPropagation != AppliedShockPropagation)
		{
			for (int32 SolverIndex = 0; SolverIndex < NumSolvers(); ++SolverIndex)
			{
				const FPBDCollisionConstraint* Constraint = GetConstraint(SolverIndex);
				Private::FPBDCollisionSolver& Solver = GetSolver(SolverIndex);
				if (Constraint != nullptr)
				{
					FConstraintSolverBody& Body0 = Solver.SolverBody0();
					FConstraintSolverBody& Body1 = Solver.SolverBody1();

					FSolverReal ShockPropagation0, ShockPropagation1;
					if (Private::CalculateBodyShockPropagation(Body0.SolverBody(), Body1.SolverBody(), ShockPropagation, ShockPropagation0, ShockPropagation1))
					{
						Body0.SetShockPropagationScale(ShockPropagation0);
						Body1.SetShockPropagationScale(ShockPropagation1);
						Solver.UpdateMassNormal();
					}
				}
			}

			AppliedShockPropagation = ShockPropagation;
		}
	}

	void FPBDCollisionContainerSolver::UpdatePositionShockPropagation(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings)
	{
		const bool bEnableShockPropagation = (It >= NumIts - SolverSettings.NumPositionShockPropagationIterations);
		const FSolverReal ShockPropagation = (bEnableShockPropagation) ? CVars::Chaos_PBDCollisionSolver_Position_MinInvMassScale : FSolverReal(1);
		ApplyShockPropagation(ShockPropagation);
	}

	void FPBDCollisionContainerSolver::UpdateVelocityShockPropagation(const FReal Dt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings)
	{
		const bool bEnableShockPropagation = (It >= NumIts - SolverSettings.NumVelocityShockPropagationIterations);
		const FSolverReal ShockPropagation = (bEnableShockPropagation) ? CVars::Chaos_PBDCollisionSolver_Velocity_MinInvMassScale : FSolverReal(1);
		ApplyShockPropagation(ShockPropagation);
	}

	void FPBDCollisionContainerSolver::SolvePositionImpl(const FReal InDt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings)
	{
		SCOPE_CYCLE_COUNTER(STAT_Collisions_Apply);
		if (!CVars::bChaos_PBDCollisionSolver_Position_SolveEnabled)
		{
			return;
		}

		if (EndIndex <= BeginIndex)
		{
			return;
		}

		UpdatePositionShockPropagation(InDt, It, NumIts, BeginIndex, EndIndex, SolverSettings);

		// We run collision detection here under two conditions (normally it is run after Integration and before the constraint solver phase):
		// 1) When deferring collision detection until the solver phase for better joint-collision behaviour (RBAN). In this case, we only do this on the first iteration.
		// 2) When using no manifolds or incremental manifolds, where we may add/replace manifold points every iteration.
		const bool bRunDeferredCollisionDetection = (It == 0) && ConstraintContainer.GetDetectorSettings().bDeferNarrowPhase;
		if (bRunDeferredCollisionDetection || bPerIterationCollisionDetection)
		{
			UpdateCollisions(InDt, BeginIndex, EndIndex);
		}

		// Only apply friction for the last few (tunable) iterations
		// Adjust max pushout to attempt to make it iteration count independent
		const FSolverReal Dt = FSolverReal(InDt);
		const bool bApplyStaticFriction = (It >= (NumIts - SolverSettings.NumPositionFrictionIterations));
		const FSolverReal MaxPushOut = (SolverSettings.MaxPushOutVelocity > 0) ? (FSolverReal(SolverSettings.MaxPushOutVelocity) * Dt) / FSolverReal(NumIts) : FSolverReal(0);

		// Apply the position correction
		if (bApplyStaticFriction)
		{
			for (int32 SolverIndex = 0; SolverIndex < NumSolvers(); ++SolverIndex)
			{
				CollisionSolvers[SolverIndex].SolvePositionWithFriction(Dt, MaxPushOut);
			}
		}
		else
		{
			for (int32 SolverIndex = 0; SolverIndex < NumSolvers(); ++SolverIndex)
			{
				CollisionSolvers[SolverIndex].SolvePositionNoFriction(Dt, MaxPushOut);
			}
		}
	}

	void FPBDCollisionContainerSolver::SolveVelocityImpl(const FReal InDt, const int32 It, const int32 NumIts, const int32 BeginIndex, const int32 EndIndex, const FPBDCollisionSolverSettings& SolverSettings)
	{
		SCOPE_CYCLE_COUNTER(STAT_Collisions_ApplyPushOut);
		if (!CVars::bChaos_PBDCollisionSolver_Velocity_SolveEnabled)
		{
			return;
		}

		if (EndIndex <= BeginIndex)
		{
			return;
		}

		UpdateVelocityShockPropagation(InDt, It, NumIts, BeginIndex, EndIndex, SolverSettings);

		const FSolverReal Dt = FSolverReal(InDt);
		const bool bApplyDynamicFriction = (It >= NumIts - SolverSettings.NumVelocityFrictionIterations);

		for (int32 SolverIndex = 0; SolverIndex < NumSolvers(); ++SolverIndex)
		{
			CollisionSolvers[SolverIndex].SolveVelocity(Dt, bApplyDynamicFriction);
		}
	}

	void FPBDCollisionContainerSolver::UpdateCollisions(const FReal InDt, const int32 BeginIndex, const int32 EndIndex)
	{
		const FSolverReal Dt = FSolverReal(InDt);
		const bool bDeferredCollisionDetection = ConstraintContainer.GetDetectorSettings().bDeferNarrowPhase;

		bool bNeedsAnotherIteration = false;
		for (int32 SolverIndex = BeginIndex; SolverIndex < EndIndex; ++SolverIndex)
		{
			if (bDeferredCollisionDetection || bCollisionConstraintPerIterationCollisionDetection[SolverIndex])
			{
				Private::FPBDCollisionSolver& CollisionSolver = GetSolver(SolverIndex);
				FPBDCollisionConstraint* Constraint = GetConstraint(SolverIndex);

				// Run collision detection at the current transforms including any correction from previous iterations
				const FConstraintSolverBody& Body0 = CollisionSolver.SolverBody0();
				const FConstraintSolverBody& Body1 = CollisionSolver.SolverBody1();
				const FRigidTransform3 CorrectedActorWorldTransform0 = FRigidTransform3(Body0.CorrectedActorP(), Body0.CorrectedActorQ());
				const FRigidTransform3 CorrectedActorWorldTransform1 = FRigidTransform3(Body1.CorrectedActorP(), Body1.CorrectedActorQ());
				const FRigidTransform3 CorrectedShapeWorldTransform0 = Constraint->GetShapeRelativeTransform0() * CorrectedActorWorldTransform0;
				const FRigidTransform3 CorrectedShapeWorldTransform1 = Constraint->GetShapeRelativeTransform1() * CorrectedActorWorldTransform1;

				// @todo(chaos): this is ugly - pass these to the required functions instead and remove from the constraint class
				// This is now only needed for LevelSet collision (see UpdateLevelsetLevelsetConstraint)
				Constraint->SetSolverBodies(&Body0.SolverBody(), &Body1.SolverBody());

				// Reset the manifold if we are not using manifolds (we just use the first manifold point)
				if (!Constraint->GetUseManifold())
				{
					Constraint->ResetActiveManifoldContacts();
					CollisionSolver.ResetManifold();
				}

				// We need to know how many points were added to the manifold
				const int32 BeginPointIndex = Constraint->NumManifoldPoints();

				// NOTE: We deliberately have not updated the ShapwWorldTranforms on the constraint. If we did that, we would calculate 
				// errors incorrectly in UpdateManifoldPoints, because the solver assumes nothing has been moved as we iterate (we accumulate 
				// corrections that will be applied later.)
				Constraint->ResetPhi(Constraint->GetCullDistance());
				Collisions::UpdateConstraint(*Constraint, CorrectedShapeWorldTransform0, CorrectedShapeWorldTransform1, Dt);

				// Update the manifold based on the new or updated contacts
				UpdateCollisionSolverManifoldFromConstraint(CollisionSolver, Constraint, Dt, BeginPointIndex, Constraint->NumManifoldPoints());

				Constraint->SetSolverBodies(nullptr, nullptr);
			}
		}
	}

}
