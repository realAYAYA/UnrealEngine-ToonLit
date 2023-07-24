// Copyright Epic Games, Inc. All Rights Reserved.

#include "Math/Vector.isph"
#include "Math/Quat.isph"
#include "Math/Transform.isph"
#include "Chaos/Matrix33.isph"

//
//
// STRUCTURES
//
//

export struct FSolverBody
{
	// World-space inverse inertia
	// @todo(chaos): do we need this, or should we force all systems to use the FConstraintSolverBody decorator?
	FMatrix33f InvI;

	// Actor-space center of mass rotation
	FVector4d RoM;

	// World-space rotation of mass at start of sub step
	FVector4d R;

	// Predicted world-space center of mass rotation (post-integration, pre-constraint-solve)
	FVector4d Q;

	// Actor-space center of mass location
	FVector3d CoM;

	// World-space center of mass state at start of sub step
	FVector3d X;

	// Predicted world-space center of mass position (post-integration, pre-constraint-solve)
	FVector3d P;


	// Local-space inverse inertia (diagonal, so only 3 elements)
	FVector3f InvILocal;

	// Inverse mass
	float InvM;

	// Net position delta applied by all constraints (constantly changing as we iterate over constraints)
	FVector3f DP;

	// Net rotation delta applied by all constraints (constantly changing as we iterate over constraints)
	FVector3f DQ;

	// Net position correction delta applied by all constraints (constantly changing as we iterate over constraints)
	// Will translate the body without introducing linear velocity
	FVector3f CP;

	// Net rotation correction delta applied by all constraints (constantly changing as we iterate over constraints)
	// Will rotate the body without introducing angular velocity
	FVector3f CQ;

	// World-space center of mass velocity
	FVector3f V;

	// World-space center of mass angular velocity
	FVector3f W;

	// Distance to a kinematic body (through the contact graph). Used by collision shock propagation
	int32 Level;

	float AlignmentPadding[2];
};

export struct FConstraintSolverBody
{
	// The body we decorate
	FSolverBody* Body;

	// The body modifiers
	float InvMScale;
	float InvIScale;
	float ShockPropagationScale;
};

export struct FWorldContactPoint
{
	// World-space contact point relative to each particle's center of mass
	FVector3f RelativeContactPoints[2];

	// World-space contact normal and tangents
	FVector3f ContactNormal;
	FVector3f ContactTangentU;
	FVector3f ContactTangentV;

	// Errors to correct along each of the contact axes
	float ContactDeltaNormal;
	float ContactDeltaTangentU;
	float ContactDeltaTangentV;

	// Target velocity along the normal direction
	float ContactTargetVelocityNormal;
};

export struct FPBDCollisionSolverManifoldPoint
{
	// World-space contact data
	FWorldContactPoint WorldContact;

	// I^-1.(R x A) for each body where A is each axis (Normal, TangentU, TangentV)
	FVector3f WorldContactNormalAngular0;
	FVector3f WorldContactTangentUAngular0;
	FVector3f WorldContactTangentVAngular0;
	FVector3f WorldContactNormalAngular1;
	FVector3f WorldContactTangentUAngular1;
	FVector3f WorldContactTangentVAngular1;

	// Contact mass (for non-friction)
	float ContactMassNormal;
	float ContactMassTangentU;
	float ContactMassTangentV;

	// Solver outputs
	float NetPushOutNormal;
	float NetPushOutTangentU;
	float NetPushOutTangentV;
	float NetImpulseNormal;
	float NetImpulseTangentU;
	float NetImpulseTangentV;

	// A measure of how much we exceeded the static friction threshold.
	// Equal to (NormalPushOut / TangentialPushOut) before clamping to the friction cone.
	// Used to move the static friction anchors to the edge of the cone in Scatter.
	float StaticFrictionRatio;
};

export struct FPBDCollisionSolver
{
	// Static Friction in the position-solve phase
	float StaticFriction;

	// Dynamic Friction in the position-solve phase
	float DynamicFriction;

	// Dynamic Friction in the velocity-solve phase
	float VelocityFriction;

	// Solver stiffness (scales all pushout and impulses)
	float Stiffness;

	// Bodies and contacts
	FConstraintSolverBody SolverBodies[2];
	FPBDCollisionSolverManifoldPoint* ManifoldPoints;
	int32 NumManifoldPoints;
	int32 MaxManifoldPoints;
};


//
//
// GENERAL FUNCTIONS
//
//


static inline uniform bool Body_IsDynamic(const uniform FSolverBody* uniform Body)
{ 
	return (Body->InvM > FLOAT_SMALL_NUMBER); 
}

