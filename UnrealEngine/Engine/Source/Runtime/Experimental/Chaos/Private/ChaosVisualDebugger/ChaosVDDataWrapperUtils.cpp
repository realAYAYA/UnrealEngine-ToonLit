// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVisualDebugger/ChaosVDDataWrapperUtils.h"

#if WITH_CHAOS_VISUAL_DEBUGGER

#include "Chaos/PBDJointConstraints.h"
#include "DataWrappers/ChaosVDJointDataWrappers.h"

#include "Chaos/Collision/ParticlePairMidPhase.h"
#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "Chaos/ParticleHandle.h"
#include "ChaosVisualDebugger/ChaosVDSerializedNameTable.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"

namespace Chaos::VisualDebugger::Utils
{
	template<typename InType,typename OutType, int32 Size, typename TransformT>
	void TransformStaticArray(const InType (&In)[Size], OutType (&Out)[Size], TransformT Trans)
	{
		for (int32 Index = 0; Index < Size; Index++)
		{
			Out[Index] = Invoke(Trans, In[Index]);
		}
	}

	inline FTransform ConvertToFTransform(const FRigidTransform3& InChaosTransform)
	{
		return InChaosTransform;
	}
}

void FChaosVDDataWrapperUtils::CopyManifoldPointsToDataWrapper(const Chaos::FManifoldPoint& InCopyFrom, FChaosVDManifoldPoint& OutCopyTo)
{
	OutCopyTo.bDisabled = InCopyFrom.Flags.bDisabled;
	OutCopyTo.bWasRestored = InCopyFrom.Flags.bWasRestored;
	OutCopyTo.bWasReplaced = InCopyFrom.Flags.bWasReplaced;
	OutCopyTo.bHasStaticFrictionAnchor = InCopyFrom.Flags.bHasStaticFrictionAnchor;
	OutCopyTo.TargetPhi = InCopyFrom.TargetPhi;
	OutCopyTo.InitialPhi = InCopyFrom.InitialPhi;

	Chaos::VisualDebugger::Utils::TransformStaticArray(InCopyFrom.ShapeAnchorPoints, OutCopyTo.ShapeAnchorPoints, &FChaosVDDataWrapperUtils::ConvertToFVector);
	Chaos::VisualDebugger::Utils::TransformStaticArray(InCopyFrom.InitialShapeContactPoints, OutCopyTo.InitialShapeContactPoints, &FChaosVDDataWrapperUtils::ConvertToFVector);
	Chaos::VisualDebugger::Utils::TransformStaticArray(InCopyFrom.ContactPoint.ShapeContactPoints, OutCopyTo.ContactPoint.ShapeContactPoints, &FChaosVDDataWrapperUtils::ConvertToFVector);

	OutCopyTo.ContactPoint.ShapeContactNormal = FVector(InCopyFrom.ContactPoint.ShapeContactNormal);
	OutCopyTo.ContactPoint.Phi = InCopyFrom.ContactPoint.Phi;
	OutCopyTo.ContactPoint.FaceIndex = InCopyFrom.ContactPoint.FaceIndex;
	OutCopyTo.ContactPoint.ContactType = static_cast<EChaosVDContactPointType>(InCopyFrom.ContactPoint.ContactType);
}

void FChaosVDDataWrapperUtils::CopyManifoldPointResultsToDataWrapper(const Chaos::FManifoldPointResult& InCopyFrom, FChaosVDManifoldPoint& OutCopyTo)
{
	OutCopyTo.NetPushOut =  FVector(InCopyFrom.NetPushOut);
	OutCopyTo.NetImpulse =  FVector(InCopyFrom.NetImpulse);
	OutCopyTo.bIsValid =  InCopyFrom.bIsValid;
	OutCopyTo.bInsideStaticFrictionCone =  InCopyFrom.bInsideStaticFrictionCone;
}

