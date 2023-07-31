// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PBIKConstraint.h"
#include "Core/PBIKSolver.h"
#include "Math/UnrealMathUtility.h"

namespace PBIK
{
	
FJointConstraint::FJointConstraint(FRigidBody* InA, FRigidBody* InB)
{
	A = InA;
	B = InB;

	const FVector PinPoint = B->Bone->Position;
	PinPointLocalToA = A->Rotation.Inverse() * (PinPoint - A->Position);
	PinPointLocalToB = B->Rotation.Inverse() * (PinPoint - B->Position);

	XOrig = B->InitialRotation * FVector(1, 0, 0);
	YOrig = B->InitialRotation * FVector(0, 1, 0);
	ZOrig = B->InitialRotation * FVector(0, 0, 1);

	AngleX = AngleY = AngleZ = 0;
}
	
void FJointConstraint::Solve(bool bMoveSubRoots)
{
	// get pos correction to use for rotation
	FVector OffsetA;
	FVector OffsetB;
	FVector Correction = GetPositionCorrection(OffsetA, OffsetB);

	// apply rotation from correction 
	// at pin point to both bodies
	A->ApplyPushToRotateBody(Correction, OffsetA);
	B->ApplyPushToRotateBody(-Correction, OffsetB);

	// enforce joint limits
	UpdateJointLimits();

	// calc inv mass of body A
	float AInvMass = 1.0f;
	AInvMass -= (A->Pin && A->Pin->bEnabled) ? A->Pin->Alpha : 0.0f;
	AInvMass -= !bMoveSubRoots && A->Bone->bIsSubRoot ? 1.0f : 0.0f;
	AInvMass = AInvMass <= 0.0f ? 0.0f : AInvMass;

	// calc inv mass of body B
	float BInvMass = 1.0f;
	BInvMass -= (B->Pin && B->Pin->bEnabled) ? B->Pin->Alpha : 0.0f;
	BInvMass -= !bMoveSubRoots && B->Bone->bIsSubRoot ? 1.0f : 0.0f;
	BInvMass = BInvMass <= 0.0f ? 0.0f : BInvMass;

	const float InvMassSum = AInvMass + BInvMass;
	if (FMath::IsNearlyZero(InvMassSum))
	{
		return; // no correction can be applied on full locked configuration
	}
	const float OneOverInvMassSum = 1.0f / InvMassSum;

	// apply positional correction to align pin point on both Bodies
	// NOTE: applying position correction AFTER rotation takes into
	// consideration change in relative pin locations mid-step after
	// bodies have been rotated by ApplyPushToRotateBody
	Correction = GetPositionCorrection(OffsetA, OffsetB);
	Correction = Correction * OneOverInvMassSum;

	A->ApplyPushToPosition(Correction * AInvMass);
	B->ApplyPushToPosition(-Correction * BInvMass);
}

void FJointConstraint::RemoveStretch()
{
	// apply position correction, keeping parent body fixed
	FVector ToA;
	FVector ToB;
	const FVector Correction = GetPositionCorrection(ToA, ToB);
	B->Position -= Correction;
}

FVector FJointConstraint::GetPositionCorrection(FVector& OutBodyToA, FVector& OutBodyToB) const
{
	OutBodyToA = A->Rotation * PinPointLocalToA;
	OutBodyToB = B->Rotation * PinPointLocalToB;
	const FVector PinPointOnA = A->Position + OutBodyToA;
	const FVector PinPointOnB = B->Position + OutBodyToB;
	return (PinPointOnB - PinPointOnA);
}

void FJointConstraint::ApplyRotationCorrection(FQuat DeltaQ) const
{
	A->ApplyRotationDelta(DeltaQ * (1.0f - A->J.RotationStiffness));
	B->ApplyRotationDelta(DeltaQ * -1.0f * (1.0f - B->J.RotationStiffness));
}

void FJointConstraint::UpdateJointLimits()
{
	FBoneSettings& J = B->J;

	// no limits at all
	if (J.X == ELimitType::Free &&
		J.Y == ELimitType::Free &&
		J.Z == ELimitType::Free)
	{
		return;
	}

	// force max angles > min angles
	J.MaxX = J.MaxX < J.MinX ? J.MinX + 1 : J.MaxX;
	J.MaxY = J.MaxY < J.MinY ? J.MinY + 1 : J.MaxY;
	J.MaxZ = J.MaxZ < J.MinZ ? J.MinZ + 1 : J.MaxZ;

	// determine which axes are explicitly or implicitly locked (limited with very small allowable angle)
	const bool bLockX = (J.X == ELimitType::Locked) || (J.X == ELimitType::Limited && ((J.MaxX - J.MinX) < 2.0f));
	const bool bLockY = (J.Y == ELimitType::Locked) || (J.Y == ELimitType::Limited && ((J.MaxY - J.MinY) < 2.0f));
	const bool bLockZ = (J.Z == ELimitType::Locked) || (J.Z == ELimitType::Limited && ((J.MaxZ - J.MinZ) < 2.0f));

	// when locked on THREE axes, consider the bone FIXED
	if (bLockX && bLockY && bLockZ)
	{
		const FQuat OrigOffset = A->InitialRotation * B->InitialRotation.Inverse();
		const FQuat CurrentOffset = A->Rotation * B->Rotation.Inverse();
		const FQuat DeltaOffset = OrigOffset * CurrentOffset.Inverse();
		const FQuat DeltaQ = FQuat(DeltaOffset.X, DeltaOffset.Y, DeltaOffset.Z, 0.0f) * 2.0f;
		ApplyRotationCorrection(DeltaQ);
		return; // fixed bones cannot move at all
	}

	// when locked on TWO axes, consider the bone a HINGE (mutually exclusive)
	const bool bXHinge = !bLockX && bLockY && bLockZ;
	const bool bYHinge = bLockX && !bLockY && bLockZ;
	const bool bZHinge = bLockX && bLockY && !bLockZ;

	// apply hinge corrections
	if (bXHinge)
	{
		UpdateLocalRotateAxes(true, false, false);
		RotateToAlignAxes(XA, XB);
	}
	else if (bYHinge)
	{
		UpdateLocalRotateAxes(false, true, false);
		RotateToAlignAxes(YA, YB);
	}
	else if (bZHinge)
	{
		UpdateLocalRotateAxes(false, false, true);
		RotateToAlignAxes(ZA, ZB);
	}

	// enforce min/max angles
	const bool bLimitX = J.X == ELimitType::Limited && !bLockX;
	const bool bLimitY = J.Y == ELimitType::Limited && !bLockY;
	const bool bLimitZ = J.Z == ELimitType::Limited && !bLockZ;
	if (bLimitX || bLimitY || bLimitZ)
	{
		DecomposeRotationAngles();
	}

	if (bLimitX)
	{
		RotateWithinLimits(J.MinX, J.MaxX, AngleX, XA, ZBProjOnX, ZA);
	}

	if (bLimitY)
	{
		RotateWithinLimits(J.MinY, J.MaxY, AngleY, YA, ZBProjOnY, ZA);
	}

	if (bLimitZ)
	{
		RotateWithinLimits(J.MinZ, J.MaxZ, AngleZ, ZA, YBProjOnZ, YA);
	}
}

void FJointConstraint::RotateToAlignAxes(const FVector& AxisA, const FVector& AxisB) const
{
	const FVector ACrossB = FVector::CrossProduct(AxisA, AxisB);
	FVector Axis;
	float Magnitude;
	ACrossB.ToDirectionAndLength(Axis, Magnitude);
	const float EffectiveInvMass = FVector::DotProduct(Axis, Axis); // Todo incorporate Body Mass here?
	if (EffectiveInvMass < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float DeltaLambda = Magnitude / EffectiveInvMass;
	const FVector Push = Axis * DeltaLambda;
	const FQuat DeltaQ = FQuat(Push.X, Push.Y, Push.Z, 0.0f);
	ApplyRotationCorrection(DeltaQ);
}

void FJointConstraint::RotateWithinLimits(
	float MinAngle,
	float MaxAngle,
	float CurrentAngle,
	FVector RotAxis,
	FVector CurVec,
	FVector RefVec) const
{
	const bool bBeyondMin = CurrentAngle < MinAngle;
	const bool bBeyondMax = CurrentAngle > MaxAngle;
	if (!(bBeyondMin || bBeyondMax))
	{
		return;
	}
	const float TgtAngle = bBeyondMin ? MinAngle : MaxAngle;
	const FQuat TgtRot = FQuat(RotAxis, FMath::DegreesToRadians(TgtAngle));
	const FVector TgtVec = TgtRot * RefVec;
	RotateToAlignAxes(TgtVec, CurVec);
}

void FJointConstraint::UpdateLocalRotateAxes(bool bX, bool bY, bool bZ)
{
	const FQuat ARot = A->Rotation * A->InitialRotation.Inverse();
	const FQuat BRot = B->Rotation * B->InitialRotation.Inverse();

	if (bX)
	{
		XA = ARot * XOrig;
		XB = BRot * XOrig;
	}

	if (bY)
	{
		YA = ARot * YOrig;
		YB = BRot * YOrig;
	}

	if (bZ)
	{
		ZA = ARot * ZOrig;
		ZB = BRot * ZOrig;
	}
}

void FJointConstraint::DecomposeRotationAngles()
{
	const FQuat ARot = A->Rotation * A->InitialRotation.Inverse();
	const FQuat BRot = B->Rotation * B->InitialRotation.Inverse();

	XA = ARot * XOrig;
	YA = ARot * YOrig;
	ZA = ARot * ZOrig;
	XB = BRot * XOrig;
	YB = BRot * YOrig;
	ZB = BRot * ZOrig;

	ZBProjOnX = FVector::VectorPlaneProject(ZB, XA).GetSafeNormal();
	ZBProjOnY = FVector::VectorPlaneProject(ZB, YA).GetSafeNormal();
	YBProjOnZ = FVector::VectorPlaneProject(YB, ZA).GetSafeNormal();

	AngleX = SignedAngleBetweenNormals(ZA, ZBProjOnX, XA);
	AngleY = SignedAngleBetweenNormals(ZA, ZBProjOnY, YA);
	AngleZ = SignedAngleBetweenNormals(YA, YBProjOnZ, ZA);
}

float FJointConstraint::SignedAngleBetweenNormals(
	const FVector& From, 
	const FVector& To, 
	const FVector& Axis) const
{
	const float FromDotTo = FVector::DotProduct(From, To);
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FromDotTo));
	const FVector Cross = FVector::CrossProduct(From, To);
	const float Dot = FVector::DotProduct(Cross, Axis);
	return Dot >= 0 ? Angle : -Angle;
}
	
