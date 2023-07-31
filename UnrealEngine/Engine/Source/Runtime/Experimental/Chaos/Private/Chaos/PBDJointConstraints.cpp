// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/ChaosDebugDraw.h"
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/Island/IslandManager.h"
#include "Chaos/Joint/ChaosJointLog.h"
#include "Chaos/Joint/ColoringGraph.h"
#include "Chaos/Joint/JointConstraintsCVars.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDJointConstraintUtilities.h"
#include "Chaos/Utilities.h"
#include "ChaosLog.h"
#include "ChaosStats.h"

#include "HAL/IConsoleManager.h"

namespace Chaos
{
	DECLARE_CYCLE_STAT(TEXT("Joints::Sort"), STAT_Joints_Sort, STATGROUP_ChaosJoint);
	DECLARE_CYCLE_STAT(TEXT("Joints::PrepareTick"), STAT_Joints_PrepareTick, STATGROUP_ChaosJoint);
	DECLARE_CYCLE_STAT(TEXT("Joints::UnprepareTick"), STAT_Joints_UnprepareTick, STATGROUP_ChaosJoint);
	DECLARE_CYCLE_STAT(TEXT("Joints::Gather"), STAT_Joints_Gather, STATGROUP_ChaosJoint);
	DECLARE_CYCLE_STAT(TEXT("Joints::Scatter"), STAT_Joints_Scatter, STATGROUP_ChaosJoint);
	DECLARE_CYCLE_STAT(TEXT("Joints::Apply"), STAT_Joints_Apply, STATGROUP_ChaosJoint);
	DECLARE_CYCLE_STAT(TEXT("Joints::ApplyPushOut"), STAT_Joints_ApplyPushOut, STATGROUP_ChaosJoint);
	DECLARE_CYCLE_STAT(TEXT("Joints::ApplyProjection"), STAT_Joints_ApplyProjection, STATGROUP_ChaosJoint);


	//
	// Constraint Handle
	//

	
	FPBDJointConstraintHandle::FPBDJointConstraintHandle()
		: bLinearPlasticityInitialized(false)
		, bAngularPlasticityInitialized(false)
	{
	}

	
	FPBDJointConstraintHandle::FPBDJointConstraintHandle(FConstraintContainer* InConstraintContainer, int32 InConstraintIndex)
		: TIndexedContainerConstraintHandle<FPBDJointConstraints>(InConstraintContainer, InConstraintIndex)
		, bLinearPlasticityInitialized(false)
		, bAngularPlasticityInitialized(false)

	{
	}

	
	void FPBDJointConstraintHandle::CalculateConstraintSpace(FVec3& OutXa, FMatrix33& OutRa, FVec3& OutXb, FMatrix33& OutRb) const
	{
		ConcreteContainer()->CalculateConstraintSpace(ConstraintIndex, OutXa, OutRa, OutXb, OutRb);
	}


	int32 FPBDJointConstraintHandle::GetConstraintIsland() const
	{
		return ConcreteContainer()->GetConstraintIsland(ConstraintIndex);
	}


	int32 FPBDJointConstraintHandle::GetConstraintLevel() const
	{
		return ConcreteContainer()->GetConstraintLevel(ConstraintIndex);
	}


	int32 FPBDJointConstraintHandle::GetConstraintColor() const
	{
		return ConcreteContainer()->GetConstraintColor(ConstraintIndex);
	}

	bool FPBDJointConstraintHandle::IsConstraintBroken() const
	{
		return ConcreteContainer()->IsConstraintBroken(ConstraintIndex);
	}

	bool FPBDJointConstraintHandle::IsConstraintBreaking() const
	{
		return ConcreteContainer()->IsConstraintBreaking(ConstraintIndex);
	}

	void FPBDJointConstraintHandle::ClearConstraintBreaking()
	{
		return ConcreteContainer()->ClearConstraintBreaking(ConstraintIndex);
	}

	bool FPBDJointConstraintHandle::IsDriveTargetChanged() const
	{
		return ConcreteContainer()->IsDriveTargetChanged(ConstraintIndex);
	}

	void FPBDJointConstraintHandle::ClearDriveTargetChanged()
	{
		return ConcreteContainer()->ClearDriveTargetChanged(ConstraintIndex);
	}

	bool FPBDJointConstraintHandle::IsConstraintEnabled() const
	{
		return ConcreteContainer()->IsConstraintEnabled(ConstraintIndex);
	}

	FVec3 FPBDJointConstraintHandle::GetLinearImpulse() const
	{
		return ConcreteContainer()->GetConstraintLinearImpulse(ConstraintIndex);
	}

	FVec3 FPBDJointConstraintHandle::GetAngularImpulse() const
	{
		return ConcreteContainer()->GetConstraintAngularImpulse(ConstraintIndex);
	}

	ESyncState FPBDJointConstraintHandle::SyncState() const
	{
		return ConcreteContainer()->GetConstraintSyncState(ConstraintIndex);
	}

	void FPBDJointConstraintHandle::SetSyncState(ESyncState SyncState)
	{
		return ConcreteContainer()->SetConstraintSyncState(ConstraintIndex, SyncState);
	}

	void FPBDJointConstraintHandle::SetEnabledDuringResim(bool bEnabled)
	{
		return ConcreteContainer()->SetConstraintEnabledDuringResim(ConstraintIndex, bEnabled);
	}

	EResimType FPBDJointConstraintHandle::ResimType() const
	{
		return ConcreteContainer()->GetConstraintResimType(ConstraintIndex);
	}

	const FPBDJointSettings& FPBDJointConstraintHandle::GetSettings() const
	{
		return ConcreteContainer()->GetConstraintSettings(ConstraintIndex);
	}

	void FPBDJointConstraintHandle::SetSettings(const FPBDJointSettings& InSettings)
	{
		// buffer the previous targets so plasticiy can reuse them
		FVec3 LinearTarget = GetSettings().LinearDrivePositionTarget;
		FRotation3 AngularTarget = GetSettings().AngularDrivePositionTarget;
		if (!bLinearPlasticityInitialized && !FMath::IsNearlyEqual(InSettings.LinearPlasticityLimit, FLT_MAX))
		{
			bLinearPlasticityInitialized = true;
		}
		if (!bAngularPlasticityInitialized && !FMath::IsNearlyEqual(InSettings.AngularPlasticityLimit, FLT_MAX))
		{
			bAngularPlasticityInitialized = true;
		}


		ConcreteContainer()->SetConstraintSettings(ConstraintIndex, InSettings);


		// transfer the previous targets when controlled by plasticiy
		if (bLinearPlasticityInitialized)
		{
			ConcreteContainer()->SetLinearDrivePositionTarget(ConstraintIndex,LinearTarget);
		}
		if (bAngularPlasticityInitialized)
		{
			ConcreteContainer()->SetAngularDrivePositionTarget(ConstraintIndex,AngularTarget);
		}
	}

	FParticlePair FPBDJointConstraintHandle::GetConstrainedParticles() const
	{ 
		return ConcreteContainer()->GetConstrainedParticles(ConstraintIndex);
	}

	void FPBDJointConstraintHandle::SetConstraintEnabled(bool bInEnabled)
	{
		return ConcreteContainer()->SetConstraintEnabled(ConstraintIndex, bInEnabled);
	}