void FChaosVDDataWrapperUtils::CopyCollisionMaterialToDataWrapper(const Chaos::FPBDCollisionConstraintMaterial& InCopyFrom, FChaosVDCollisionMaterial& OutCopyTo)
{
	OutCopyTo.FaceIndex = InCopyFrom.FaceIndex;
	OutCopyTo.DynamicFriction = InCopyFrom.DynamicFriction;
	OutCopyTo.StaticFriction = InCopyFrom.StaticFriction;
	OutCopyTo.Restitution = InCopyFrom.Restitution;
	OutCopyTo.RestitutionThreshold = InCopyFrom.RestitutionThreshold;
	OutCopyTo.InvMassScale0 = InCopyFrom.InvMassScale0;
	OutCopyTo.InvMassScale1 = InCopyFrom.InvMassScale1;
	OutCopyTo.InvInertiaScale0 = InCopyFrom.InvInertiaScale0;
	OutCopyTo.InvInertiaScale1 = InCopyFrom.InvInertiaScale1;
}

FChaosVDParticleDataWrapper FChaosVDDataWrapperUtils::BuildParticleDataWrapperFromParticle(const Chaos::FGeometryParticleHandle* ParticleHandlePtr, const TSharedRef<Chaos::VisualDebugger::FChaosVDSerializableNameTable>& InNameTableInstance)
{
	check(ParticleHandlePtr);

	FChaosVDParticleDataWrapper WrappedParticleData;

	WrappedParticleData.ParticleIndex = ParticleHandlePtr->UniqueIdx().Idx;
	WrappedParticleData.Type =  static_cast<EChaosVDParticleType>(ParticleHandlePtr->Type);

#if CHAOS_DEBUG_NAME
	// Passing it as a Ptr because from here until it is serialized right after this function ends this string does not change
	// Passing it as a sharedptr has an additional 20% cost as it has to increment the reference counter, which adds up
	// TODO: We should switch to FName to take advantage of the new CVD Serializable name table so they can be de-duplicated, but to do so we need to change how we create our debug names to not be unique strings
	WrappedParticleData.DebugNamePtr = ParticleHandlePtr->DebugName().IsValid() ? ParticleHandlePtr->DebugName().Get() : nullptr;
#endif

	WrappedParticleData.ParticlePositionRotation.CopyFrom(Chaos::FConstGenericParticleHandle(ParticleHandlePtr));

	if (const Chaos::TKinematicGeometryParticleHandleImp<Chaos::FReal, 3, true>* KinematicParticle = ParticleHandlePtr->CastToKinematicParticle())
	{
		WrappedParticleData.ParticleVelocities.CopyFrom(*KinematicParticle);
	}

	if (const Chaos::TPBDRigidParticleHandleImp<Chaos::FReal, 3, true>* RigidParticle = ParticleHandlePtr->CastToRigidParticle())
	{
		WrappedParticleData.ParticleDynamics.CopyFrom(*RigidParticle);
		WrappedParticleData.ParticleDynamicsMisc.CopyFrom(*RigidParticle);
		WrappedParticleData.ParticleMassProps.CopyFrom(*RigidParticle);
	}

	if (const Chaos::TPBDRigidClusteredParticleHandleImp<Chaos::FReal, 3, true>* ClusteredParticle = ParticleHandlePtr->CastToClustered())
	{
		WrappedParticleData.ParticleCluster.CopyFrom(*ClusteredParticle);
	}

	WrappedParticleData.MarkAsValid();

	return MoveTemp(WrappedParticleData);
}

