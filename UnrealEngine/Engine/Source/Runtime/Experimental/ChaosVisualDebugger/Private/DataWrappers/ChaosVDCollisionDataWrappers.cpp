// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataWrappers/ChaosVDCollisionDataWrappers.h"

bool FChaosVDContactPoint::Serialize(FArchive& Ar)
{
	Ar << ShapeContactPoints;
	Ar << ShapeContactNormal;
	Ar << Phi;
	Ar << FaceIndex;
	Ar << ContactType;

	return true;
}

bool FChaosVDManifoldPoint::Serialize(FArchive& Ar)
{
	FArchive_Serialize_BitfieldBool(Ar, bDisabled);
	FArchive_Serialize_BitfieldBool(Ar, bWasRestored);
	FArchive_Serialize_BitfieldBool(Ar, bWasReplaced);

	FArchive_Serialize_BitfieldBool(Ar, bHasStaticFrictionAnchor);
	FArchive_Serialize_BitfieldBool(Ar, bIsValid);
	FArchive_Serialize_BitfieldBool(Ar, bInsideStaticFrictionCone);

	Ar << NetPushOut;
	Ar << NetImpulse;

	Ar << TargetPhi;
	Ar << ShapeAnchorPoints;
	Ar << InitialShapeContactPoints;
	Ar << ContactPoint;
	Ar << ShapeContactPoints;

	return true;
}

bool FChaosVDConstraint::Serialize(FArchive& Ar)
{
	FArchive_Serialize_BitfieldBool(Ar, bDisabled);
	FArchive_Serialize_BitfieldBool(Ar, bUseManifold);
	FArchive_Serialize_BitfieldBool(Ar, bUseIncrementalManifold);
	FArchive_Serialize_BitfieldBool(Ar, bCanRestoreManifold);
	FArchive_Serialize_BitfieldBool(Ar, bWasManifoldRestored);
	FArchive_Serialize_BitfieldBool(Ar, bIsQuadratic0);
	FArchive_Serialize_BitfieldBool(Ar, bIsQuadratic1);
	FArchive_Serialize_BitfieldBool(Ar, bIsProbeUnmodified);
	FArchive_Serialize_BitfieldBool(Ar, bIsProbe);
	FArchive_Serialize_BitfieldBool(Ar, bCCDEnabled);
	FArchive_Serialize_BitfieldBool(Ar, bCCDSweepEnabled);
	FArchive_Serialize_BitfieldBool(Ar, bModifierApplied);
	FArchive_Serialize_BitfieldBool(Ar, bMaterialSet);

	Ar << AccumulatedImpulse;
	Ar << ShapesType;
	Ar << ShapeWorldTransforms;
	Ar << ImplicitTransforms;
	Ar << CullDistance;
	Ar << CollisionMargins;	
	Ar << CollisionTolerance;	
	Ar << ClosestManifoldPointIndex;	
	Ar << ExpectedNumManifoldPoints;	
	Ar << LastShapeWorldPositionDelta;
	Ar << LastShapeWorldRotationDelta;
	Ar << Stiffness;
	Ar << CCDTimeOfImpact;
	Ar << CCDEnablePenetration;
	Ar << CCDTargetPenetration;
	Ar << ManifoldPoints;
	Ar << SolverID;
	Ar << Particle0Index;
	Ar << Particle1Index;

	return true;
}


bool FChaosVDParticlePairMidPhase::Serialize(FArchive& Ar)
{
	Ar << SolverID;
	
	FArchive_Serialize_BitfieldBool(Ar, bIsActive);
	FArchive_Serialize_BitfieldBool(Ar, bIsCCD);
	FArchive_Serialize_BitfieldBool(Ar, bIsCCDActive);
	FArchive_Serialize_BitfieldBool(Ar, bIsSleeping);
	FArchive_Serialize_BitfieldBool(Ar, bIsModified);
		
	Ar << LastUsedEpoch;

	Ar << Particle0Idx;
	Ar << Particle1Idx;

	Ar << Constraints;

	return true;
}