	//
	// Constraint Settings
	//

	
	FPBDJointSettings::FPBDJointSettings()
		: Stiffness(1)
		, LinearProjection(0)
		, AngularProjection(0)
		, ShockPropagation(0)
		, TeleportDistance(0)
		, TeleportAngle(0)
		, ParentInvMassScale(1)
		, bCollisionEnabled(true)
		, bProjectionEnabled(false)
		, bShockPropagationEnabled(false)
		, LinearMotionTypes({ EJointMotionType::Locked, EJointMotionType::Locked, EJointMotionType::Locked })
		, LinearLimit(FLT_MAX)
		, AngularMotionTypes({ EJointMotionType::Free, EJointMotionType::Free, EJointMotionType::Free })
		, AngularLimits(FVec3(FLT_MAX, FLT_MAX, FLT_MAX))
		, bSoftLinearLimitsEnabled(false)
		, bSoftTwistLimitsEnabled(false)
		, bSoftSwingLimitsEnabled(false)
		, LinearSoftForceMode(EJointForceMode::Acceleration)
		, AngularSoftForceMode(EJointForceMode::Acceleration)
		, SoftLinearStiffness(0)
		, SoftLinearDamping(0)
		, SoftTwistStiffness(0)
		, SoftTwistDamping(0)
		, SoftSwingStiffness(0)
		, SoftSwingDamping(0)
		, LinearRestitution(0)
		, TwistRestitution(0)
		, SwingRestitution(0)
		, LinearContactDistance(0)
		, TwistContactDistance(0)
		, SwingContactDistance(0)
		, LinearDrivePositionTarget(FVec3(0, 0, 0))
		, LinearDriveVelocityTarget(FVec3(0, 0, 0))
		, bLinearPositionDriveEnabled(TVector<bool, 3>(false, false, false))
		, bLinearVelocityDriveEnabled(TVector<bool, 3>(false, false, false))
		, LinearDriveForceMode(EJointForceMode::Acceleration)
		, LinearDriveStiffness(FVec3(0))
		, LinearDriveDamping(FVec3(0))
		, AngularDrivePositionTarget(FRotation3::FromIdentity())
		, AngularDriveVelocityTarget(FVec3(0, 0, 0))
		, bAngularSLerpPositionDriveEnabled(false)
		, bAngularSLerpVelocityDriveEnabled(false)
		, bAngularTwistPositionDriveEnabled(false)
		, bAngularTwistVelocityDriveEnabled(false)
		, bAngularSwingPositionDriveEnabled(false)
		, bAngularSwingVelocityDriveEnabled(false)
		, AngularDriveForceMode(EJointForceMode::Acceleration)
		, AngularDriveStiffness(FVec3(0))
		, AngularDriveDamping(FVec3(0))
		, LinearBreakForce(FLT_MAX)
		, LinearPlasticityLimit(FLT_MAX)
		, LinearPlasticityType(EPlasticityType::Free)
		, LinearPlasticityInitialDistanceSquared(FLT_MAX)
		, AngularBreakTorque(FLT_MAX)
		, AngularPlasticityLimit(FLT_MAX)
		, ContactTransferScale(0.f)
		, UserData(nullptr)
	{
	}