FChaosVDConstraint FChaosVDDataWrapperUtils::BuildConstraintDataWrapperFromConstraint(const Chaos::FPBDCollisionConstraint& InConstraint)
{
	FChaosVDConstraint WrappedConstraintData;
	
	WrappedConstraintData.bIsCurrent = InConstraint.Flags.bIsCurrent;
	WrappedConstraintData.bDisabled = InConstraint.Flags.bDisabled;
	WrappedConstraintData.bUseManifold = InConstraint.Flags.bUseManifold;
	WrappedConstraintData.bUseIncrementalManifold = InConstraint.Flags.bUseIncrementalManifold;
	WrappedConstraintData.bCanRestoreManifold = InConstraint.Flags.bCanRestoreManifold;
	WrappedConstraintData.bWasManifoldRestored = InConstraint.Flags.bWasManifoldRestored;
	WrappedConstraintData.bIsQuadratic0 = InConstraint.Flags.bIsQuadratic0;
	WrappedConstraintData.bIsQuadratic1 = InConstraint.Flags.bIsQuadratic1;
	WrappedConstraintData.bIsProbe = InConstraint.Flags.bIsProbe;
	WrappedConstraintData.bCCDEnabled = InConstraint.Flags.bCCDEnabled;
	WrappedConstraintData.bCCDSweepEnabled = InConstraint.Flags.bCCDSweepEnabled;
	WrappedConstraintData.bModifierApplied = InConstraint.Flags.bModifierApplied;
	WrappedConstraintData.bMaterialSet = InConstraint.Flags.bMaterialSet;
	WrappedConstraintData.ShapesType = static_cast<EChaosVDContactShapesType>(InConstraint.ShapesType);
	WrappedConstraintData.CullDistance = InConstraint.CullDistance;
	WrappedConstraintData.CollisionTolerance = InConstraint.CollisionTolerance;
	WrappedConstraintData.ClosestManifoldPointIndex = InConstraint.ClosestManifoldPointIndex;
	WrappedConstraintData.ExpectedNumManifoldPoints = InConstraint.ExpectedNumManifoldPoints;
	WrappedConstraintData.Stiffness = InConstraint.Stiffness;
	WrappedConstraintData.MinInitialPhi = InConstraint.MinInitialPhi;
	WrappedConstraintData.InitialOverlapDepenetrationVelocity = InConstraint.InitialOverlapDepenetrationVelocity;
	WrappedConstraintData.CCDTimeOfImpact = InConstraint.CCDTimeOfImpact;
	WrappedConstraintData.CCDEnablePenetration = InConstraint.CCDEnablePenetration;
	WrappedConstraintData.CCDTargetPenetration = InConstraint.CCDTargetPenetration;
	
	CopyCollisionMaterialToDataWrapper(InConstraint.Material, WrappedConstraintData.Material);

	WrappedConstraintData.AccumulatedImpulse = FVector(InConstraint.AccumulatedImpulse);

	WrappedConstraintData.Particle0Index = InConstraint.GetParticle0()->UniqueIdx().Idx;
	WrappedConstraintData.Particle1Index = InConstraint.GetParticle1()->UniqueIdx().Idx;


	Chaos::VisualDebugger::Utils::TransformStaticArray(InConstraint.ShapeWorldTransforms, WrappedConstraintData.ShapeWorldTransforms, &Chaos::VisualDebugger::Utils::ConvertToFTransform);
	Chaos::VisualDebugger::Utils::TransformStaticArray(InConstraint.ImplicitTransform, WrappedConstraintData.ImplicitTransforms, &Chaos::VisualDebugger::Utils::ConvertToFTransform);

	WrappedConstraintData.CollisionMargins = TArray(InConstraint.CollisionMargins, std::size(InConstraint.CollisionMargins));
	WrappedConstraintData.LastShapeWorldPositionDelta = FVector(InConstraint.LastShapeWorldPositionDelta);
	WrappedConstraintData.LastShapeWorldRotationDelta = FQuat(InConstraint.LastShapeWorldRotationDelta);

	const int32 MaxManifoldPoints = InConstraint.ManifoldPoints.Num();
	WrappedConstraintData.ManifoldPoints.Reserve(MaxManifoldPoints);
	WrappedConstraintData.ManifoldPoints.SetNum(MaxManifoldPoints);

	for (int32 PointIndex = 0; PointIndex < MaxManifoldPoints; PointIndex++)
	{
		FChaosVDManifoldPoint& CurrentCVDMainFoldPoint = WrappedConstraintData.ManifoldPoints[PointIndex];

		if (PointIndex < InConstraint.SavedManifoldPoints.Num())
		{
			const Chaos::FSavedManifoldPoint& CurrentChaosSavedManifoldPoint = InConstraint.SavedManifoldPoints[PointIndex];

			Chaos::VisualDebugger::Utils::TransformStaticArray(CurrentChaosSavedManifoldPoint.ShapeContactPoints, CurrentCVDMainFoldPoint.ShapeContactPoints, &FChaosVDDataWrapperUtils::ConvertToFVector);
		}

		if (PointIndex < InConstraint.ManifoldPoints.Num())
		{
			const Chaos::FManifoldPoint& CurrentChaosMainFoldPoint = InConstraint.ManifoldPoints[PointIndex];
			CopyManifoldPointsToDataWrapper(CurrentChaosMainFoldPoint, CurrentCVDMainFoldPoint);
		}
		

		if (PointIndex < InConstraint.ManifoldPointResults.Num())
		{
			const Chaos::FManifoldPointResult& CurrentChaosMainFoldPointResult = InConstraint.ManifoldPointResults[PointIndex];
			CopyManifoldPointResultsToDataWrapper(CurrentChaosMainFoldPointResult, CurrentCVDMainFoldPoint);
		}
	}

	return MoveTemp(WrappedConstraintData);
}