static inline void Body_ApplyPositionDelta(uniform FSolverBody* uniform Body, const uniform FVector3f DP)
{
	Body->DP = Body->DP + DP;
}

static inline void Body_ApplyRotationDelta(uniform FSolverBody* uniform Body, const uniform FVector3f DQ)
{
	Body->DQ = Body->DQ + DQ;
}

static inline uniform float CalculatePositionErrorNormal(
	uniform FPBDCollisionSolverManifoldPoint* uniform SolverManifoldPoint, 
	const uniform FSolverBody* uniform Body0, 
	const uniform FSolverBody* uniform Body1, 
	const uniform float MaxPushOut)
{
	// Linear version: calculate the contact delta assuming linear motion after applying a positional impulse at the contact point. There will be an error that depends on the size of the rotation.
	const uniform FVector3f ContactDelta0 = Body0->DP + VectorCross(Body0->DQ, SolverManifoldPoint->WorldContact.RelativeContactPoints[0]);
	const uniform FVector3f ContactDelta1 = Body1->DP + VectorCross(Body1->DQ, SolverManifoldPoint->WorldContact.RelativeContactPoints[1]);
	const uniform FVector3f ContactDelta = ContactDelta0 - ContactDelta1;

	uniform float ContactDeltaNormal = SolverManifoldPoint->WorldContact.ContactDeltaNormal + VectorDot(ContactDelta, SolverManifoldPoint->WorldContact.ContactNormal);

	// NOTE: ContactDeltaNormal is negative for penetration
	// NOTE: MaxPushOut == 0 disables the pushout limits
	if ((MaxPushOut > 0) && (ContactDeltaNormal < -MaxPushOut))
	{
		ContactDeltaNormal = -MaxPushOut;
	}

	return ContactDeltaNormal;
}

static inline uniform float CalculatePositionCorrectionNormal(
	const uniform float Stiffness,
	const uniform float ContactDeltaNormal,
	const uniform float ContactMassNormal,
	const uniform float NetPushOutNormal)
{
	const uniform float PushOutNormal = -Stiffness * ContactDeltaNormal * ContactMassNormal;

	// The total pushout so far this sub-step
	// Unilateral constraint: Net-negative impulses not allowed (negative incremental impulses are allowed as long as the net is positive)
	return (NetPushOutNormal + PushOutNormal) > 0 ? PushOutNormal : -NetPushOutNormal;
}


//
//
// GAUSS SEIDEL SOLVER
//
//

static inline void CollisionSolverApplyPositionCorrectionNormal(
	uniform FPBDCollisionSolver* uniform Solver,
	const uniform float Stiffness,
	const uniform float ContactDeltaNormal,
	uniform FPBDCollisionSolverManifoldPoint* uniform ManifoldPoint,
	uniform FSolverBody* uniform Body0,
	uniform FSolverBody* uniform Body1)
{
	const uniform float PushOutNormal = CalculatePositionCorrectionNormal(Stiffness, ContactDeltaNormal, ManifoldPoint->ContactMassNormal, ManifoldPoint->NetPushOutNormal);

	ManifoldPoint->NetPushOutNormal = ManifoldPoint->NetPushOutNormal + PushOutNormal;

	// Update the particle state based on the pushout
	if (Body_IsDynamic(Body0))
	{
		const uniform FVector3f DX0 = ManifoldPoint->WorldContact.ContactNormal * (Body0->InvM * PushOutNormal);
		const uniform FVector3f DR0 = ManifoldPoint->WorldContactNormalAngular0 * PushOutNormal;
		Body_ApplyPositionDelta(Body0, DX0);
		Body_ApplyRotationDelta(Body0, DR0);
	}
	if (Body_IsDynamic(Body1))
	{
		const uniform FVector3f DX1 = ManifoldPoint->WorldContact.ContactNormal * (Body1->InvM * -PushOutNormal);
		const uniform FVector3f DR1 = ManifoldPoint->WorldContactNormalAngular1 * -PushOutNormal;
		Body_ApplyPositionDelta(Body1, DX1);
		Body_ApplyRotationDelta(Body1, DR1);
	}
}

