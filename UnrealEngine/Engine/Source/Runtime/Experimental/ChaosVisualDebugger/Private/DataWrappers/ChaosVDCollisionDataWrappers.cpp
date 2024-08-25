// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "DataWrappers/ChaosVDDataSerializationMacros.h"

bool FChaosVDContactPoint::Serialize(FArchive& Ar)
{
	CVD_SERIALIZE_STATIC_ARRAY(Ar, ShapeContactPoints);
	Ar << ShapeContactNormal;
	Ar << Phi;
	Ar << FaceIndex;
	Ar << ContactType;

	return true;
}

bool FChaosVDManifoldPoint::Serialize(FArchive& Ar)
{
	EChaosVDManifoldPointFlags PackedFlags = EChaosVDManifoldPointFlags::None;

	
	// Note: When the UI is done to show the enums in the same way we show these bitfiedls as read only booleans in CVD, we can remove all
	// this macro boilerplate and just serialize the the flags directly
	if (Ar.IsLoading())
	{
		Ar << PackedFlags;
		CVD_UNPACK_BITFIELD_DATA(bDisabled, PackedFlags, EChaosVDManifoldPointFlags::Disabled);
		CVD_UNPACK_BITFIELD_DATA(bWasRestored, PackedFlags, EChaosVDManifoldPointFlags::WasRestored);
		CVD_UNPACK_BITFIELD_DATA(bWasReplaced, PackedFlags, EChaosVDManifoldPointFlags::WasReplaced);
		CVD_UNPACK_BITFIELD_DATA(bHasStaticFrictionAnchor, PackedFlags, EChaosVDManifoldPointFlags::HasStaticFrictionAnchor);
		CVD_UNPACK_BITFIELD_DATA(bIsValid, PackedFlags, EChaosVDManifoldPointFlags::IsValid);
		CVD_UNPACK_BITFIELD_DATA(bInsideStaticFrictionCone, PackedFlags, EChaosVDManifoldPointFlags::InsideStaticFrictionCone);
	}
	else
	{
		CVD_PACK_BITFIELD_DATA(bDisabled, PackedFlags, EChaosVDManifoldPointFlags::Disabled);
		CVD_PACK_BITFIELD_DATA(bWasRestored, PackedFlags, EChaosVDManifoldPointFlags::WasRestored);
		CVD_PACK_BITFIELD_DATA(bWasReplaced, PackedFlags, EChaosVDManifoldPointFlags::WasReplaced);
		CVD_PACK_BITFIELD_DATA(bHasStaticFrictionAnchor, PackedFlags, EChaosVDManifoldPointFlags::HasStaticFrictionAnchor);
		CVD_PACK_BITFIELD_DATA(bIsValid, PackedFlags, EChaosVDManifoldPointFlags::IsValid);
		CVD_PACK_BITFIELD_DATA(bInsideStaticFrictionCone, PackedFlags, EChaosVDManifoldPointFlags::InsideStaticFrictionCone);
		Ar << PackedFlags;
	}

	Ar << NetPushOut;
	Ar << NetImpulse;

	Ar << TargetPhi;
	Ar << InitialPhi;

	CVD_SERIALIZE_STATIC_ARRAY(Ar, ShapeAnchorPoints);
	CVD_SERIALIZE_STATIC_ARRAY(Ar, InitialShapeContactPoints);

	Ar << ContactPoint;
	CVD_SERIALIZE_STATIC_ARRAY(Ar, ShapeContactPoints);

	return true;
}

bool FChaosVDCollisionMaterial::Serialize(FArchive& Ar)
{
	Ar << FaceIndex;
	Ar << MaterialDynamicFriction;
	Ar << MaterialStaticFriction;
	Ar << MaterialRestitution;
	Ar << DynamicFriction;
	Ar << StaticFriction;
	Ar << Restitution;
	Ar << RestitutionThreshold;
	Ar << InvMassScale0;
	Ar << InvMassScale1;
	Ar << InvInertiaScale0;
	Ar << InvInertiaScale1;

	return true;
}