FChaosVDParticlePairMidPhase FChaosVDDataWrapperUtils::BuildMidPhaseDataWrapperFromMidPhase(const Chaos::FParticlePairMidPhase& InMidPhase)
{
	FChaosVDParticlePairMidPhase WrappedMidPhaseData;
	
	WrappedMidPhaseData.bIsActive = InMidPhase.Flags.bIsActive;
	WrappedMidPhaseData.bIsCCD = InMidPhase.Flags.bIsCCD;
	WrappedMidPhaseData.bIsCCDActive = InMidPhase.Flags.bIsCCDActive;
	WrappedMidPhaseData.bIsSleeping = InMidPhase.Flags.bIsSleeping;
	WrappedMidPhaseData.bIsModified = InMidPhase.Flags.bIsModified;
	WrappedMidPhaseData.LastUsedEpoch = InMidPhase.LastUsedEpoch;

	WrappedMidPhaseData.Particle0Idx = InMidPhase.Particle0->UniqueIdx().Idx;
	WrappedMidPhaseData.Particle1Idx = InMidPhase.Particle1->UniqueIdx().Idx;

	InMidPhase.VisitConstCollisions([&WrappedMidPhaseData](const Chaos::FPBDCollisionConstraint& Constraint)
	{
		FChaosVDConstraint WrappedConstraintData = FChaosVDDataWrapperUtils::BuildConstraintDataWrapperFromConstraint(Constraint);
		WrappedMidPhaseData.Constraints.Add(MoveTemp(WrappedConstraintData));
		return Chaos::ECollisionVisitorResult::Continue;
	}, Chaos::ECollisionVisitorFlags::VisitAllCurrent);

	return MoveTemp(WrappedMidPhaseData);
}

#ifndef CVD_COPY_FIELD_TO_WRAPPER
	#define CVD_COPY_FIELD_TO_WRAPPER(Source,Target, Field) \
	Target.Field = Source.Field;
#endif

#ifndef CVD_COPY_FIELD_TO_WRAPPER_WITH_CAST
	#define CVD_COPY_FIELD_TO_WRAPPER_WITH_CAST(Source,Target, Field, TargetType) \
	Target.Field = static_cast<TargetType>(Source.Field);
#endif

#ifndef CVD_COPY_VECTOR_FIELD_TO_WRAPPER
	#define CVD_COPY_VECTOR_FIELD_TO_WRAPPER(Source,Target, Field) \
	Target.Field = FChaosVDDataWrapperUtils::ConvertToFVector(Source.Field);
#endif

