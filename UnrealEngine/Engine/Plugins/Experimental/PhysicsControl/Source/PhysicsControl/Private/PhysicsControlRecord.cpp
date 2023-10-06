// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlRecord.h"
#include "PhysicsControlComponentLog.h"
#include "PhysicsControlComponentHelpers.h"

#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodyInstance.h"

//======================================================================================================================
void FPhysicsControlState::Reset()
{
	if (ConstraintInstance)
	{
		ConstraintInstance->TermConstraint();
	}
	*this = FPhysicsControlState();
}

//======================================================================================================================
// A constraint created with identity frames will just have the child frame at the mesh origin,
// which is not necessarily where the center of gravity is.
FConstraintInstance* FPhysicsControlRecord::CreateConstraint(UObject* ConstraintDebugOwner, FName ControlName)
{
	if (!PhysicsControlState.ConstraintInstance)
	{
		PhysicsControlState.ConstraintInstance = MakeShared<FConstraintInstance>();
	}
	FConstraintInstance* ConstraintInstance = PhysicsControlState.ConstraintInstance.Get();

	FBodyInstance* ParentBody = UE::PhysicsControlComponent::GetBodyInstance(
		PhysicsControl.ParentMeshComponent, PhysicsControl.ParentBoneName);
	FBodyInstance* ChildBody = UE::PhysicsControlComponent::GetBodyInstance(
		PhysicsControl.ChildMeshComponent, PhysicsControl.ChildBoneName);

	if (PhysicsControl.ParentMeshComponent && !PhysicsControl.ParentBoneName.IsNone() && !ParentBody)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, 
			TEXT("Failed to find expected parent body %s when making constraint for control %s"), 
			*PhysicsControl.ParentBoneName.ToString(),
			*ControlName.ToString());
		return nullptr;
	}
	if (PhysicsControl.ChildMeshComponent && !PhysicsControl.ChildBoneName.IsNone() && !ChildBody)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, 
			TEXT("Failed to find expected child body %s when making constraint for control %s"), 
			*PhysicsControl.ChildBoneName.ToString(),
			*ControlName.ToString());
		return nullptr;
	}

	ConstraintInstance->InitConstraint(ChildBody, ParentBody, 1.0f, ConstraintDebugOwner);

	// Ensure the control point is set
	UpdateConstraintControlPoint();

	ConstraintInstance->SetLinearXMotion(ELinearConstraintMotion::LCM_Free);
	ConstraintInstance->SetLinearYMotion(ELinearConstraintMotion::LCM_Free);
	ConstraintInstance->SetLinearZMotion(ELinearConstraintMotion::LCM_Free);
	ConstraintInstance->SetAngularSwing1Motion(EAngularConstraintMotion::ACM_Free);
	ConstraintInstance->SetAngularSwing2Motion(EAngularConstraintMotion::ACM_Free);
	ConstraintInstance->SetAngularTwistMotion(EAngularConstraintMotion::ACM_Free);

	ConstraintInstance->SetLinearPositionDrive(true, true, true);
	ConstraintInstance->SetLinearVelocityDrive(true, true, true);
	ConstraintInstance->SetAngularDriveMode(EAngularDriveMode::SLERP);
	ConstraintInstance->SetOrientationDriveSLERP(true);
	ConstraintInstance->SetAngularVelocityDriveSLERP(true);

	ConstraintInstance->SetDisableCollision(PhysicsControl.ControlSettings.bDisableCollision);

	return ConstraintInstance;
}

//======================================================================================================================
// Note that, by default, the constraint frames are simply identity. We only modify Frame1, which 
// corresponds to the child frame. Frame2 will always be identity, because we never change it.
void FPhysicsControlRecord::UpdateConstraintControlPoint()
{
	FConstraintInstance* ConstraintInstance = PhysicsControlState.ConstraintInstance.Get();
	if (ConstraintInstance)
	{
		// Constraints are child then parent
		FTransform Frame1 = ConstraintInstance->GetRefFrame(EConstraintFrame::Frame1);
		Frame1.SetTranslation(PhysicsControl.ControlSettings.ControlPoint);
		ConstraintInstance->SetRefFrame(EConstraintFrame::Frame1, Frame1);
	}
}

//======================================================================================================================
void FPhysicsControlRecord::ResetControlPoint()
{
	FBodyInstance* ChildBodyInstance = UE::PhysicsControlComponent::GetBodyInstance(
		PhysicsControl.ChildMeshComponent, PhysicsControl.ChildBoneName);

	PhysicsControl.ControlSettings.ControlPoint = ChildBodyInstance
		? ChildBodyInstance->GetMassSpaceLocal().GetTranslation() : FVector::ZeroVector;

	UpdateConstraintControlPoint();
}