static inline void CollisionSolverSolvePositionNoFriction(
	uniform FPBDCollisionSolver* uniform Solver, 
	const uniform float Dt, 
	const uniform float MaxPushOut)
{
	for (uniform int PointIndex = 0; PointIndex < Solver->NumManifoldPoints; ++PointIndex)
	{
		uniform FPBDCollisionSolverManifoldPoint* uniform SolverManifoldPoint = &Solver->ManifoldPoints[PointIndex];

		const uniform float ContactDeltaNormal = CalculatePositionErrorNormal(SolverManifoldPoint, Solver->SolverBodies[0].Body, Solver->SolverBodies[1].Body, MaxPushOut);

		const uniform bool bProcessManifoldPoint = (ContactDeltaNormal < 0) || (SolverManifoldPoint->NetPushOutNormal > FLOAT_SMALL_NUMBER);
		if (bProcessManifoldPoint)
		{
			CollisionSolverApplyPositionCorrectionNormal(Solver, Solver->Stiffness, ContactDeltaNormal, SolverManifoldPoint, Solver->SolverBodies[0].Body, Solver->SolverBodies[1].Body);
		}
	}
}

export void SolvePositionNoFriction(
	uniform FPBDCollisionSolver* uniform Solvers, 
	const uniform int NumSolvers, 
	const uniform float Dt, 
	const uniform float MaxPushOut)
{
	for (uniform int SolverIndex = 0; SolverIndex < NumSolvers; ++SolverIndex)
	{
		CollisionSolverSolvePositionNoFriction(&Solvers[SolverIndex], Dt, MaxPushOut);
	}
}


//
//
// PARTIAL-JACOBI SOLVER
//
//

static void CollisionSolver_SolvePositionNoFriction_Jacobi(
	uniform FPBDCollisionSolver* uniform Solver, 
	const uniform float Dt, 
	const uniform float MaxPushOut,
	const uniform float JacobiStiffness)
{
	uniform FSolverBody* uniform Body0 = Solver->SolverBodies[0].Body;
	uniform FSolverBody* uniform Body1 = Solver->SolverBodies[1].Body;

	uniform FVector3f DX0 = FloatZeroVector;
	uniform FVector3f DX1 = FloatZeroVector;
	uniform FVector3f DR0 = FloatZeroVector;
	uniform FVector3f DR1 = FloatZeroVector;

	const uniform float Stiffness = JacobiStiffness * Solver->Stiffness;

	for (uniform int PointIndex = 0; PointIndex < Solver->NumManifoldPoints; ++PointIndex)
	{
		uniform FPBDCollisionSolverManifoldPoint* uniform ManifoldPoint = &Solver->ManifoldPoints[PointIndex];

		const uniform float ContactDeltaNormal = CalculatePositionErrorNormal(ManifoldPoint, Solver->SolverBodies[0].Body, Solver->SolverBodies[1].Body, MaxPushOut);

		const uniform bool bProcessManifoldPoint = (ContactDeltaNormal < 0) || (ManifoldPoint->NetPushOutNormal > FLOAT_SMALL_NUMBER);
		if (bProcessManifoldPoint)
		{
			const uniform float PushOutNormal = CalculatePositionCorrectionNormal(Stiffness, ContactDeltaNormal, ManifoldPoint->ContactMassNormal, ManifoldPoint->NetPushOutNormal);

			ManifoldPoint->NetPushOutNormal = ManifoldPoint->NetPushOutNormal + PushOutNormal;

			if (Body_IsDynamic(Body0))
			{
				const uniform FVector3f DDX0 = ManifoldPoint->WorldContact.ContactNormal * (Body0->InvM * PushOutNormal);
				const uniform FVector3f DDR0 = ManifoldPoint->WorldContactNormalAngular0 * PushOutNormal;
				DX0 = DX0 + DDX0;
				DR0 = DR0 + DDR0;
			}
			if (Body_IsDynamic(Body1))
			{
				const uniform FVector3f DDX1 = ManifoldPoint->WorldContact.ContactNormal * (Body1->InvM * PushOutNormal);
				const uniform FVector3f DDR1 = ManifoldPoint->WorldContactNormalAngular1 * PushOutNormal;
				DX1 = DX1 - DDX1;
				DR1 = DR1 - DDR1;
			}
		}
	}

	if (Body_IsDynamic(Body0))
	{
		Body_ApplyPositionDelta(Body0, DX0);
		Body_ApplyRotationDelta(Body0, DR0);
	}
	if (Body_IsDynamic(Body1))
	{
		Body_ApplyPositionDelta(Body1, DX1);
		Body_ApplyRotationDelta(Body1, DR1);
	}
}


export void SolvePositionNoFrictionJacobi(
	uniform FPBDCollisionSolver* uniform Solvers, 
	const uniform int NumSolvers, 
	const uniform float Dt, 
	const uniform float MaxPushOut,
	const uniform float JacobiStiffness)
{
	for (uniform int SolverIndex = 0; SolverIndex < NumSolvers; ++SolverIndex)
	{
		CollisionSolver_SolvePositionNoFriction_Jacobi(&Solvers[SolverIndex], Dt, MaxPushOut, JacobiStiffness);
	}
}