FChaosVDJointConstraint FChaosVDDataWrapperUtils::BuildJointDataWrapper(const Chaos::FPBDJointConstraintHandle* ConstaintHanlde)
{
	FChaosVDJointConstraint WrappedJointData;

	if (ConstaintHanlde)
	{
		WrappedJointData.ConstraintIndex = ConstaintHanlde->GetConstraintIndex();

		WrappedJointData.ParticleParIndexes[0] = ConstaintHanlde->GetConstrainedParticles()[0]->UniqueIdx().Idx;
		WrappedJointData.ParticleParIndexes[1] = ConstaintHanlde->GetConstrainedParticles()[1]->UniqueIdx().Idx;

		WrappedJointData.JointState.bBroken = ConstaintHanlde->IsConstraintBroken();
		WrappedJointData.JointState.bBreaking = ConstaintHanlde->IsConstraintBreaking();
		WrappedJointData.JointState.bDisabled = !ConstaintHanlde->IsConstraintEnabled();
		WrappedJointData.JointState.bDriveTargetChanged = ConstaintHanlde->IsDriveTargetChanged();

		//TODO: Island related data getters are deprecated. We need to see where is bet to get that data now and if this should be recorded as part of the CVD Constraint wrapper
		//WrappedJointData.JointState.Color = ConstaintHanlde->GetConstraintColor();
		//WrappedJointData.JointState.Island = ConstaintHanlde->GetConstraintIsland();
		//WrappedJointData.JointState.IslandSize = ConstaintHanlde->GetConstraintIsland();
		
		WrappedJointData.JointState.bEnabledDuringResim = ConstaintHanlde->IsEnabledDuringResim();
		WrappedJointData.JointState.AngularImpulse = ConstaintHanlde->GetAngularImpulse();
		WrappedJointData.JointState.LinearImpulse = ConstaintHanlde->GetLinearImpulse();

		switch(ConstaintHanlde->ResimType())
		{
			case Chaos::EResimType::FullResim:
				WrappedJointData.JointState.ResimType = EChaosVDJointReSimType::FullResim;
				break;
			case Chaos::EResimType::ResimAsFollower:
				WrappedJointData.JointState.ResimType = EChaosVDJointReSimType::ResimAsFollower;
				break;
		}

		switch(ConstaintHanlde->SyncState())
		{
			case Chaos::ESyncState::InSync:
				WrappedJointData.JointState.SyncState = EChaosVDJointSyncType::InSync;
				break;
			case Chaos::ESyncState::HardDesync:
				WrappedJointData.JointState.SyncState = EChaosVDJointSyncType::HardDesync;
				break;
		}

		WrappedJointData.JointState.MarkAsValid();

		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, Stiffness);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearProjection);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularProjection);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularProjection);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, TeleportDistance);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, TeleportAngle);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, ParentInvMassScale);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bCollisionEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bMassConditioningEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bSoftLinearLimitsEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bSoftTwistLimitsEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bAngularSLerpPositionDriveEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bAngularSLerpVelocityDriveEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bAngularTwistPositionDriveEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bAngularTwistVelocityDriveEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bAngularSwingPositionDriveEnabled);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, bAngularSwingVelocityDriveEnabled);

		WrappedJointData.JointSettings.LinearMotionTypes[0] = static_cast<EChaosVDJointMotionType>(ConstaintHanlde->GetJointSettings().LinearMotionTypes[0]);
		WrappedJointData.JointSettings.LinearMotionTypes[1] = static_cast<EChaosVDJointMotionType>(ConstaintHanlde->GetJointSettings().LinearMotionTypes[1]);
		WrappedJointData.JointSettings.LinearMotionTypes[2] = static_cast<EChaosVDJointMotionType>(ConstaintHanlde->GetJointSettings().LinearMotionTypes[2]);

		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearLimit);

		WrappedJointData.JointSettings.AngularMotionTypes[0] = static_cast<EChaosVDJointMotionType>(ConstaintHanlde->GetJointSettings().AngularMotionTypes[0]);
		WrappedJointData.JointSettings.AngularMotionTypes[1] = static_cast<EChaosVDJointMotionType>(ConstaintHanlde->GetJointSettings().AngularMotionTypes[1]);
		WrappedJointData.JointSettings.AngularMotionTypes[2] = static_cast<EChaosVDJointMotionType>(ConstaintHanlde->GetJointSettings().AngularMotionTypes[2]);

		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularLimits);
		CVD_COPY_FIELD_TO_WRAPPER_WITH_CAST(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearSoftForceMode, EChaosVDJointForceMode);
		CVD_COPY_FIELD_TO_WRAPPER_WITH_CAST(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularSoftForceMode, EChaosVDJointForceMode);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, SoftLinearStiffness);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, SoftLinearDamping);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, SoftTwistStiffness);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, SoftTwistDamping);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, SoftSwingStiffness);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, SoftSwingDamping);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearRestitution);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, TwistRestitution);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, TwistRestitution);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, SwingRestitution);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearContactDistance);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, TwistContactDistance);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, SwingContactDistance);

		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearDrivePositionTarget);
		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearDriveVelocityTarget);
		
		WrappedJointData.JointSettings.bLinearPositionDriveEnabled0 = ConstaintHanlde->GetJointSettings().bLinearPositionDriveEnabled[0];
		WrappedJointData.JointSettings.bLinearPositionDriveEnabled1 = ConstaintHanlde->GetJointSettings().bLinearPositionDriveEnabled[1];
		WrappedJointData.JointSettings.bLinearPositionDriveEnabled2 = ConstaintHanlde->GetJointSettings().bLinearPositionDriveEnabled[2];

		WrappedJointData.JointSettings.bLinearVelocityDriveEnabled0 = ConstaintHanlde->GetJointSettings().bLinearVelocityDriveEnabled[0];
		WrappedJointData.JointSettings.bLinearVelocityDriveEnabled1 = ConstaintHanlde->GetJointSettings().bLinearVelocityDriveEnabled[1];
		WrappedJointData.JointSettings.bLinearVelocityDriveEnabled2 = ConstaintHanlde->GetJointSettings().bLinearVelocityDriveEnabled[2];

		CVD_COPY_FIELD_TO_WRAPPER_WITH_CAST(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearDriveForceMode, EChaosVDJointForceMode);

		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearDriveStiffness);
		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearDriveDamping);
		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearDriveMaxForce);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularDrivePositionTarget);
		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularDriveVelocityTarget);
		CVD_COPY_FIELD_TO_WRAPPER_WITH_CAST(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularDriveForceMode, EChaosVDJointForceMode);
		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularDriveStiffness);
		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularDriveDamping);
		CVD_COPY_VECTOR_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularDriveMaxTorque);

		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearBreakForce);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearPlasticityLimit);

		CVD_COPY_FIELD_TO_WRAPPER_WITH_CAST(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearPlasticityType, EChaosVDPlasticityType);
		
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, LinearPlasticityInitialDistanceSquared);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularBreakTorque);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, AngularPlasticityLimit);
		CVD_COPY_FIELD_TO_WRAPPER(ConstaintHanlde->GetJointSettings(), WrappedJointData.JointSettings, ContactTransferScale);
		
		WrappedJointData.JointSettings.ConnectorTransforms[0] = ConstaintHanlde->GetJointSettings().ConnectorTransforms[0];
		WrappedJointData.JointSettings.ConnectorTransforms[1] = ConstaintHanlde->GetJointSettings().ConnectorTransforms[1];
	
		WrappedJointData.JointSettings.MarkAsValid();
		 
	}
	
	WrappedJointData.MarkAsValid();
	
	return MoveTemp(WrappedJointData);
}

