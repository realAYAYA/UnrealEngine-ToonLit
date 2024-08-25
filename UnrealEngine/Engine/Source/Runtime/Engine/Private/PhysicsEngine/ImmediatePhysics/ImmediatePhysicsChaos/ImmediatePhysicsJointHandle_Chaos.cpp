// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsJointHandle_Chaos.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsActorHandle_Chaos.h"

#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/ChaosConstraintSettings.h"

#include "PhysicsEngine/ConstraintInstance.h"

//UE_DISABLE_OPTIMIZATION

static_assert((int32)Chaos::EJointMotionType::Free == (int32)EAngularConstraintMotion::ACM_Free, "Chaos::EJointMotionType and EAngularConstraintMotion mismatch");
static_assert((int32)Chaos::EJointMotionType::Limited == (int32)EAngularConstraintMotion::ACM_Limited, "Chaos::EJointMotionType and EAngularConstraintMotion mismatch");
static_assert((int32)Chaos::EJointMotionType::Locked == (int32)EAngularConstraintMotion::ACM_Locked, "Chaos::EJointMotionType and EAngularConstraintMotion mismatch");

// NOTE: Hard dependence on EJointAngularConstraintIndex - the following will break if we change the order (but can be easily fixed). See FJointHandle::FJointHandle
static_assert((int32)Chaos::EJointAngularConstraintIndex::Twist == 0, "Angular drive targets have hard dependency on constraint order");
static_assert((int32)Chaos::EJointAngularConstraintIndex::Swing1 == 2, "Angular drive targets have hard dependency on constraint order");

namespace ImmediatePhysics_Chaos
{