	void FPBDJointSettings::Sanitize()
	{
		// Disable soft joints for locked dofs
		if ((LinearMotionTypes[0] == EJointMotionType::Locked) && (LinearMotionTypes[1] == EJointMotionType::Locked) && (LinearMotionTypes[2] == EJointMotionType::Locked))
		{
			bSoftLinearLimitsEnabled = false;
		}
		if (AngularMotionTypes[(int32)EJointAngularConstraintIndex::Twist] == EJointMotionType::Locked)
		{
			bSoftTwistLimitsEnabled = false;
		}
		if ((AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing1] == EJointMotionType::Locked) && (AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing2] == EJointMotionType::Locked))
		{
			bSoftSwingLimitsEnabled = false;
		}

		// Reset limits if they won't be used (means we don't have to check if limited/locked in a few cases).
		// A side effect: if we enable a constraint, we need to reset the value of the limit.
		if ((LinearMotionTypes[0] != EJointMotionType::Limited) && (LinearMotionTypes[1] != EJointMotionType::Limited) && (LinearMotionTypes[2] != EJointMotionType::Limited))
		{
			LinearLimit = 0;
		}
		if (AngularMotionTypes[(int32)EJointAngularConstraintIndex::Twist] != EJointMotionType::Limited)
		{
			AngularLimits[(int32)EJointAngularConstraintIndex::Twist] = 0;
		}
		if (AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing1] != EJointMotionType::Limited)
		{
			AngularLimits[(int32)EJointAngularConstraintIndex::Swing1] = 0;
		}
		if (AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing2] != EJointMotionType::Limited)
		{
			AngularLimits[(int32)EJointAngularConstraintIndex::Swing2] = 0;
		}

		// If we have a zero degree limit angle, lock the joint, or set a non-zero limit (to avoid division by zero in axis calculations)
		const FReal MinAngularLimit = 0.01f;
		if ((AngularMotionTypes[(int32)EJointAngularConstraintIndex::Twist] == EJointMotionType::Limited) && (AngularLimits[(int32)EJointAngularConstraintIndex::Twist] < MinAngularLimit))
		{
			if (bSoftTwistLimitsEnabled)
			{
				AngularLimits[(int32)EJointAngularConstraintIndex::Twist] = MinAngularLimit;
			}
			else
			{
				AngularMotionTypes[(int32)EJointAngularConstraintIndex::Twist] = EJointMotionType::Locked;
			}
		}
		if ((AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing1] == EJointMotionType::Limited) && (AngularLimits[(int32)EJointAngularConstraintIndex::Swing1] < MinAngularLimit))
		{
			if (bSoftSwingLimitsEnabled)
			{
				AngularLimits[(int32)EJointAngularConstraintIndex::Swing1] = MinAngularLimit;
			}
			else
			{
				AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing1] = EJointMotionType::Locked;
			}
		}
		if ((AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing2] == EJointMotionType::Limited) && (AngularLimits[(int32)EJointAngularConstraintIndex::Swing2] < MinAngularLimit))
		{
			if (bSoftSwingLimitsEnabled)
			{
				AngularLimits[(int32)EJointAngularConstraintIndex::Swing2] = MinAngularLimit;
			}
			else
			{
				AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing2] = EJointMotionType::Locked;
			}
		}

		// SLerp drive is only allowed if no angular dofs are locked
		if (bAngularSLerpPositionDriveEnabled || bAngularSLerpVelocityDriveEnabled)
		{
			if ((AngularMotionTypes[(int32)EJointAngularConstraintIndex::Twist] == EJointMotionType::Locked)
				|| (AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing1] == EJointMotionType::Locked)
				|| (AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing2] == EJointMotionType::Locked))
			{
				bAngularSLerpPositionDriveEnabled = false;
				bAngularSLerpVelocityDriveEnabled = false;
			}
		}
	}

	
	FPBDJointState::FPBDJointState()
		: Island(INDEX_NONE)
		, Level(INDEX_NONE)
		, Color(INDEX_NONE)
		, IslandSize(0)
		, bDisabled(false)
		, bBroken(false)
		, bBreaking(false)
		, bDriveTargetChanged(false), LinearImpulse(FVec3(0))
		, AngularImpulse(FVec3(0))
	{
	}


	//
	// Solver Settings
	//

	
	FPBDJointSolverSettings::FPBDJointSolverSettings()
		: SwingTwistAngleTolerance(1.0e-6f)
		, PositionTolerance(0)
		, AngleTolerance(0)
		, MinParentMassRatio(0)
		, MaxInertiaRatio(0)
		, MinSolverStiffness(1)
		, MaxSolverStiffness(1)
		, NumIterationsAtMaxSolverStiffness(1)
		, NumShockPropagationIterations(0)
		, bUseLinearSolver(true)
		, bSolvePositionLast(true)
		, bEnableTwistLimits(true)
		, bEnableSwingLimits(true)
		, bEnableDrives(true)
		, LinearStiffnessOverride(-1)
		, TwistStiffnessOverride(-1)
		, SwingStiffnessOverride(-1)
		, LinearProjectionOverride(-1)
		, AngularProjectionOverride(-1)
		, ShockPropagationOverride(-1)
		, LinearDriveStiffnessOverride(-1)
		, LinearDriveDampingOverride(-1)
		, AngularDriveStiffnessOverride(-1)
		, AngularDriveDampingOverride(-1)
		, SoftLinearStiffnessOverride(-1)
		, SoftLinearDampingOverride(-1)
		, SoftTwistStiffnessOverride(-1)
		, SoftTwistDampingOverride(-1)
		, SoftSwingStiffnessOverride(-1)
		, SoftSwingDampingOverride(-1)
	{
	}


	//
	// Constraint Container
	//

	
	FPBDJointConstraints::FPBDJointConstraints()
		: Base(FConstraintContainerHandle::StaticType())
		, Settings()
		, bJointsDirty(false)
	{
	}

	
	FPBDJointConstraints::~FPBDJointConstraints()
	{
	}

	
	const FPBDJointSolverSettings& FPBDJointConstraints::GetSettings() const
	{
		return Settings;
	}

	
	void FPBDJointConstraints::SetSettings(const FPBDJointSolverSettings& InSettings)
	{
		Settings = InSettings;
	}


	int32 FPBDJointConstraints::NumConstraints() const
	{
		return ConstraintParticles.Num();
	}

	void FPBDJointConstraints::GetConstrainedParticleIndices(const int32 ConstraintIndex, int32& Index0, int32& Index1) const
	{
		// In solvers we need Particle0 to be the parent particle but ConstraintInstance has Particle1 as the parent, so by default
		// we need to flip the indices before we pass them to the solver. 

		Index0 = 1;
		Index1 = 0;
	}

	typename FPBDJointConstraints::FConstraintContainerHandle* FPBDJointConstraints::AddConstraint(const FParticlePair& InConstrainedParticles, const FRigidTransform3& WorldConstraintFrame)
	{
		FPBDJointSettings JointSettings;
		JointSettings.ConnectorTransforms[0] = FRigidTransform3(
			WorldConstraintFrame.GetTranslation() - InConstrainedParticles[0]->X(),
			WorldConstraintFrame.GetRotation() * InConstrainedParticles[0]->R().Inverse()
			);
		JointSettings.ConnectorTransforms[1] = FRigidTransform3(
			WorldConstraintFrame.GetTranslation() - InConstrainedParticles[1]->X(),
			WorldConstraintFrame.GetRotation() * InConstrainedParticles[1]->R().Inverse()
			);
		return AddConstraint(InConstrainedParticles, JointSettings);
	}

	
	typename FPBDJointConstraints::FConstraintContainerHandle* FPBDJointConstraints::AddConstraint(const FParticlePair& InConstrainedParticles, const FTransformPair& InConnectorTransforms)
	{
		FPBDJointSettings JointSettings;
		JointSettings.ConnectorTransforms = InConnectorTransforms;
		return AddConstraint(InConstrainedParticles, JointSettings);
	}

	
	typename FPBDJointConstraints::FConstraintContainerHandle* FPBDJointConstraints::AddConstraint(const FParticlePair& InConstrainedParticles, const FPBDJointSettings& InConstraintSettings)
	{
		bJointsDirty = true;

		int ConstraintIndex = Handles.Num();
		Handles.Add(HandleAllocator.AllocHandle(this, ConstraintIndex));
		ConstraintParticles.Add(InConstrainedParticles);
		ConstraintStates.Add(FPBDJointState());

		ConstraintSettings.AddDefaulted();
		SetConstraintSettings(ConstraintIndex, InConstraintSettings);

		// If our particle(s) are disabled, so is the constraint for now. It will get enabled if both particles get enabled.
		const bool bStartDisabled = FConstGenericParticleHandle(InConstrainedParticles[0])->Disabled() || FConstGenericParticleHandle(InConstrainedParticles[1])->Disabled();
		ConstraintStates[ConstraintIndex].bDisabled = bStartDisabled;

		return Handles.Last();
	}

	
	void FPBDJointConstraints::RemoveConstraint(int ConstraintIndex)
	{
		bJointsDirty = true;

		FConstraintContainerHandle* ConstraintHandle = Handles[ConstraintIndex];
		if (ConstraintHandle != nullptr)
		{
			if (ConstraintParticles[ConstraintIndex][0] != nullptr)
			{
				ConstraintParticles[ConstraintIndex][0]->RemoveConstraintHandle(ConstraintHandle);
			}
			if (ConstraintParticles[ConstraintIndex][1] != nullptr)
			{
				ConstraintParticles[ConstraintIndex][1]->RemoveConstraintHandle(ConstraintHandle);
			}

			// Release the handle for the freed constraint
			HandleAllocator.FreeHandle(ConstraintHandle);
			Handles[ConstraintIndex] = nullptr;
		}

		// Swap the last constraint into the gap to keep the array packed
		ConstraintParticles.RemoveAtSwap(ConstraintIndex);
		ConstraintSettings.RemoveAtSwap(ConstraintIndex);
		ConstraintStates.RemoveAtSwap(ConstraintIndex);
		Handles.RemoveAtSwap(ConstraintIndex);

		// Update the handle for the constraint that was moved
		if (ConstraintIndex < Handles.Num())
		{
			SetConstraintIndex(Handles[ConstraintIndex], ConstraintIndex);
		}
	}

	
	void FPBDJointConstraints::DisconnectConstraints(const TSet<TGeometryParticleHandle<FReal, 3>*>& RemovedParticles)
	{
		for (TGeometryParticleHandle<FReal, 3>*RemovedParticle : RemovedParticles)
		{
			for (FConstraintHandle* ConstraintHandle : RemovedParticle->ParticleConstraints())
			{
				if (FPBDJointConstraintHandle* JointHandle = ConstraintHandle->As<FPBDJointConstraintHandle>())
				{
					JointHandle->SetEnabled(false); // constraint lifespan is managed by the proxy

					int ConstraintIndex = JointHandle->GetConstraintIndex();
					if (ConstraintIndex != INDEX_NONE)
					{
						if (ConstraintParticles[ConstraintIndex][0] == RemovedParticle)
						{
							ConstraintParticles[ConstraintIndex][0] = nullptr;
						}
						if (ConstraintParticles[ConstraintIndex][1] == RemovedParticle)
						{
							ConstraintParticles[ConstraintIndex][1] = nullptr;
						}
					}
				}
			}

			RemovedParticle->ParticleConstraints().Reset();
		}
	}


	void FPBDJointConstraints::SortConstraints()
	{
		// Sort constraints so that constraints with lower level (closer to a kinematic joint) are first
		// @todo(ccaulfield): should probably also take islands/particle order into account
		// @todo(ccaulfield): optimize (though isn't called very often)
		SCOPE_CYCLE_COUNTER(STAT_Joints_Sort);

		FHandles SortedHandles = Handles;
		SortedHandles.StableSort([](const FConstraintContainerHandle& L, const FConstraintContainerHandle& R)
			{
				if (L.GetConstraintIsland() != R.GetConstraintIsland())
				{
					return L.GetConstraintIsland() < R.GetConstraintIsland();
				}
				else if (L.GetConstraintLevel() != R.GetConstraintLevel())
				{
					return L.GetConstraintLevel() < R.GetConstraintLevel();
				}
				return L.GetConstraintColor() < R.GetConstraintColor();
			});

		TArray<FPBDJointSettings> SortedConstraintSettings;
		TArray<FParticlePair> SortedConstraintParticles;
		TArray<FPBDJointState> SortedConstraintStates;
		SortedConstraintSettings.Reserve(SortedHandles.Num());
		SortedConstraintParticles.Reserve(SortedHandles.Num());
		SortedConstraintStates.Reserve(SortedHandles.Num());

		for (int32 SortedConstraintIndex = 0; SortedConstraintIndex < SortedHandles.Num(); ++SortedConstraintIndex)
		{
			FConstraintContainerHandle* Handle = SortedHandles[SortedConstraintIndex];
			int32 UnsortedConstraintIndex = Handle->GetConstraintIndex();

			SortedConstraintSettings.Add(ConstraintSettings[UnsortedConstraintIndex]);
			SortedConstraintParticles.Add(ConstraintParticles[UnsortedConstraintIndex]);
			SortedConstraintStates.Add(ConstraintStates[UnsortedConstraintIndex]);
			SetConstraintIndex(Handle, SortedConstraintIndex);
		}

		Swap(ConstraintSettings, SortedConstraintSettings);
		Swap(ConstraintParticles, SortedConstraintParticles);
		Swap(ConstraintStates, SortedConstraintStates);
		Swap(Handles, SortedHandles);
	}


	bool FPBDJointConstraints::IsConstraintEnabled(int32 ConstraintIndex) const
	{
		return !ConstraintStates[ConstraintIndex].bDisabled;
	}

	bool FPBDJointConstraints::IsConstraintBroken(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].bBroken;
	}

	bool FPBDJointConstraints::IsConstraintBreaking(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].bBreaking;
	}

	void FPBDJointConstraints::ClearConstraintBreaking(int32 ConstraintIndex)
	{
		ConstraintStates[ConstraintIndex].bBreaking = false;
	}

	bool FPBDJointConstraints::IsDriveTargetChanged(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].bDriveTargetChanged;
	}

	void FPBDJointConstraints::ClearDriveTargetChanged(int32 ConstraintIndex)
	{
		ConstraintStates[ConstraintIndex].bDriveTargetChanged = false;
	}

	void FPBDJointConstraints::SetConstraintEnabled(int32 ConstraintIndex, bool bEnabled)
	{
		const FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][0]);
		const FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][1]);

		if (bEnabled)
		{ 
			// only enable constraint if the particles are valid and not disabled
			// and if the constraint is not broken
			if (Particle0->Handle() != nullptr && !Particle0->Disabled()
				&& Particle1->Handle() != nullptr && !Particle1->Disabled()
				&& !IsConstraintBroken(ConstraintIndex))
			{
				ConstraintStates[ConstraintIndex].bDisabled = false;
			}
		}
		else
		{ 
			// desirable to allow disabling no matter what state the endpoints
			ConstraintStates[ConstraintIndex].bDisabled = true;
		}
	}

	void FPBDJointConstraints::SetConstraintBroken(int32 ConstraintIndex, bool bBroken)
	{
		ConstraintStates[ConstraintIndex].bBroken = bBroken;
	}

	void FPBDJointConstraints::SetConstraintBreaking(int32 ConstraintIndex, bool bBreaking)
	{
		ConstraintStates[ConstraintIndex].bBreaking = bBreaking;
	}

	void FPBDJointConstraints::SetDriveTargetChanged(int32 ConstraintIndex, bool bTargetChanged)
	{
		ConstraintStates[ConstraintIndex].bDriveTargetChanged = bTargetChanged;
	}

	void FPBDJointConstraints::BreakConstraint(int32 ConstraintIndex)
	{
		SetConstraintEnabled(ConstraintIndex, false);
		SetConstraintBroken(ConstraintIndex, true);
		SetConstraintBreaking(ConstraintIndex, true);
		if (BreakCallback)
		{
			BreakCallback(Handles[ConstraintIndex]);
		}
	}

	void FPBDJointConstraints::FixConstraint(int32 ConstraintIndex)
	{
		SetConstraintBroken(ConstraintIndex, false);
		SetConstraintEnabled(ConstraintIndex, true);
	}

	
	void FPBDJointConstraints::SetBreakCallback(const FJointBreakCallback& Callback)
	{
		BreakCallback = Callback;
	}


	void FPBDJointConstraints::ClearBreakCallback()
	{
		BreakCallback = nullptr;
	}


	const typename FPBDJointConstraints::FConstraintContainerHandle* FPBDJointConstraints::GetConstraintHandle(int32 ConstraintIndex) const
	{
		return Handles[ConstraintIndex];
	}

	
	typename FPBDJointConstraints::FConstraintContainerHandle* FPBDJointConstraints::GetConstraintHandle(int32 ConstraintIndex)
	{
		return Handles[ConstraintIndex];
	}

	
	const FParticlePair& FPBDJointConstraints::GetConstrainedParticles(int32 ConstraintIndex) const
	{
		return ConstraintParticles[ConstraintIndex];
	}

	
	const FPBDJointSettings& FPBDJointConstraints::GetConstraintSettings(int32 ConstraintIndex) const
	{
		return ConstraintSettings[ConstraintIndex];
	}


	void FPBDJointConstraints::SetConstraintSettings(int32 ConstraintIndex, const FPBDJointSettings& InConstraintSettings)
	{
		ConstraintSettings[ConstraintIndex] = InConstraintSettings;
		ConstraintSettings[ConstraintIndex].Sanitize();
	}

	void FPBDJointConstraints::SetLinearDrivePositionTarget(int32 ConstraintIndex, FVec3 InLinearDrivePositionTarget)
	{
		ConstraintSettings[ConstraintIndex].LinearDrivePositionTarget = InLinearDrivePositionTarget;
	}

	void FPBDJointConstraints::SetAngularDrivePositionTarget(int32 ConstraintIndex, FRotation3 InAngularDrivePositionTarget)
	{
		ConstraintSettings[ConstraintIndex].AngularDrivePositionTarget = InAngularDrivePositionTarget;
	}

	int32 FPBDJointConstraints::GetConstraintIsland(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].Island;
	}


	int32 FPBDJointConstraints::GetConstraintLevel(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].Level;
	}


	int32 FPBDJointConstraints::GetConstraintColor(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].Color;
	}


	FVec3 FPBDJointConstraints::GetConstraintLinearImpulse(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].LinearImpulse;
	}

	FVec3 FPBDJointConstraints::GetConstraintAngularImpulse(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].AngularImpulse;
	}

	ESyncState FPBDJointConstraints::GetConstraintSyncState(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].SyncState;
	}

	void FPBDJointConstraints::SetConstraintSyncState(int32 ConstraintIndex, ESyncState SyncState)
	{
		ConstraintStates[ConstraintIndex].SyncState = SyncState;
	}

	void FPBDJointConstraints::SetConstraintEnabledDuringResim(int32 ConstraintIndex, bool bEnabled)
	{
		ConstraintStates[ConstraintIndex].bEnabledDuringResim = bEnabled;
	}

	EResimType FPBDJointConstraints::GetConstraintResimType(int32 ConstraintIndex) const
	{
		return ConstraintStates[ConstraintIndex].ResimType;
	}

	void FPBDJointConstraints::PrepareTick()
	{
		SCOPE_CYCLE_COUNTER(STAT_Joints_PrepareTick);

		if (bJointsDirty)
		{
			ColorConstraints();
			SortConstraints();

			bJointsDirty = false;
		}

		if (Settings.bUseLinearSolver)
		{
			CachedConstraintSolvers.SetNum(NumConstraints());
		}
		else
		{
			ConstraintSolvers.SetNum(NumConstraints());		
		}
	}

	void FPBDJointConstraints::UnprepareTick()
	{
		SCOPE_CYCLE_COUNTER(STAT_Joints_UnprepareTick);

		if (Settings.bUseLinearSolver)
		{
			CachedConstraintSolvers.Empty();
		}
		else
		{
			ConstraintSolvers.Empty();
		}
	}
	
	void FPBDJointConstraints::CalculateConstraintSpace(int32 ConstraintIndex, FVec3& OutX0, FMatrix33& OutR0, FVec3& OutX1, FMatrix33& OutR1) const
	{
		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);
		FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index0]);
		FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index1]);
		const FVec3 P0 = FParticleUtilities::GetCoMWorldPosition(Particle0);
		const FRotation3 Q0 = FParticleUtilities::GetCoMWorldRotation(Particle0);
		const FVec3 P1 = FParticleUtilities::GetCoMWorldPosition(Particle1);
		const FRotation3 Q1 = FParticleUtilities::GetCoMWorldRotation(Particle1);
		const FRigidTransform3& XL0 = FParticleUtilities::ParticleLocalToCoMLocal(Particle0, ConstraintSettings[ConstraintIndex].ConnectorTransforms[Index0]);
		const FRigidTransform3& XL1 = FParticleUtilities::ParticleLocalToCoMLocal(Particle1, ConstraintSettings[ConstraintIndex].ConnectorTransforms[Index1]);

		OutX0 = P0 + Q0 * XL0.GetTranslation();
		OutX1 = P1 + Q1 * XL1.GetTranslation();
		OutR0 = FRotation3(Q0 * XL0.GetRotation()).ToMatrix();
		OutR1 = FRotation3(Q1 * XL1.GetRotation()).ToMatrix();
	}

	void FPBDJointConstraints::AddConstraintsToGraph(FPBDIslandManager& IslandManager)
	{ 
		IslandManager.AddContainerConstraints(*this);
	}

	void FPBDJointConstraints::AddBodies(FSolverBodyContainer& SolverBodyContainer)
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			AddBodies(ConstraintIndex, SolverBodyContainer);
		}
	}

	void FPBDJointConstraints::GatherInput(const FReal Dt)
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			GatherInput(ConstraintIndex, Dt);
		}
	}

	void FPBDJointConstraints::ScatterOutput(const FReal Dt)
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			ScatterOutput(ConstraintIndex, Dt);
		}
	}

	void FPBDJointConstraints::ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts)
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			ApplyPositionConstraint(Dt, ConstraintIndex, It, NumIts);
		}
	}

	void FPBDJointConstraints::ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts)
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			ApplyVelocityConstraint(Dt, ConstraintIndex, It, NumIts);
		}
	}

	void FPBDJointConstraints::ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts)
	{
		if (It == 0)
		{
			for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
			{
				PrepareProjectionConstraint(Dt, ConstraintIndex, It, NumIts);
			}
		}

		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			ApplyProjectionConstraint(Dt, ConstraintIndex, It, NumIts);
		}
	}

	void FPBDJointConstraints::AddBodies(const TArrayView<int32>& ConstraintIndices, FSolverBodyContainer& SolverBodyContainer)
	{
		for (int32 ConstraintIndex : ConstraintIndices)
		{
			AddBodies(ConstraintIndex, SolverBodyContainer);
		}
	}

	void FPBDJointConstraints::GatherInput(const TArrayView<int32>& ConstraintIndices, const FReal Dt)
	{
		for (int32 ConstraintIndex : ConstraintIndices)
		{
			GatherInput(ConstraintIndex, Dt);
		}
	}

	void FPBDJointConstraints::ScatterOutput(const TArrayView<int32>& ConstraintIndices, const FReal Dt)
	{
		for (int32 ConstraintIndex : ConstraintIndices)
		{
			ScatterOutput(ConstraintIndex, Dt);
		}
	}

	void FPBDJointConstraints::ApplyPositionConstraints(const TArrayView<int32>& ConstraintIndices, const FReal Dt, const int32 It, const int32 NumIts)
	{
		for (int32 ConstraintIndex : ConstraintIndices)
		{
			ApplyPositionConstraint(Dt, ConstraintIndex, It, NumIts);
		}
	}

	void FPBDJointConstraints::ApplyVelocityConstraints(const TArrayView<int32>& ConstraintIndices, const FReal Dt, const int32 It, const int32 NumIts)
	{
		for (int32 ConstraintIndex : ConstraintIndices)
		{
			ApplyVelocityConstraint(Dt, ConstraintIndex, It, NumIts);
		}
	}

	void FPBDJointConstraints::ApplyProjectionConstraints(const TArrayView<int32>& ConstraintIndices, const FReal Dt, const int32 It, const int32 NumIts)
	{
		if (It == 0)
		{
			for (int32 ConstraintIndex : ConstraintIndices)
			{
				PrepareProjectionConstraint(Dt, ConstraintIndex, It, NumIts);
			}
		}

		for (int32 ConstraintIndex : ConstraintIndices)
		{
			ApplyProjectionConstraint(Dt, ConstraintIndex, It, NumIts);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//
	// Begin single-particle solve methods
	//
	//////////////////////////////////////////////////////////////////////////

	void FPBDJointConstraints::AddBodies(const int32 ConstraintIndex, FSolverBodyContainer& SolverBodyContainer)
	{
		if (ConstraintStates[ConstraintIndex].bDisabled)
		{
			return;
		}

		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);
		FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index0]);
		FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index1]);

		// Find the solver bodies for the particles we constrain. This will add them to the container
		// if they aren't there already, and ensure that they are populated with the latest data.
		FSolverBody* SolverBody0 = SolverBodyContainer.FindOrAdd(Particle0);
		FSolverBody* SolverBody1 = SolverBodyContainer.FindOrAdd(Particle1);

		if (Settings.bUseLinearSolver)
		{
			FPBDJointCachedSolver& Solver = CachedConstraintSolvers[ConstraintIndex];
			Solver.SetSolverBodies(SolverBody0, SolverBody1);
		}
		else
		{
			FPBDJointSolver& Solver = ConstraintSolvers[ConstraintIndex];
			Solver.SetSolverBodies(SolverBody0, SolverBody1);
		}
	}

	void FPBDJointConstraints::GatherInput(const int32 ConstraintIndex, const FReal Dt)
	{
		if (ConstraintStates[ConstraintIndex].bDisabled)
		{
			return;
		}

		const FPBDJointSettings& JointSettings = ConstraintSettings[ConstraintIndex];
		const FTransformPair& JointFrames = JointSettings.ConnectorTransforms;

		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);
		FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index0]);
		FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index1]);

		if (Settings.bUseLinearSolver)
		{
			FPBDJointCachedSolver& Solver = CachedConstraintSolvers[ConstraintIndex];
			Solver.Init(
				Dt,
				Settings,
				JointSettings,
				FParticleUtilities::ParticleLocalToCoMLocal(Particle0, JointFrames[Index0]),
				FParticleUtilities::ParticleLocalToCoMLocal(Particle1, JointFrames[Index1]));
		}
		else
		{
			FPBDJointSolver& Solver = ConstraintSolvers[ConstraintIndex];
			Solver.Init(
				Dt,
				Settings,
				JointSettings,
				FParticleUtilities::ParticleLocalToCoMLocal(Particle0, JointFrames[Index0]),
				FParticleUtilities::ParticleLocalToCoMLocal(Particle1, JointFrames[Index1]));
		}

		// Plasticity should not be turned on in the middle of simulation.
		const bool bUseLinearPlasticity = JointSettings.LinearPlasticityLimit != FLT_MAX;
		if (bUseLinearPlasticity)
		{
			const bool bIsCOMDistanceInitialized = !FMath::IsNearlyEqual(JointSettings.LinearPlasticityInitialDistanceSquared, (FReal)FLT_MAX);
			if (!bIsCOMDistanceInitialized)
			{
				// Joint plasticity is baed on the distance of one of the moment arms of the joint. Typically, plasticity
				// will get setup from the joint pivot to the child COM (centor of mass), so that is found first. However, when 
				// the pivot is at the child COM then we fall back to the distance between thge pivot and parent COM.
				ConstraintSettings[ConstraintIndex].LinearPlasticityInitialDistanceSquared = JointSettings.ConnectorTransforms[Index1].GetTranslation().SizeSquared();
				if (FMath::IsNearlyZero(ConstraintSettings[ConstraintIndex].LinearPlasticityInitialDistanceSquared))
				{
					ConstraintSettings[ConstraintIndex].LinearPlasticityInitialDistanceSquared = JointSettings.ConnectorTransforms[Index0].GetTranslation().SizeSquared();
				}
				// @todo(chaos): move this to validation
				ensureMsgf(!FMath::IsNearlyZero(ConstraintSettings[ConstraintIndex].LinearPlasticityInitialDistanceSquared), TEXT("Plasticity made inactive due to Zero length difference between parent and child rigid body."));
			}
		}
	}

	void FPBDJointConstraints::ScatterOutput(const int32 ConstraintIndex, FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_Joints_Scatter);

		if (ConstraintStates[ConstraintIndex].bDisabled)
		{
			return;
		}

		FPBDJointState& JointState = ConstraintStates[ConstraintIndex];

		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);

		if (Settings.bUseLinearSolver)
		{
			FPBDJointCachedSolver& Solver = CachedConstraintSolvers[ConstraintIndex];
			// NOTE: LinearImpulse/AngularImpulse in the solver are not really impulses - they are mass-weighted position/rotation delta, or (impulse x dt).
			if (Dt > UE_SMALL_NUMBER)
			{
				if (Index0 == 0)
				{
					JointState.LinearImpulse = Solver.GetNetLinearImpulse() / Dt;
					JointState.AngularImpulse = Solver.GetNetAngularImpulse() / Dt;
				}
				else
				{
					// Particles were flipped in the solver...
					JointState.LinearImpulse = -Solver.GetNetLinearImpulse() / Dt;
					JointState.AngularImpulse = -Solver.GetNetAngularImpulse() / Dt;
				}
			}
			else
			{
				JointState.LinearImpulse = FVec3(0);
				JointState.AngularImpulse = FVec3(0);
			}

			ApplyPlasticityLimits(ConstraintIndex);

			// Remove our solver body reference (they are not valid between frames)
			CachedConstraintSolvers[ConstraintIndex].Deinit();
		}
		else
		{
			FPBDJointSolver& Solver = ConstraintSolvers[ConstraintIndex];
			// NOTE: LinearImpulse/AngularImpulse in the solver are not really impulses - they are mass-weighted position/rotation delta, or (impulse x dt).
			if (Dt > UE_SMALL_NUMBER)
			{
				if (Index0 == 0)
				{
					JointState.LinearImpulse = Solver.GetNetLinearImpulse() / Dt;
					JointState.AngularImpulse = Solver.GetNetAngularImpulse() / Dt;
				}
				else
				{
					// Particles were flipped in the solver...
					JointState.LinearImpulse = -Solver.GetNetLinearImpulse() / Dt;
					JointState.AngularImpulse = -Solver.GetNetAngularImpulse() / Dt;
				}
			}
			else
			{
				JointState.LinearImpulse = FVec3(0);
				JointState.AngularImpulse = FVec3(0);
			}

			ApplyPlasticityLimits(ConstraintIndex);

			// Remove our solver body reference (they are not valid between frames)
			ConstraintSolvers[ConstraintIndex].Deinit();
		}
	}

	FReal FPBDJointConstraints::CalculateIterationStiffness(int32 It, int32 NumIts) const
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


	bool FPBDJointConstraints::CanEvaluate(const int32 ConstraintIndex) const
	{
		if (!IsConstraintEnabled(ConstraintIndex))
		{
			return false;
		}

		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);
		const FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index0]);
		const FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index1]);

		// check for valid and enabled particles
		if (Particle0->Handle() == nullptr || Particle0->Disabled()
			|| Particle1->Handle() == nullptr || Particle1->Disabled())
		{
			return false;
		}

		// check valid particle and solver state
		if (Settings.bUseLinearSolver)
		{
			const FPBDJointCachedSolver& Solver = CachedConstraintSolvers[ConstraintIndex];
			if ((Particle0->Sleeping() && Particle1->Sleeping())
				|| (Particle0->IsKinematic() && Particle1->Sleeping())
				|| (Particle0->Sleeping() && Particle1->IsKinematic())
				|| (FMath::IsNearlyZero(Solver.InvM(0)) && FMath::IsNearlyZero(Solver.InvM(1))))
			{
				return false;
			}
		}
		else
		{
			const FPBDJointSolver& Solver = ConstraintSolvers[ConstraintIndex];
			if ((Particle0->Sleeping() && Particle1->Sleeping())
				|| (Particle0->IsKinematic() && Particle1->Sleeping())
				|| (Particle0->Sleeping() && Particle1->IsKinematic())
				|| (FMath::IsNearlyZero(Solver.InvM(0)) && FMath::IsNearlyZero(Solver.InvM(1))))
			{
				return false;
			}
		}
		return true;
	}

	// @todo(chaos): ShockPropagation needs to handle the parent/child being in opposite order
	FReal FPBDJointConstraints::CalculateShockPropagationInvMassScale(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FPBDJointSettings& JointSettings, const int32 It, const int32 NumIts) const
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

	// This position solver iterates over each of the inner constraints (position, twist, swing) and solves them independently.
	// This will converge slowly in some cases, particularly where resolving angular constraints violates position constraints and vice versa.
	bool FPBDJointConstraints::ApplyPositionConstraint(const FReal Dt, const int32 ConstraintIndex, const int32 It, const int32 NumIts)
	{
		if (!CanEvaluate(ConstraintIndex))
		{
			return false;
		}

		const TVector<TGeometryParticleHandle<FReal, 3>*, 2>& Constraint = ConstraintParticles[ConstraintIndex];
		UE_LOG(LogChaosJoint, VeryVerbose, TEXT("Solve Joint Position Constraint %d %s %s (dt = %f; it = %d / %d)"), ConstraintIndex, *Constraint[0]->ToString(), *Constraint[1]->ToString(), Dt, It, NumIts);

		// @todo(chaos): store this on the Solver object and don't access the particles here
		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);
		FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index0]);
		FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index1]);
		if ((Particle0->Sleeping() && Particle1->Sleeping())
			|| (Particle0->IsKinematic() && Particle1->Sleeping())
			|| (Particle0->Sleeping() && Particle1->IsKinematic()))
		{
			return false;
		}

		const FPBDJointSettings& JointSettings = ConstraintSettings[ConstraintIndex];
		if (Settings.bUseLinearSolver)
		{
			FPBDJointCachedSolver& Solver = CachedConstraintSolvers[ConstraintIndex];
			if ((FMath::IsNearlyZero(Solver.InvM(0)) && FMath::IsNearlyZero(Solver.InvM(1))))
			{
				return false;
			}

			Solver.Update(Dt, Settings, JointSettings);

			// Set parent inverse mass scale based on current shock propagation state
			const FReal ShockPropagationInvMassScale = CalculateShockPropagationInvMassScale(Solver.Body0(), Solver.Body1(), JointSettings, It, NumIts);
			Solver.SetShockPropagationScales(ShockPropagationInvMassScale, FReal(1), Dt);

			const FReal IterationStiffness = CalculateIterationStiffness(It, NumIts);
			Solver.ApplyConstraints(Dt, IterationStiffness, Settings, JointSettings);

			// @todo(ccaulfield): The break limit should really be applied to the impulse in the solver to prevent 1-frame impulses larger than the threshold
			if ((JointSettings.LinearBreakForce!=FLT_MAX) || (JointSettings.AngularBreakTorque!=FLT_MAX))
			{
				ApplyBreakThreshold(Dt, ConstraintIndex, Solver.GetNetLinearImpulse(), Solver.GetNetAngularImpulse());
			}
		}
		else
		{
			FPBDJointSolver& Solver = ConstraintSolvers[ConstraintIndex];
			if ((FMath::IsNearlyZero(Solver.InvM(0)) && FMath::IsNearlyZero(Solver.InvM(1))))
			{
				return false;
			}

			Solver.Update(Dt, Settings, JointSettings);

			// Set parent inverse mass scale based on current shock propagation state
			const FReal ShockPropagationInvMassScale = CalculateShockPropagationInvMassScale(Solver.Body0(), Solver.Body1(), JointSettings, It, NumIts);

			const FReal IterationStiffness = CalculateIterationStiffness(It, NumIts);
			Solver.SetShockPropagationScales(ShockPropagationInvMassScale, FReal(1));

			Solver.ApplyConstraints(Dt, IterationStiffness, Settings, JointSettings);

			// @todo(ccaulfield): The break limit should really be applied to the impulse in the solver to prevent 1-frame impulses larger than the threshold
			if ((JointSettings.LinearBreakForce!=FLT_MAX) || (JointSettings.AngularBreakTorque!=FLT_MAX))
			{
				ApplyBreakThreshold(Dt, ConstraintIndex, Solver.GetNetLinearImpulse(), Solver.GetNetAngularImpulse());
			}
		}

		return true;
	}

	// QuasiPBD applies a velocity solve in phase 2
	// Standard PBD does nothing
	bool FPBDJointConstraints::ApplyVelocityConstraint(const FReal Dt, const int32 ConstraintIndex, const int32 It, const int32 NumIts)
	{
		if (!CanEvaluate(ConstraintIndex))
		{
			return false;
		}

		const TVector<TGeometryParticleHandle<FReal, 3>*, 2>& Constraint = ConstraintParticles[ConstraintIndex];
		UE_LOG(LogChaosJoint, VeryVerbose, TEXT("Solve Joint Velocity Constraint %d %s %s (dt = %f; it = %d / %d)"), ConstraintIndex, *Constraint[0]->ToString(), *Constraint[1]->ToString(), Dt, It, NumIts);

		// @todo(chaos): store this on the Solver object and don't access the particles here
		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);
		FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index0]);
		FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index1]);
		if ((Particle0->Sleeping() && Particle1->Sleeping())
			|| (Particle0->IsKinematic() && Particle1->Sleeping())
			|| (Particle0->Sleeping() && Particle1->IsKinematic()))
		{
			return false;
		}

		const FPBDJointSettings& JointSettings = ConstraintSettings[ConstraintIndex];
		if (Settings.bUseLinearSolver)
		{
			FPBDJointCachedSolver& Solver = CachedConstraintSolvers[ConstraintIndex];
			if ((FMath::IsNearlyZero(Solver.InvM(0)) && FMath::IsNearlyZero(Solver.InvM(1))))
			{
				return false;
			}

			Solver.Update(Dt, Settings, JointSettings);
			
			// Set parent inverse mass scale based on current shock propagation state
			const FReal ShockPropagationInvMassScale = CalculateShockPropagationInvMassScale(Solver.Body0(), Solver.Body1(), JointSettings, It, NumIts);
			Solver.SetShockPropagationScales(ShockPropagationInvMassScale, FReal(1), Dt);

			const FReal IterationStiffness = CalculateIterationStiffness(It, NumIts);
			Solver.ApplyVelocityConstraints(Dt, IterationStiffness, Settings, JointSettings);

			// @todo(ccaulfield): should probably add to net impulses in push out too...(for breaking etc)
		}
		else
		{
			FPBDJointSolver& Solver = ConstraintSolvers[ConstraintIndex];
			if ((FMath::IsNearlyZero(Solver.InvM(0)) && FMath::IsNearlyZero(Solver.InvM(1))))
			{
				return false;
			}

			Solver.Update(Dt, Settings, JointSettings);

			// Set parent inverse mass scale based on current shock propagation state
			const FReal ShockPropagationInvMassScale = CalculateShockPropagationInvMassScale(Solver.Body0(), Solver.Body1(), JointSettings, It, NumIts);
			Solver.SetShockPropagationScales(ShockPropagationInvMassScale, FReal(1));

			const FReal IterationStiffness = CalculateIterationStiffness(It, NumIts);
			Solver.ApplyVelocityConstraints(Dt, IterationStiffness, Settings, JointSettings);

			// @todo(ccaulfield): should probably add to net impulses in push out too...(for breaking etc)
		}

		return true;
	}

	void FPBDJointConstraints::PrepareProjectionConstraint(const FReal Dt, const int32 ConstraintIndex, const int32 It, const int32 NumIts)
	{
		// Collect all the data for projection prior to the first iteration. 
		// This must happen for all joints before we project any joints so the the initial state for each joint is not polluted by any earlier projections.
		// @todo(chaos): if we ever support projection on other constraint types, we will need a PrepareProjection phase so that all constraint types
		// can initialize correctly before any constraints apply their projection. For now we can just check the iteration count is zero.
		check(It == 0);

		if (Settings.bUseLinearSolver)
		{
			if (!CanEvaluate(ConstraintIndex))
			{
				return;
			}

			FPBDJointCachedSolver& Solver = CachedConstraintSolvers[ConstraintIndex];
			const FPBDJointSettings& JointSettings = ConstraintSettings[ConstraintIndex];
			if (JointSettings.bProjectionEnabled && (Solver.IsDynamic(0) || Solver.IsDynamic(1)))
			{
				Solver.InitProjection(Dt, Settings, JointSettings);
			}
		}
	}

	// Projection phase
	bool FPBDJointConstraints::ApplyProjectionConstraint(const FReal Dt, const int32 ConstraintIndex, const int32 It, const int32 NumIts)
	{
		if (!CanEvaluate(ConstraintIndex))
		{
			return false;
		}

		const TVector<TGeometryParticleHandle<FReal, 3>*, 2>& Constraint = ConstraintParticles[ConstraintIndex];
		UE_LOG(LogChaosJoint, VeryVerbose, TEXT("Project Joint Constraint %d %s %s (dt = %f; it = %d / %d)"), ConstraintIndex, *Constraint[0]->ToString(), *Constraint[1]->ToString(), Dt, It, NumIts);

		const FPBDJointSettings& JointSettings = ConstraintSettings[ConstraintIndex];
		if (!JointSettings.bProjectionEnabled)
		{
			return false;
		}

		// @todo(chaos): store this on the Solver object and don't access the particles here
		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);
		FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index0]);
		FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index1]);
		if ((Particle0->Sleeping() && Particle1->Sleeping())
			|| (Particle0->IsKinematic() && Particle1->Sleeping())
			|| (Particle0->Sleeping() && Particle1->IsKinematic()))
		{
			return false;
		}

		if (Settings.bUseLinearSolver)
		{
			FPBDJointCachedSolver& Solver = CachedConstraintSolvers[ConstraintIndex];
			if ((FMath::IsNearlyZero(Solver.InvM(0)) && FMath::IsNearlyZero(Solver.InvM(1))))
			{
				return false;
			}

			if (It == 0)
			{
				Solver.ApplyTeleports(Dt, Settings, JointSettings);
			}

			Solver.ApplyProjections(Dt, Settings, JointSettings, (It==(NumIts-1)));
		}
		else
		{
			FPBDJointSolver& Solver = ConstraintSolvers[ConstraintIndex];
			if ((FMath::IsNearlyZero(Solver.InvM(0)) && FMath::IsNearlyZero(Solver.InvM(1))))
			{
				return false;
			}

			Solver.Update(Dt, Settings, JointSettings);

			if (It == 0)
			{
				// @todo(chaos): support reverse parent/child
				Solver.Body1().UpdateRotationDependentState();
				Solver.UpdateMasses(FReal(0), FReal(1));
			}

			Solver.ApplyProjections(Dt, Settings, JointSettings);
		}

		return true;
	}


	void FPBDJointConstraints::ApplyBreakThreshold(const FReal Dt, int32 ConstraintIndex, const FVec3& LinearImpulse, const FVec3& AngularImpulse)
	{
		const FPBDJointSettings& JointSettings = ConstraintSettings[ConstraintIndex];

		// NOTE: LinearImpulse/AngularImpulse are not really impulses - they are mass-weighted position/rotation delta, or (impulse x dt).
		// The Threshold is a force limit, so we need to convert it to a position delta caused by that force in one timestep

		bool bBreak = false;
		if (!bBreak && JointSettings.LinearBreakForce!=FLT_MAX)
		{
			const FReal LinearForceSq = LinearImpulse.SizeSquared() / (Dt * Dt * Dt * Dt);
			const FReal LinearThresholdSq = FMath::Square(JointSettings.LinearBreakForce);

			UE_LOG(LogChaosJoint, VeryVerbose, TEXT("Constraint %d Linear Break Check: %f / %f at Dt = %f"), ConstraintIndex, FMath::Sqrt(LinearForceSq), FMath::Sqrt(LinearThresholdSq), Dt);

			bBreak = LinearForceSq > LinearThresholdSq;
		}

		if (!bBreak && JointSettings.AngularBreakTorque!=FLT_MAX)
		{
			const FReal AngularForceSq = AngularImpulse.SizeSquared() / (Dt * Dt * Dt * Dt);
			const FReal AngularThresholdSq = FMath::Square(JointSettings.AngularBreakTorque);
			UE_LOG(LogChaosJoint, VeryVerbose, TEXT("Constraint %d Angular Break Check: %f / %f at Dt = %f"), ConstraintIndex, FMath::Sqrt(AngularForceSq), FMath::Sqrt(AngularThresholdSq), Dt);

			bBreak = AngularForceSq > AngularThresholdSq;
		}

		if (bBreak)
		{
			BreakConstraint(ConstraintIndex);
		}
	}


	void FPBDJointConstraints::ApplyPlasticityLimits(int32 ConstraintIndex)
	{
		FPBDJointSettings& JointSettings = ConstraintSettings[ConstraintIndex];
		const bool bHasLinearPlasticityLimit = JointSettings.LinearPlasticityLimit != FLT_MAX;
		const bool bHasAngularPlasticityLimit = JointSettings.AngularPlasticityLimit != FLT_MAX;
		const bool bHasPlasticityLimits = bHasLinearPlasticityLimit || bHasAngularPlasticityLimit;
		if (!bHasPlasticityLimits)
		{
			return;
		}

		if (!Settings.bEnableDrives)
		{
			return;
		}

		int32 Index0, Index1;
		GetConstrainedParticleIndices(ConstraintIndex, Index0, Index1);
		{
			FGenericParticleHandle Particle0 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index0]);
			FGenericParticleHandle Particle1 = FGenericParticleHandle(ConstraintParticles[ConstraintIndex][Index1]);
			if (Particle0->Disabled() || Particle1->Disabled())
			{
				return;
			}
		}

		FConstraintSolverBody& Body0 = Settings.bUseLinearSolver ? CachedConstraintSolvers[ConstraintIndex].Body(0) : ConstraintSolvers[ConstraintIndex].Body(0);
		FConstraintSolverBody& Body1 = Settings.bUseLinearSolver ? CachedConstraintSolvers[ConstraintIndex].Body(1) : ConstraintSolvers[ConstraintIndex].Body(1);

		const FTransformPair& ConstraintFramesLocal = JointSettings.ConnectorTransforms;
		FTransformPair ConstraintFramesGlobal(ConstraintFramesLocal[Index0] * FRigidTransform3(Body0.ActorP(), Body0.ActorQ()), ConstraintFramesLocal[Index1] * FRigidTransform3(Body1.ActorP(), Body1.ActorQ()));
		FQuat Q1 = ConstraintFramesGlobal[1].GetRotation();
		Q1.EnforceShortestArcWith(ConstraintFramesGlobal[0].GetRotation());
		ConstraintFramesGlobal[1].SetRotation(Q1);

		if (bHasLinearPlasticityLimit)
		{
			FVec3 LinearDisplacement = ConstraintFramesGlobal[0].InverseTransformPositionNoScale(ConstraintFramesGlobal[1].GetTranslation());

			// @todo(chaos): still need to warn against the case where all position drives are not enabled or all dimensions are locked. Warning should print out the joint names and should only print out once to avoid spamming.
			for (int32 Axis = 0; Axis < 3; Axis++)
			{
				if (!JointSettings.bLinearPositionDriveEnabled[Axis] || JointSettings.LinearMotionTypes[Axis] == EJointMotionType::Locked)
				{
					LinearDisplacement[Axis] = 0;
				}
			}
			// Assuming that the dimensions which are locked or have no targets are 0. in LinearDrivePositionTarget
			FReal LinearPlasticityDistanceThreshold = JointSettings.LinearPlasticityLimit * JointSettings.LinearPlasticityLimit * JointSettings.LinearPlasticityInitialDistanceSquared;
			if ((LinearDisplacement - JointSettings.LinearDrivePositionTarget).SizeSquared() > LinearPlasticityDistanceThreshold)
			{
				if (JointSettings.LinearPlasticityType == EPlasticityType::Free)
				{
					JointSettings.LinearDrivePositionTarget = LinearDisplacement;
					SetDriveTargetChanged(ConstraintIndex, true);
				}
				else // EPlasticityType::Shrink || EPlasticityType::Grow
				{
					// Shrink and Grow are based on the distance between the joint pivot and the child. 
					// Note, if the pivot is located at the COM of the child then shrink will not do anything. 
					FVec3 StartDelta = ConstraintFramesLocal[Index1].InverseTransformPositionNoScale(JointSettings.LinearDrivePositionTarget);
					FVec3 CurrentDelta = ConstraintFramesGlobal[Index1].InverseTransformPositionNoScale(Body1.P());

					if (JointSettings.LinearPlasticityType == EPlasticityType::Shrink && CurrentDelta.SizeSquared() < StartDelta.SizeSquared())
					{
						JointSettings.LinearDrivePositionTarget = LinearDisplacement;
						SetDriveTargetChanged(ConstraintIndex, true);
					}
					else if (JointSettings.LinearPlasticityType == EPlasticityType::Grow && CurrentDelta.SizeSquared() > StartDelta.SizeSquared())
					{
						JointSettings.LinearDrivePositionTarget = LinearDisplacement;
						SetDriveTargetChanged(ConstraintIndex, true);
					}
				}
			}
		}
		if (bHasAngularPlasticityLimit)
		{
			FRotation3 Swing, Twist; FPBDJointUtilities::DecomposeSwingTwistLocal(ConstraintFramesGlobal[0].GetRotation(), ConstraintFramesGlobal[1].GetRotation(), Swing, Twist);

			// @todo(chaos): still need to warn against the case where all position drives are not enabled or all dimensions are locked. Warning should print out the joint names and should only print out once to avoid spamming.
			if ((!JointSettings.bAngularSLerpPositionDriveEnabled && !JointSettings.bAngularTwistPositionDriveEnabled) || JointSettings.AngularMotionTypes[(int32)EJointAngularConstraintIndex::Twist] == EJointMotionType::Locked)
			{
				Twist = FRotation3::Identity;
			}
			// @todo(chaos): clamp rotation if only swing1(swing2) is locked
			if ((!JointSettings.bAngularSLerpPositionDriveEnabled && !JointSettings.bAngularSwingPositionDriveEnabled) || (JointSettings.AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing1] == EJointMotionType::Locked && JointSettings.AngularMotionTypes[(int32)EJointAngularConstraintIndex::Swing2] == EJointMotionType::Locked))
			{
				Swing = FRotation3::Identity;
			}

			const FRotation3 AngularDisplacement = Swing * Twist;
			// Assuming that the dimensions which are locked or have no targets are 0. in AngularDrivePositionTarget
			const FReal AngleRad = JointSettings.AngularDrivePositionTarget.AngularDistance(AngularDisplacement);
			if (AngleRad > JointSettings.AngularPlasticityLimit)
			{
				JointSettings.AngularDrivePositionTarget = AngularDisplacement;
				SetDriveTargetChanged(ConstraintIndex, true);
			}
		}
	}

	// Assign an Island, Level and Color to each constraint. Constraints must be processed in Level order, but
	// constraints of the same color are independent and can be processed in parallel (SIMD or Task)
	// NOTE: Constraints are the Vertices in this graph, and Edges connect constraints sharing a Particle. 
	// This makes the coloring of constraints simpler, but might not be what you expect so keep that in mind! 
	void FPBDJointConstraints::ColorConstraints()
	{
		// Add a Vertex for all constraints involving at least one dynamic body
		// Maintain a map from Constraint Index to Vertex Index
		FColoringGraph Graph;
		TArray<int32> ConstraintVertices; // Map of ConstraintIndex -> VertexIndex
		Graph.ReserveVertices(NumConstraints());
		ConstraintVertices.SetNumZeroed(NumConstraints());
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			if (ConstraintStates[ConstraintIndex].bDisabled)
			{
				continue;
			}

			TPBDRigidParticleHandle<FReal, 3>* Particle0 = ConstraintParticles[ConstraintIndex][0]->CastToRigidParticle();
			TPBDRigidParticleHandle<FReal, 3>* Particle1 = ConstraintParticles[ConstraintIndex][1]->CastToRigidParticle();
			bool IsParticle0Dynamic = (Particle0 != nullptr) && (Particle0->ObjectState() == EObjectStateType::Dynamic || Particle0->ObjectState() == EObjectStateType::Sleeping);
			bool IsParticle1Dynamic = (Particle1 != nullptr) && (Particle1->ObjectState() == EObjectStateType::Dynamic || Particle1->ObjectState() == EObjectStateType::Sleeping);

			bool bContainsDynamic = IsParticle0Dynamic || IsParticle1Dynamic;
			if (bContainsDynamic)
			{
				ConstraintVertices[ConstraintIndex] = Graph.AddVertex();

				// Set kinematic-connected constraints to level 0 to initialize level calculation
				bool bContainsKinematic = !IsParticle0Dynamic || !IsParticle1Dynamic;
				if (bContainsKinematic)
				{
					Graph.SetVertexLevel(ConstraintVertices[ConstraintIndex], 0);
				}
			}
			else
			{
				// Constraint has no dynamics
				// This shouldn't happen often, but particles can change from dynamic to kinematic
				// and back again witout destroying joints, so it needs to be supported
				ConstraintVertices[ConstraintIndex] = INDEX_NONE;
			}
		}

		// Build a map of particles to constraints. We ignore non-dynamic particles since
		// two constraints that share only a static/kinematic particle will not interact.
		TMap<const FGeometryParticleHandle*, TArray<int32>> ParticleConstraints; // Map of ParticleHandle -> Constraint Indices involving the particle
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			if (ConstraintStates[ConstraintIndex].bDisabled)
			{
				continue;
			}

			const FConstGenericParticleHandle Particle0 = ConstraintParticles[ConstraintIndex][0];
			const FConstGenericParticleHandle Particle1 = ConstraintParticles[ConstraintIndex][1];
			
			if (Particle0->IsDynamic())
			{
				ParticleConstraints.FindOrAdd(Particle0->Handle()).Add(ConstraintIndex);
			}
			if (Particle1->IsDynamic())
			{
				ParticleConstraints.FindOrAdd(Particle1->Handle()).Add(ConstraintIndex);
			}
		}

		// Connect constraints that share a dynamic particle
		// Algorithm:
		//		Loop over particles
		//			Loop over all constraint pairs on that particle
		//				Add an edge to connect the constraints
		//
		Graph.ReserveEdges((ParticleConstraints.Num() * (ParticleConstraints.Num() - 1)) / 2);
		for (auto& ParticleConstraintsElement : ParticleConstraints)
		{
			// Loop over constraint pairs connected to the particle
			// Visit each pair only once (see inner loop indexing)
			const TArray<int32>& ParticleConstraintIndices = ParticleConstraintsElement.Value;
			const int32 NumParticleConstraintIndices = ParticleConstraintIndices.Num();
			for (int32 ParticleConstraintIndex0 = 0; ParticleConstraintIndex0 < NumParticleConstraintIndices; ++ParticleConstraintIndex0)
			{
				const int32 ConstraintIndex0 = ParticleConstraintIndices[ParticleConstraintIndex0];
				const int32 VertexIndex0 = ConstraintVertices[ConstraintIndex0];
				if(VertexIndex0 == INDEX_NONE)
				{
					// Constraint has no dynamics
					continue;
				}

				for (int32 ParticleConstraintIndex1 = ParticleConstraintIndex0 + 1; ParticleConstraintIndex1 < NumParticleConstraintIndices; ++ParticleConstraintIndex1)
				{
					const int32 ConstraintIndex1 = ParticleConstraintIndices[ParticleConstraintIndex1];
					const int32 VertexIndex1 = ConstraintVertices[ConstraintIndex1];
					if(VertexIndex1 == INDEX_NONE)
					{
						// Constraint has no dynamics
						continue;
					}
					Graph.AddEdge(VertexIndex0, VertexIndex1);
				}
			}
		}

		// Colorize the graph
		Graph.Islandize();
		Graph.Levelize();
		Graph.Colorize();

		// Set the constraint colors
		for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints(); ++ConstraintIndex)
		{
			if ( ConstraintStates[ConstraintIndex].bDisabled) continue;

			int32 VertexIndex = ConstraintVertices[ConstraintIndex];
			ConstraintStates[ConstraintIndex].Island = Graph.GetVertexIsland(VertexIndex);
			ConstraintStates[ConstraintIndex].IslandSize = Graph.GetVertexIslandSize(VertexIndex);
			ConstraintStates[ConstraintIndex].Level = Graph.GetVertexLevel(VertexIndex);
			ConstraintStates[ConstraintIndex].Color = Graph.GetVertexColor(VertexIndex);
		}
	}

}

namespace Chaos
{
	template class TIndexedContainerConstraintHandle<FPBDJointConstraints>;
}