FPinConstraint::FPinConstraint(
	FRigidBody* InBody,
	const FVector& InPinPositionOrig,
	const FQuat& InPinRotationOrig,
	const bool bInPinRotation)
{
	GoalPosition = InPinPositionOrig;
	GoalRotation = InPinRotationOrig;
	
	A = InBody;
	PinPointLocalToA = A->Rotation.Inverse() * (GoalPosition - A->Position);

	ARotLocalToPin = A->Rotation * GoalRotation.Inverse();
	bPinRotation = bInPinRotation;
}

void FPinConstraint::Solve(bool bMoveSubRoots)
{
	if (!bEnabled || (Alpha <= KINDA_SMALL_NUMBER))
	{
		return;
	}

	if (!bMoveSubRoots && A->Bone->bIsSubRoot)
	{
		return;
	}

	// get positional correction for rotation
	FVector AToPinPoint;
	FVector Correction = GetPositionCorrection(AToPinPoint);

	if (bPinRotation)
	{
		// move body to pin location
		A->Position += Correction;
		// keep body at fixed rotation relative to the pin goal rotation
		A->Rotation = ARotLocalToPin * GoalRotation;
	}else
	{
		// rotate body from alignment of pin points
		A->ApplyPushToRotateBody(Correction, AToPinPoint);

		// apply positional correction to Body to align with target (after rotation)
		// (applying directly without considering PositionStiffness because PinConstraints need
		// to precisely pull the attached body to achieve convergence)
		Correction = GetPositionCorrection(AToPinPoint);
		A->Position += Correction; 
	}
}

void FPinConstraint::SetGoal(const FVector& InGoalPosition, const FQuat& InGoalRotation, const float InAlpha)
{
	GoalPosition = InGoalPosition;
	GoalRotation = InGoalRotation;
	Alpha = InAlpha;
}

FVector FPinConstraint::GetPositionCorrection(FVector& OutBodyToPinPoint) const
{
	OutBodyToPinPoint = A->Rotation * PinPointLocalToA;
	const FVector PinPoint = A->Position + OutBodyToPinPoint;
	return (GoalPosition - PinPoint) * Alpha;
}

} // namespace

