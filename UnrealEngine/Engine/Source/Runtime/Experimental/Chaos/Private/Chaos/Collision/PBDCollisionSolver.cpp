// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Collision/PBDCollisionSolver.h"

#include "Chaos/Collision/CollisionApplyType.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/Collision/SolverCollisionContainer.h"
#include "Chaos/CollisionResolution.h"
#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/CollisionResolutionUtil.h"
#include "Chaos/Evolution/SolverBodyContainer.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDCollisionConstraintsContact.h"
#include "Chaos/Utilities.h"

//PRAGMA_DISABLE_OPTIMIZATION

DEFINE_LOG_CATEGORY(LogChaosCollision);

namespace Chaos
{
	namespace CVars
	{
		extern int32 Chaos_Collision_UseShockPropagation;

		bool bChaos_PBDCollisionSolver_Position_SolveEnabled = true;
		float Chaos_PBDCollisionSolver_Position_MinInvMassScale = 0.77f;
		float Chaos_PBDCollisionSolver_Position_StaticFrictionStiffness = 0.5f;
		float Chaos_PBDCollisionSolver_Position_PositionSolverTolerance = 0.001f;		// cms
		float Chaos_PBDCollisionSolver_Position_RotationSolverTolerance = 0.001f;		// rads

		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Position_SolveEnabled(TEXT("p.Chaos.PBDCollisionSolver.Position.SolveEnabled"), bChaos_PBDCollisionSolver_Position_SolveEnabled, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Position_MinInvMassScale(TEXT("p.Chaos.PBDCollisionSolver.Position.MinInvMassScale"), Chaos_PBDCollisionSolver_Position_MinInvMassScale, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Position_StaticFrictionStiffness(TEXT("p.Chaos.PBDCollisionSolver.Position.StaticFriction.Stiffness"), Chaos_PBDCollisionSolver_Position_StaticFrictionStiffness, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Position_PositionSolverTolerance(TEXT("p.Chaos.PBDCollisionSolver.Position.PositionTolerance"), Chaos_PBDCollisionSolver_Position_PositionSolverTolerance, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Position_RotationSolverTolerance(TEXT("p.Chaos.PBDCollisionSolver.Position.RotationTolerance"), Chaos_PBDCollisionSolver_Position_RotationSolverTolerance, TEXT(""));

		bool bChaos_PBDCollisionSolver_Velocity_SolveEnabled = true;
		// If this is the same as Chaos_PBDCollisionSolver_Position_MinInvMassScale and all velocity iterations have shockpropagation, we avoid recalculating constraiunt-space mass
		float Chaos_PBDCollisionSolver_Velocity_MinInvMassScale = Chaos_PBDCollisionSolver_Position_MinInvMassScale;
		bool bChaos_PBDCollisionSolver_Velocity_FrictionEnabled = true;
		bool bChaos_PBDCollisionSolver_Velocity_AveragePointEnabled = true;

		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Velocity_SolveEnabled(TEXT("p.Chaos.PBDCollisionSolver.Velocity.SolveEnabled"), bChaos_PBDCollisionSolver_Velocity_SolveEnabled, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Velocity_MinInvMassScale(TEXT("p.Chaos.PBDCollisionSolver.Velocity.MinInvMassScale"), Chaos_PBDCollisionSolver_Velocity_MinInvMassScale, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Velocity_FrictionEnabled(TEXT("p.Chaos.PBDCollisionSolver.Velocity.FrictionEnabled"), bChaos_PBDCollisionSolver_Velocity_FrictionEnabled, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_PBDCollisionSolver_Velocity_AveragePointEnabled(TEXT("p.Chaos.PBDCollisionSolver.Velocity.AveragePointEnabled"), bChaos_PBDCollisionSolver_Velocity_AveragePointEnabled, TEXT(""));
	}
	using namespace CVars;


	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	FPBDCollisionSolver::FPBDCollisionSolver()
		: State()
	{
	}

	void FPBDCollisionSolver::EnablePositionShockPropagation()
	{
		SetShockPropagationInvMassScale(Chaos_PBDCollisionSolver_Position_MinInvMassScale);
	}

	void FPBDCollisionSolver::EnableVelocityShockPropagation()
	{
		SetShockPropagationInvMassScale(Chaos_PBDCollisionSolver_Velocity_MinInvMassScale);
	}

	void FPBDCollisionSolver::DisableShockPropagation()
	{
		SetShockPropagationInvMassScale(FReal(1));
	}

	void FPBDCollisionSolver::SetShockPropagationInvMassScale(const FSolverReal InvMassScale)
	{
		FConstraintSolverBody& Body0 = SolverBody0();
		FConstraintSolverBody& Body1 = SolverBody1();

		// Shock propagation decreases the inverse mass of bodies that are lower in the pile
		// of objects. This significantly improves stability of heaps and stacks. Height in the pile is indictaed by the "level". 
		// No need to set an inverse mass scale if the other body is kinematic (with inv mass of 0).
		// Bodies at the same level do not take part in shock propagation.
		if (Body0.IsDynamic() && Body1.IsDynamic() && (Body0.Level() != Body1.Level()))
		{
			// Set the inv mass scale of the "lower" body to make it heavier
			bool bInvMassUpdated = false;
			if (Body0.Level() < Body1.Level())
			{
				if (Body0.ShockPropagationScale() != InvMassScale)
				{
					Body0.SetShockPropagationScale(InvMassScale);
					bInvMassUpdated = true;
				}
			}
			else
			{
				if (Body1.ShockPropagationScale() != InvMassScale)
				{
					Body1.SetShockPropagationScale(InvMassScale);
					bInvMassUpdated = true;
				}
			}

			// If the masses changed, we need to rebuild the contact mass for each manifold point
			if (bInvMassUpdated)
			{
				for (int32 PointIndex = 0; PointIndex < State.NumManifoldPoints; ++PointIndex)
				{
					State.ManifoldPoints[PointIndex].UpdateMassNormal(Body0, Body1);
				}
			}
		}
	}

	void FPBDCollisionSolver::SolveVelocityAverage(const FSolverReal Dt)
	{
		FConstraintSolverBody& Body0 = SolverBody0();
		FConstraintSolverBody& Body1 = SolverBody1();

		// Generate a new contact point at the average of all the active contacts
		int32 NumActiveManifoldPoints = 0;
		FSolverVec3 RelativeContactPosition0 = FSolverVec3(0);
		FSolverVec3 RelativeContactPosition1 = FSolverVec3(0);
		FSolverVec3 WorldContactNormal = FSolverVec3(0);
		FSolverReal NetPushOutNormal = FSolverReal(0);
		FSolverReal WorldContactVelocityTargetNormal = FSolverReal(0);
		for (int32 PointIndex = 0; PointIndex < State.NumManifoldPoints; ++PointIndex)
		{
			FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = State.ManifoldPoints[PointIndex];
			if (SolverManifoldPoint.ShouldSolveVelocity())
			{
				RelativeContactPosition0 += SolverManifoldPoint.RelativeContactPosition0;
				RelativeContactPosition1 += SolverManifoldPoint.RelativeContactPosition1;
				WorldContactVelocityTargetNormal += SolverManifoldPoint.WorldContactVelocityTargetNormal;
				WorldContactNormal = SolverManifoldPoint.WorldContactNormal;	// Take last value - should all be similar
				NetPushOutNormal += SolverManifoldPoint.NetPushOutNormal;
				++NumActiveManifoldPoints;
			}
		}

		// Solve for velocity at the new conatct point
		// We only do this if we have multiple active contacts
		// NOTE: this average contact isn't really correct, especially when all contacts do not equally
		// contribute to the pushout (which is normal even for a simple box on a plane). E.g., the WorldContactVelocityTargetNormal
		// and NetPushOutNormal will not be right. The goal here though is just to get close to the correct result in fewer iterations
		// than we would have if we just solved the corners of the box in sequence.
		if (NumActiveManifoldPoints > 1)
		{
			const FSolverReal InvCount = FSolverReal(1) / FSolverReal(NumActiveManifoldPoints);

			FPBDCollisionSolverManifoldPoint AverageManifoldPoint;
			AverageManifoldPoint.RelativeContactPosition0 = RelativeContactPosition0 * InvCount;
			AverageManifoldPoint.RelativeContactPosition1 = RelativeContactPosition1 * InvCount;
			AverageManifoldPoint.WorldContactNormal = WorldContactNormal;
			AverageManifoldPoint.WorldContactVelocityTargetNormal = WorldContactVelocityTargetNormal * InvCount;
			
			// Total pushout (not average) which is correct if the average point is also the centroid but otherwise probably an overestimate.
			// This is used to limit the possibly-attractive impulse that corrects implicit velocity errors from the PBD solve, but it
			// which might cause stickiness if overestimated. Although it should be recitifed when we solve the normal contact point velocities
			// so I don't think it matters...we could do better though if it is a problem
			AverageManifoldPoint.NetPushOutNormal = NetPushOutNormal;

			// Calculate the contact mass (and derived properties) for this point
			FSolverVec3 WorldContactNormalAngular0 = FSolverVec3(0);
			FSolverVec3 WorldContactNormalAngular1 = FSolverVec3(0);
			FSolverReal ContactMassInvNormal = FSolverReal(0);
			if (Body0.IsDynamic())
			{
				const FSolverVec3 R0xN = FSolverVec3::CrossProduct(RelativeContactPosition0, WorldContactNormal);
				WorldContactNormalAngular0 = Body0.InvI() * R0xN;
				ContactMassInvNormal += FSolverVec3::DotProduct(R0xN, WorldContactNormalAngular0) + Body0.InvM();
			}
			if (Body1.IsDynamic())
			{
				const FSolverVec3 R1xN = FSolverVec3::CrossProduct(RelativeContactPosition1, WorldContactNormal);
				WorldContactNormalAngular1 = Body1.InvI() * R1xN;
				ContactMassInvNormal += FSolverVec3::DotProduct(R1xN, WorldContactNormalAngular1) + Body1.InvM();
			}
			AverageManifoldPoint.ContactMassNormal = (ContactMassInvNormal > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvNormal : FSolverReal(0);
			AverageManifoldPoint.WorldContactNormalAngular0 = WorldContactNormalAngular0;
			AverageManifoldPoint.WorldContactNormalAngular1 = WorldContactNormalAngular1;

			// @todo(chaos): we don't use these - maybe do the calculation without an actual manifold point object...
			AverageManifoldPoint.ContactMassTangentU = FSolverReal(0);
			AverageManifoldPoint.ContactMassTangentV = FSolverReal(0);
			AverageManifoldPoint.WorldContactTangentU = FSolverVec3(0);
			AverageManifoldPoint.WorldContactTangentV = FSolverVec3(0);
			AverageManifoldPoint.WorldContactTangentUAngular0 = FSolverVec3(0);
			AverageManifoldPoint.WorldContactTangentVAngular0 = FSolverVec3(0);
			AverageManifoldPoint.WorldContactTangentUAngular1 = FSolverVec3(0);
			AverageManifoldPoint.WorldContactTangentVAngular1 = FSolverVec3(0);
			AverageManifoldPoint.WorldContactDeltaNormal = FSolverReal(0);
			AverageManifoldPoint.WorldContactDeltaTangentU = FSolverReal(0);
			AverageManifoldPoint.WorldContactDeltaTangentV = FSolverReal(0);
			AverageManifoldPoint.NetPushOutTangentU = FSolverReal(0);
			AverageManifoldPoint.NetPushOutTangentV = FSolverReal(0);
			AverageManifoldPoint.NetImpulseNormal = FSolverReal(0);
			AverageManifoldPoint.NetImpulseTangentU = FSolverReal(0);
			AverageManifoldPoint.NetImpulseTangentV = FSolverReal(0);
			AverageManifoldPoint.StaticFrictionRatio = FSolverReal(0);

			FSolverReal ContactVelocityDeltaNormal;
			AverageManifoldPoint.CalculateContactVelocityErrorNormal(Body0, Body1, ContactVelocityDeltaNormal);

			const FSolverReal MinImpulseNormal = FMath::Min(FSolverReal(0), -AverageManifoldPoint.NetPushOutNormal / Dt);

			ApplyVelocityCorrectionNormal(
				State.Stiffness,
				ContactVelocityDeltaNormal,
				MinImpulseNormal,
				AverageManifoldPoint,
				Body0,
				Body1);

			// Now distribute the net impulse among the active points so we don't over-correct pushout from initial overlaps
			for (int32 PointIndex = 0; PointIndex < State.NumManifoldPoints; ++PointIndex)
			{
				FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = State.ManifoldPoints[PointIndex];
				if (SolverManifoldPoint.ShouldSolveVelocity())
				{
					SolverManifoldPoint.NetImpulseNormal += AverageManifoldPoint.NetImpulseNormal * InvCount;
				}
			}
		}
	}

	bool FPBDCollisionSolver::SolveVelocity(const FSolverReal Dt, const bool bApplyDynamicFriction)
	{
		// Apply restitution at the average contact point
		// This means we don't need to run as many iterations to get stable bouncing
		// It also helps with zero restitution to counter any velocioty added by the PBD solve
		const bool bSolveAverageContact = (State.NumManifoldPoints > 1) && bChaos_PBDCollisionSolver_Velocity_AveragePointEnabled;
		if (bSolveAverageContact)
		{
			SolveVelocityAverage(Dt);
		}

		FConstraintSolverBody& Body0 = SolverBody0();
		FConstraintSolverBody& Body1 = SolverBody1();

		// NOTE: this dynamic friction implementation is iteration-count sensitive
		// @todo(chaos): fix iteration count dependence of dynamic friction
		const FSolverReal DynamicFriction = (bApplyDynamicFriction && (Dt > 0) && bChaos_PBDCollisionSolver_Velocity_FrictionEnabled) ? State.VelocityFriction : FSolverReal(0);

		for (int32 PointIndex = 0; PointIndex < State.NumManifoldPoints; ++PointIndex)
		{
			FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = State.ManifoldPoints[PointIndex];
			if (SolverManifoldPoint.ShouldSolveVelocity())
			{
				const FSolverReal MinImpulseNormal = FMath::Min(FSolverReal(0), -SolverManifoldPoint.NetPushOutNormal / Dt);

				if (DynamicFriction > 0)
				{
					FSolverReal ContactVelocityDeltaNormal, ContactVelocityDeltaTangentU, ContactVelocityDeltaTangentV;
					SolverManifoldPoint.CalculateContactVelocityError(Body0, Body1, DynamicFriction, Dt, ContactVelocityDeltaNormal, ContactVelocityDeltaTangentU, ContactVelocityDeltaTangentV);

					ApplyVelocityCorrection(
						State.Stiffness,
						Dt,
						DynamicFriction,
						ContactVelocityDeltaNormal,
						ContactVelocityDeltaTangentU,
						ContactVelocityDeltaTangentV,
						MinImpulseNormal,
						SolverManifoldPoint,
						Body0,
						Body1);
				}
				else
				{
					FSolverReal ContactVelocityDeltaNormal;
					SolverManifoldPoint.CalculateContactVelocityErrorNormal(Body0, Body1, ContactVelocityDeltaNormal);

					ApplyVelocityCorrectionNormal(
						State.Stiffness,
						ContactVelocityDeltaNormal,
						MinImpulseNormal,
						SolverManifoldPoint,
						Body0,
						Body1);
				}
			}
		}

		// Early-out support for the velocity solve is not currently very important because we
		// only run one iteration in the velocity solve phase.
		// @todo(chaos): support early-out in velocity solve if necessary
		return true;
	}
}