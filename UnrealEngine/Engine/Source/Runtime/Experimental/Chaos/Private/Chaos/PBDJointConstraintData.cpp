// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDJointConstraintData.h"
#include "PBDRigidsSolver.h"

namespace Chaos
{
	FJointConstraint::FJointConstraint()
		: FConstraintBase(EConstraintType::JointConstraintType)
		, KinematicEndPoint(nullptr)
	{
	}

	void FJointConstraint::SetKinematicEndPoint(FSingleParticlePhysicsProxy* InDummyParticle, FPBDRigidsSolver* Solver)
	{
		ensure(KinematicEndPoint == nullptr);
		KinematicEndPoint = InDummyParticle;
		Solver->RegisterObject(KinematicEndPoint);
	}

	FSingleParticlePhysicsProxy* FJointConstraint::GetKinematicEndPoint() const
	{
		return KinematicEndPoint;
	}

	void FJointConstraint::SetLinearPositionDriveEnabled(TVector<bool,3> Enabled)
	{
		SetLinearPositionDriveXEnabled(Enabled.X);
		SetLinearPositionDriveYEnabled(Enabled.Y);
		SetLinearPositionDriveZEnabled(Enabled.Z);
	}


	void FJointConstraint::SetLinearVelocityDriveEnabled(TVector<bool,3> Enabled)
	{
		SetLinearVelocityDriveXEnabled(Enabled.X);
		SetLinearVelocityDriveYEnabled(Enabled.Y);
		SetLinearVelocityDriveZEnabled(Enabled.Z);
	}

	void FJointConstraint::ReleaseKinematicEndPoint(FPBDRigidsSolver* Solver)
	{
		if (KinematicEndPoint)
		{
			Solver->UnregisterObject(KinematicEndPoint);
			KinematicEndPoint = nullptr;
		}
	}


} // Chaos