bool FChaosVDConstraint::Serialize(FArchive& Ar)
{	
	EChaosVDConstraintFlags PackedFlags = EChaosVDConstraintFlags::None;

	
	// Note: When the UI is done to show the enums in the same way we show these bitfiedls as read only booleans in CVD, we can remove all
	// this macro boilerplate and just serialize the the flags directly
	if (Ar.IsLoading())
	{
		Ar << PackedFlags;
		CVD_UNPACK_BITFIELD_DATA(bIsCurrent, PackedFlags, EChaosVDConstraintFlags::IsCurrent);
		CVD_UNPACK_BITFIELD_DATA(bDisabled, PackedFlags, EChaosVDConstraintFlags::Disabled);
		CVD_UNPACK_BITFIELD_DATA(bUseManifold, PackedFlags, EChaosVDConstraintFlags::UseManifold);
		CVD_UNPACK_BITFIELD_DATA(bUseIncrementalManifold, PackedFlags, EChaosVDConstraintFlags::UseIncrementalManifold);
		CVD_UNPACK_BITFIELD_DATA(bCanRestoreManifold, PackedFlags, EChaosVDConstraintFlags::CanRestoreManifold);
		CVD_UNPACK_BITFIELD_DATA(bWasManifoldRestored, PackedFlags, EChaosVDConstraintFlags::WasManifoldRestored);
		CVD_UNPACK_BITFIELD_DATA(bIsQuadratic0, PackedFlags, EChaosVDConstraintFlags::IsQuadratic0);
		CVD_UNPACK_BITFIELD_DATA(bIsProbe, PackedFlags, EChaosVDConstraintFlags::IsProbe);
		CVD_UNPACK_BITFIELD_DATA(bCCDEnabled, PackedFlags, EChaosVDConstraintFlags::CCDEnabled);
		CVD_UNPACK_BITFIELD_DATA(bCCDSweepEnabled, PackedFlags, EChaosVDConstraintFlags::CCDSweepEnabled);
		CVD_UNPACK_BITFIELD_DATA(bModifierApplied, PackedFlags, EChaosVDConstraintFlags::ModifierApplied);
		CVD_UNPACK_BITFIELD_DATA(bMaterialSet, PackedFlags, EChaosVDConstraintFlags::MaterialSet);
	}
	else
	{
		CVD_PACK_BITFIELD_DATA(bIsCurrent, PackedFlags, EChaosVDConstraintFlags::IsCurrent);
		CVD_PACK_BITFIELD_DATA(bDisabled, PackedFlags, EChaosVDConstraintFlags::Disabled);
		CVD_PACK_BITFIELD_DATA(bUseManifold, PackedFlags, EChaosVDConstraintFlags::UseManifold);
		CVD_PACK_BITFIELD_DATA(bUseIncrementalManifold, PackedFlags, EChaosVDConstraintFlags::UseIncrementalManifold);
		CVD_PACK_BITFIELD_DATA(bCanRestoreManifold, PackedFlags, EChaosVDConstraintFlags::CanRestoreManifold);
		CVD_PACK_BITFIELD_DATA(bWasManifoldRestored, PackedFlags, EChaosVDConstraintFlags::WasManifoldRestored);
		CVD_PACK_BITFIELD_DATA(bIsQuadratic0, PackedFlags, EChaosVDConstraintFlags::IsQuadratic0);
		CVD_PACK_BITFIELD_DATA(bIsProbe, PackedFlags, EChaosVDConstraintFlags::IsProbe);
		CVD_PACK_BITFIELD_DATA(bCCDEnabled, PackedFlags, EChaosVDConstraintFlags::CCDEnabled);
		CVD_PACK_BITFIELD_DATA(bCCDSweepEnabled, PackedFlags, EChaosVDConstraintFlags::CCDSweepEnabled);
		CVD_PACK_BITFIELD_DATA(bModifierApplied, PackedFlags, EChaosVDConstraintFlags::ModifierApplied);
		CVD_PACK_BITFIELD_DATA(bMaterialSet, PackedFlags, EChaosVDConstraintFlags::MaterialSet);
		Ar << PackedFlags;
	}

	Ar << Material;
	Ar << AccumulatedImpulse;
	Ar << ShapesType;

	CVD_SERIALIZE_STATIC_ARRAY(Ar, ShapeWorldTransforms);
	CVD_SERIALIZE_STATIC_ARRAY(Ar, ImplicitTransforms);

	Ar << CullDistance;
	Ar << CollisionMargins;	
	Ar << CollisionTolerance;	
	Ar << ClosestManifoldPointIndex;	
	Ar << ExpectedNumManifoldPoints;	
	Ar << LastShapeWorldPositionDelta;
	Ar << LastShapeWorldRotationDelta;
	Ar << Stiffness;
	Ar << MinInitialPhi;
	Ar << InitialOverlapDepenetrationVelocity;
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

	
	EChaosVDMidPhaseFlags PackedFlags = EChaosVDMidPhaseFlags::None;

	
	// Note: When the UI is done to show the enums in the same way we show these bitfiedls as read only booleans in CVD, we can remove all
	// this macro boilerplate and just serialize the the flags directly
	if (Ar.IsLoading())
	{
		Ar << PackedFlags;
		CVD_UNPACK_BITFIELD_DATA(bIsActive, PackedFlags, EChaosVDMidPhaseFlags::IsActive);
		CVD_UNPACK_BITFIELD_DATA(bIsCCD, PackedFlags, EChaosVDMidPhaseFlags::IsCCD);
		CVD_UNPACK_BITFIELD_DATA(bIsCCDActive, PackedFlags, EChaosVDMidPhaseFlags::IsCCDActive);
		CVD_UNPACK_BITFIELD_DATA(bIsCCDActive, PackedFlags, EChaosVDMidPhaseFlags::IsCCDActive);
		CVD_UNPACK_BITFIELD_DATA(bIsSleeping, PackedFlags, EChaosVDMidPhaseFlags::IsSleeping);
		CVD_UNPACK_BITFIELD_DATA(bIsModified, PackedFlags, EChaosVDMidPhaseFlags::IsModified);
	}
	else
	{
		CVD_PACK_BITFIELD_DATA(bIsActive, PackedFlags, EChaosVDMidPhaseFlags::IsActive);
		CVD_PACK_BITFIELD_DATA(bIsCCD, PackedFlags, EChaosVDMidPhaseFlags::IsCCD);
		CVD_PACK_BITFIELD_DATA(bIsCCDActive, PackedFlags, EChaosVDMidPhaseFlags::IsCCDActive);
		CVD_PACK_BITFIELD_DATA(bIsCCDActive, PackedFlags, EChaosVDMidPhaseFlags::IsCCDActive);
		CVD_PACK_BITFIELD_DATA(bIsSleeping, PackedFlags, EChaosVDMidPhaseFlags::IsSleeping);
		CVD_PACK_BITFIELD_DATA(bIsModified, PackedFlags, EChaosVDMidPhaseFlags::IsModified);
		Ar << PackedFlags;
	}
		
	Ar << LastUsedEpoch;

	Ar << Particle0Idx;
	Ar << Particle1Idx;

	Ar << Constraints;

	return true;
}