	void TransferJointSettings(FConstraintInstance* ConstraintInstance, Chaos::FPBDJointSettings& ConstraintSettings)
	{
		using namespace Chaos;

		const FConstraintProfileProperties& Profile = ConstraintInstance->ProfileInstance;

		ConstraintSettings.Stiffness = ConstraintSettings::JointStiffness();

		ConstraintSettings.LinearMotionTypes =
		{
			static_cast<EJointMotionType>(ConstraintInstance->GetLinearXMotion()),
			static_cast<EJointMotionType>(ConstraintInstance->GetLinearYMotion()),
			static_cast<EJointMotionType>(ConstraintInstance->GetLinearZMotion()),
		};
		ConstraintSettings.LinearLimit = ConstraintInstance->GetLinearLimit();

		ConstraintSettings.AngularMotionTypes[(int32)EJointAngularConstraintIndex::Twist] = static_cast<EJointMotionType>(ConstraintInstance->GetAngularTwistMotion());
		ConstraintSettings.AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing1] = static_cast<EJointMotionType>(ConstraintInstance->GetAngularSwing1Motion());
		ConstraintSettings.AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing2] = static_cast<EJointMotionType>(ConstraintInstance->GetAngularSwing2Motion());
		ConstraintSettings.AngularLimits[(int32)EJointAngularConstraintIndex::Twist] = FMath::DegreesToRadians(ConstraintInstance->GetAngularTwistLimit());
		ConstraintSettings.AngularLimits[(int32)EJointAngularConstraintIndex::Swing1] = FMath::DegreesToRadians(ConstraintInstance->GetAngularSwing1Limit());
		ConstraintSettings.AngularLimits[(int32)EJointAngularConstraintIndex::Swing2] = FMath::DegreesToRadians(ConstraintInstance->GetAngularSwing2Limit());

		ConstraintSettings.bProjectionEnabled = Profile.bEnableProjection;
		ConstraintSettings.bShockPropagationEnabled = Profile.bEnableShockPropagation;
		ConstraintSettings.bMassConditioningEnabled = Profile.bEnableMassConditioning;

		ConstraintSettings.LinearProjection = Profile.bEnableProjection ? Profile.ProjectionLinearAlpha : 0.0f;
		ConstraintSettings.AngularProjection = Profile.bEnableProjection ? Profile.ProjectionAngularAlpha : 0.0f;
		ConstraintSettings.TeleportDistance = Profile.bEnableProjection ? Profile.ProjectionLinearTolerance : -1.0f;
		ConstraintSettings.TeleportAngle = Profile.bEnableProjection ? FMath::DegreesToRadians(Profile.ProjectionAngularTolerance) : -1.0f;
		ConstraintSettings.ShockPropagation = Profile.bEnableShockPropagation ? Profile.ShockPropagationAlpha : 0.0f;
		ConstraintSettings.ParentInvMassScale = Profile.bParentDominates ? (FReal)0 : (FReal)1;

		ConstraintSettings.bSoftLinearLimitsEnabled = ConstraintInstance->GetIsSoftLinearLimit();
		ConstraintSettings.bSoftTwistLimitsEnabled = ConstraintInstance->GetIsSoftTwistLimit();
		ConstraintSettings.bSoftSwingLimitsEnabled = ConstraintInstance->GetIsSoftSwingLimit();
		ConstraintSettings.SoftLinearStiffness = Chaos::ConstraintSettings::SoftLinearStiffnessScale() * ConstraintInstance->GetSoftLinearLimitStiffness();
		ConstraintSettings.SoftLinearDamping = Chaos::ConstraintSettings::SoftLinearDampingScale() * ConstraintInstance->GetSoftLinearLimitDamping();
		ConstraintSettings.SoftTwistStiffness = Chaos::ConstraintSettings::SoftAngularStiffnessScale() * ConstraintInstance->GetSoftTwistLimitStiffness();
		ConstraintSettings.SoftTwistDamping = Chaos::ConstraintSettings::SoftAngularDampingScale() * ConstraintInstance->GetSoftTwistLimitDamping();
		ConstraintSettings.SoftSwingStiffness = Chaos::ConstraintSettings::SoftAngularStiffnessScale() * ConstraintInstance->GetSoftSwingLimitStiffness();
		ConstraintSettings.SoftSwingDamping = Chaos::ConstraintSettings::SoftAngularDampingScale() * ConstraintInstance->GetSoftSwingLimitDamping();
		ConstraintSettings.LinearSoftForceMode = (Chaos::ConstraintSettings::SoftLinearForceMode() == 0) ? EJointForceMode::Acceleration : EJointForceMode::Force;
		ConstraintSettings.AngularSoftForceMode = (Chaos::ConstraintSettings::SoftAngularForceMode() == 0) ? EJointForceMode::Acceleration : EJointForceMode::Force;

		if (!ConstraintSettings.bSoftLinearLimitsEnabled)
		{
			ConstraintSettings.LinearRestitution = Profile.LinearLimit.Restitution;
			ConstraintSettings.LinearContactDistance = Profile.LinearLimit.ContactDistance;
		}
		if (!ConstraintSettings.bSoftTwistLimitsEnabled)
		{
			ConstraintSettings.TwistRestitution = Profile.TwistLimit.Restitution;
			ConstraintSettings.TwistContactDistance = FMath::DegreesToRadians(Profile.TwistLimit.ContactDistance);
		}
		if (!ConstraintSettings.bSoftSwingLimitsEnabled)
		{
			ConstraintSettings.SwingRestitution = Profile.ConeLimit.Restitution;
			ConstraintSettings.SwingContactDistance = FMath::DegreesToRadians(Profile.ConeLimit.ContactDistance);
		}

		ConstraintSettings.LinearDrivePositionTarget = Profile.LinearDrive.PositionTarget;
		ConstraintSettings.LinearDriveVelocityTarget = Profile.LinearDrive.VelocityTarget;
		ConstraintSettings.bLinearPositionDriveEnabled[0] = Profile.LinearDrive.XDrive.bEnablePositionDrive;
		ConstraintSettings.bLinearPositionDriveEnabled[1] = Profile.LinearDrive.YDrive.bEnablePositionDrive;
		ConstraintSettings.bLinearPositionDriveEnabled[2] = Profile.LinearDrive.ZDrive.bEnablePositionDrive;
		ConstraintSettings.bLinearVelocityDriveEnabled[0] = Profile.LinearDrive.XDrive.bEnableVelocityDrive;
		ConstraintSettings.bLinearVelocityDriveEnabled[1] = Profile.LinearDrive.YDrive.bEnableVelocityDrive;
		ConstraintSettings.bLinearVelocityDriveEnabled[2] = Profile.LinearDrive.ZDrive.bEnableVelocityDrive;
		ConstraintSettings.LinearDriveStiffness = Chaos::ConstraintSettings::LinearDriveStiffnessScale() * Chaos::FVec3(Profile.LinearDrive.XDrive.Stiffness, Profile.LinearDrive.YDrive.Stiffness, Profile.LinearDrive.ZDrive.Stiffness);
		ConstraintSettings.LinearDriveDamping = Chaos::ConstraintSettings::LinearDriveDampingScale() * Chaos::FVec3(Profile.LinearDrive.XDrive.Damping, Profile.LinearDrive.YDrive.Damping, Profile.LinearDrive.ZDrive.Damping);
		ConstraintSettings.LinearDriveForceMode = EJointForceMode::Acceleration;

		ConstraintSettings.AngularDrivePositionTarget = FQuat(Profile.AngularDrive.OrientationTarget);
		ConstraintSettings.AngularDriveVelocityTarget = Profile.AngularDrive.AngularVelocityTarget * 2.0f * UE_PI; // Rev/s to Rad/s

		if (Profile.AngularDrive.AngularDriveMode == EAngularDriveMode::SLERP)
		{
			ConstraintSettings.AngularDriveStiffness = Chaos::ConstraintSettings::AngularDriveStiffnessScale() * FVec3(Profile.AngularDrive.SlerpDrive.Stiffness);
			ConstraintSettings.AngularDriveDamping = Chaos::ConstraintSettings::AngularDriveDampingScale() * FVec3(Profile.AngularDrive.SlerpDrive.Damping);
			ConstraintSettings.bAngularSLerpPositionDriveEnabled = Profile.AngularDrive.SlerpDrive.bEnablePositionDrive;
			ConstraintSettings.bAngularSLerpVelocityDriveEnabled = Profile.AngularDrive.SlerpDrive.bEnableVelocityDrive;
		}
		else
		{
			ConstraintSettings.AngularDriveStiffness = Chaos::ConstraintSettings::AngularDriveStiffnessScale() * FVec3(Profile.AngularDrive.TwistDrive.Stiffness, Profile.AngularDrive.SwingDrive.Stiffness, Profile.AngularDrive.SwingDrive.Stiffness);
			ConstraintSettings.AngularDriveDamping = Chaos::ConstraintSettings::AngularDriveDampingScale() * FVec3(Profile.AngularDrive.TwistDrive.Damping, Profile.AngularDrive.SwingDrive.Damping, Profile.AngularDrive.SwingDrive.Damping);
			ConstraintSettings.bAngularTwistPositionDriveEnabled = Profile.AngularDrive.TwistDrive.bEnablePositionDrive;
			ConstraintSettings.bAngularTwistVelocityDriveEnabled = Profile.AngularDrive.TwistDrive.bEnableVelocityDrive;
			ConstraintSettings.bAngularSwingPositionDriveEnabled = Profile.AngularDrive.SwingDrive.bEnablePositionDrive;
			ConstraintSettings.bAngularSwingVelocityDriveEnabled = Profile.AngularDrive.SwingDrive.bEnableVelocityDrive;
		}
		ConstraintSettings.AngularDriveForceMode = EJointForceMode::Acceleration;

		ConstraintSettings.LinearBreakForce = (Profile.bLinearBreakable) ? Chaos::ConstraintSettings::LinearBreakScale() * Profile.LinearBreakThreshold : FLT_MAX;
		ConstraintSettings.LinearPlasticityLimit = (Profile.bLinearPlasticity) ? FMath::Clamp((float)Profile.LinearPlasticityThreshold, 0.f, 1.f) : FLT_MAX;
		ConstraintSettings.AngularBreakTorque = (Profile.bAngularBreakable) ? Chaos::ConstraintSettings::AngularBreakScale() * Profile.AngularBreakThreshold : FLT_MAX;
		ConstraintSettings.AngularPlasticityLimit = (Profile.bAngularPlasticity) ? Profile.AngularPlasticityThreshold : FLT_MAX;

		ConstraintSettings.ContactTransferScale = 0.0f;


		// UE Disables Soft Limits when the Limit is less than some threshold. This is not necessary in Chaos but for now we also do it for parity's sake (See FLinearConstraint::UpdateLinearLimit_AssumesLocked).
		if (ConstraintSettings.LinearLimit < RB_MinSizeToLockDOF)
		{
			for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				if (ConstraintSettings.LinearMotionTypes[AxisIndex] == EJointMotionType::Limited)
				{
					ConstraintSettings.LinearMotionTypes[AxisIndex] = EJointMotionType::Locked;
				}
			}
		}

		// Disable Soft Limits when stiffness is extremely high
		//const FReal MaxLinearStiffness = 10000;
		//const FReal MaxAngularStiffness = 10000;
		//if (ConstraintSettings.bSoftLinearLimitsEnabled && ConstraintSettings.SoftLinearStiffness > MaxLinearStiffness)
		//{
		//	ConstraintSettings.bSoftLinearLimitsEnabled = false;
		//}
		//if (ConstraintSettings.bSoftTwistLimitsEnabled && ConstraintSettings.SoftTwistStiffness > MaxAngularStiffness)
		//{
		//	ConstraintSettings.bSoftTwistLimitsEnabled = false;
		//}
		//if (ConstraintSettings.bSoftSwingLimitsEnabled && ConstraintSettings.SoftSwingStiffness > MaxAngularStiffness)
		//{
		//	ConstraintSettings.bSoftSwingLimitsEnabled = false;
		//}
	}

	FJointHandle::FJointHandle(FChaosConstraintContainer* InConstraints, FConstraintInstance* ConstraintInstance, FActorHandle* Actor1, FActorHandle* Actor2)
		: ActorHandles({ Actor1, Actor2 })
		, Constraints(InConstraints)
	{
		using namespace Chaos;

		FPBDJointSettings ConstraintSettings;

		if (ConstraintInstance != nullptr)
		{
			// BodyInstance/PhysX has the constraint locations in actor-space, but we need them in Center-of-Mass space
			TransferJointSettings(ConstraintInstance, ConstraintSettings);
			FReal JointScale = ConstraintInstance->GetLastKnownScale();
			ConstraintSettings.ConnectorTransforms[0] = FParticleUtilities::ActorLocalToParticleLocal(FGenericParticleHandle(Actor1->GetParticle()), ConstraintInstance->GetRefFrame(EConstraintFrame::Frame1));
			ConstraintSettings.ConnectorTransforms[1] = FParticleUtilities::ActorLocalToParticleLocal(FGenericParticleHandle(Actor2->GetParticle()), ConstraintInstance->GetRefFrame(EConstraintFrame::Frame2));
			ConstraintSettings.ConnectorTransforms[0].ScaleTranslation(JointScale);
			ConstraintSettings.ConnectorTransforms[1].ScaleTranslation(JointScale);
		}
		else
		{
			// TEMP: all creation with null ConstraintIndex for PhAt handles
			ConstraintSettings.ConnectorTransforms[0] = Actor2->GetWorldTransform().GetRelativeTransform(Actor1->GetWorldTransform());
			ConstraintSettings.ConnectorTransforms[1] = FRigidTransform3();
			ConstraintSettings.LinearMotionTypes = { EJointMotionType::Limited, EJointMotionType::Limited, EJointMotionType::Limited };
			ConstraintSettings.LinearLimit = 0.1f;
			ConstraintSettings.SoftLinearStiffness = 500.0f;
			ConstraintSettings.SoftLinearDamping = 100.0f;
			ConstraintSettings.bSoftLinearLimitsEnabled = true;
			ConstraintSettings.LinearSoftForceMode = EJointForceMode::Acceleration;
			ConstraintSettings.LinearProjection = 0.0f;
			ConstraintSettings.AngularProjection = 0.0f;
			ConstraintSettings.TeleportDistance = -1.0f;
			ConstraintSettings.TeleportAngle = -1.0f;
		}

		ConstraintSettings.Sanitize();

		ConstraintHandle = Constraints->AddConstraint({ Actor1->ParticleHandle, Actor2->ParticleHandle }, ConstraintSettings);

		SetActorInertiaConditioningDirty();
	}

	FJointHandle::FJointHandle(FChaosConstraintContainer* InConstraints, const FPBDJointSettings& ConstraintSettings, FActorHandle* const Actor1, FActorHandle* const Actor2)
		: ActorHandles({ Actor1, Actor2 })
		, Constraints(InConstraints)
	{
		ConstraintHandle = Constraints->AddConstraint({ Actor1->ParticleHandle, Actor2->ParticleHandle }, ConstraintSettings);

		SetActorInertiaConditioningDirty();
	}

	FJointHandle::~FJointHandle()
	{
		ConstraintHandle->SetConstraintEnabled(false);
		ConstraintHandle->RemoveConstraint();
	}

	typename FJointHandle::FChaosConstraintHandle* FJointHandle::GetConstraint()
	{
		return ConstraintHandle;
	}
	
	const typename FJointHandle::FChaosConstraintHandle* FJointHandle::GetConstraint() const
	{
		return ConstraintHandle;
	}

	const Chaos::TVec2<FActorHandle*>& FJointHandle::GetActorHandles()
	{
		return ActorHandles;
	}

	const Chaos::TVec2<const FActorHandle*>& FJointHandle::GetActorHandles() const
	{
		return reinterpret_cast<const Chaos::TVec2<const FActorHandle*>&>(ActorHandles);
	}

	void FJointHandle::SetSoftLinearSettings(bool bLinearSoft, FReal LinearStiffness, FReal LinearDamping)
	{
		using namespace Chaos;
		FPBDJointSettings JointSettings = ConstraintHandle->GetSettings();
		JointSettings.bSoftLinearLimitsEnabled = bLinearSoft;
		JointSettings.SoftLinearStiffness = bLinearSoft ? LinearStiffness : 0.0f;
		JointSettings.SoftLinearDamping = bLinearSoft ? LinearDamping : 0.0f;
		ConstraintHandle->SetSettings(JointSettings);
	}

	void FJointHandle::SetActorInertiaConditioningDirty()
	{
		using namespace Chaos;

		if (ActorHandles[0]->ParticleHandle != nullptr)
		{
			FGenericParticleHandle(ActorHandles[0]->ParticleHandle)->SetInertiaConditioningDirty();
		}

		if (ActorHandles[1]->ParticleHandle != nullptr)
		{
			FGenericParticleHandle(ActorHandles[1]->ParticleHandle)->SetInertiaConditioningDirty();
		}
	}
}