void FChaosVDDataWrapperUtils::CopyShapeDataToWrapper(const Chaos::FShapeInstancePtr& ShapeDataPtr, FChaosVDShapeCollisionData& OutCopyTo)
{
	const Chaos::FCollisionData& CollisionData = ShapeDataPtr->GetCollisionData();

	OutCopyTo.bQueryCollision = CollisionData.bQueryCollision;
	OutCopyTo.bIsProbe = CollisionData.bIsProbe;
	OutCopyTo.bSimCollision = CollisionData.bSimCollision;
	OutCopyTo.CollisionTraceType = static_cast<EChaosVDCollisionTraceFlag>(CollisionData.CollisionTraceType);

	OutCopyTo.SimData.Word0 = CollisionData.SimData.Word0;
	OutCopyTo.SimData.Word1 = CollisionData.SimData.Word1;
	OutCopyTo.SimData.Word2 = CollisionData.SimData.Word2;
	OutCopyTo.SimData.Word3 = CollisionData.SimData.Word3;
	
	OutCopyTo.QueryData.Word0 = CollisionData.QueryData.Word0;
	OutCopyTo.QueryData.Word1 = CollisionData.QueryData.Word1;
	OutCopyTo.QueryData.Word2 = CollisionData.QueryData.Word2;
	OutCopyTo.QueryData.Word3 = CollisionData.QueryData.Word3;
}
#endif //WITH_CHAOS_VISUAL_DEBUGGER