bool FChaosVDCollisionFilterData::Serialize(FArchive& Ar)
{
	Ar << Word0;
	Ar << Word1;
	Ar << Word2;
	Ar << Word3;

	return !Ar.IsError();
}

bool FChaosVDShapeCollisionData::Serialize(FArchive& Ar)
{
	Ar << CollisionTraceType;

	EChaosVDCollisionShapeDataFlags PackedFlags = EChaosVDCollisionShapeDataFlags::None;

	
	// Note: When the UI is done to show the enums in the same way we show these bitfiedls as read only booleans in CVD, we can remove all
	// this macro boilerplate and just serialize the the flags directly
	if (Ar.IsLoading())
	{
		Ar << PackedFlags;
		CVD_UNPACK_BITFIELD_DATA(bSimCollision, PackedFlags, EChaosVDCollisionShapeDataFlags::SimCollision);
		CVD_UNPACK_BITFIELD_DATA(bQueryCollision, PackedFlags, EChaosVDCollisionShapeDataFlags::QueryCollision);
		CVD_UNPACK_BITFIELD_DATA(bIsProbe, PackedFlags, EChaosVDCollisionShapeDataFlags::IsProbe);
	}
	else
	{
		CVD_PACK_BITFIELD_DATA(bSimCollision, PackedFlags, EChaosVDCollisionShapeDataFlags::SimCollision);
		CVD_PACK_BITFIELD_DATA(bQueryCollision, PackedFlags, EChaosVDCollisionShapeDataFlags::QueryCollision);
		CVD_PACK_BITFIELD_DATA(bIsProbe, PackedFlags, EChaosVDCollisionShapeDataFlags::IsProbe);
		Ar << PackedFlags;
	}

	return true;
}

bool FChaosVDShapeCollisionData::operator==(const FChaosVDShapeCollisionData& Other) const
{
	return CollisionTraceType == Other.CollisionTraceType
			&& bSimCollision == Other.bSimCollision
			&& bQueryCollision == Other.bQueryCollision
			&& bIsProbe == Other.bIsProbe;
}
