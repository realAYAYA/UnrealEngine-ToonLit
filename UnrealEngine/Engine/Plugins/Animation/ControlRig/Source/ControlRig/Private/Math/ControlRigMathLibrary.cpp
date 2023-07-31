// Copyright Epic Games, Inc. All Rights Reserved.

#include "Math/ControlRigMathLibrary.h"
#include "AHEasing/easing.h"
#include "TwoBoneIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigMathLibrary)

float FControlRigMathLibrary::AngleBetween(const FVector& A, const FVector& B)
{
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		return 0.f;
	}

	return FMath::Acos(FVector::DotProduct(A, B) / (A.Size() * B.Size()));
}

void FControlRigMathLibrary::FourPointBezier(const FCRFourPointBezier& Bezier, float T, FVector& OutPosition, FVector& OutTangent)
{
	FourPointBezier(Bezier.A, Bezier.B, Bezier.C, Bezier.D, T, OutPosition, OutTangent);
}

void FControlRigMathLibrary::FourPointBezier(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float T, FVector& OutPosition, FVector& OutTangent)
{
	const FVector AB = FMath::Lerp<FVector>(A, B, T);
	const FVector BC = FMath::Lerp<FVector>(B, C, T);
	const FVector CD = FMath::Lerp<FVector>(C, D, T);
	const FVector ABBC = FMath::Lerp<FVector>(AB, BC, T);
	const FVector BCCD = FMath::Lerp<FVector>(BC, CD, T);
	OutPosition = FMath::Lerp<FVector>(ABBC, BCCD, T);
	OutTangent = (BCCD - ABBC).GetSafeNormal();
}

float FControlRigMathLibrary::EaseFloat(float Value, EControlRigAnimEasingType Type)
{
	switch(Type)
	{
		case EControlRigAnimEasingType::Linear:
		{
			break;
		}
		case EControlRigAnimEasingType::QuadraticEaseIn:
		{
			Value = QuadraticEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::QuadraticEaseOut:
		{
			Value = QuadraticEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::QuadraticEaseInOut:
		{
			Value = QuadraticEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::CubicEaseIn:
		{
			Value = CubicEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::CubicEaseOut:
		{
			Value = CubicEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::CubicEaseInOut:
		{
			Value = CubicEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::QuarticEaseIn:
		{
			Value = QuarticEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::QuarticEaseOut:
		{
			Value = QuarticEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::QuarticEaseInOut:
		{
			Value = QuarticEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::QuinticEaseIn:
		{
			Value = QuinticEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::QuinticEaseOut:
		{
			Value = QuinticEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::QuinticEaseInOut:
		{
			Value = QuinticEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::SineEaseIn:
		{
			Value = SineEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::SineEaseOut:
		{
			Value = SineEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::SineEaseInOut:
		{
			Value = SineEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::CircularEaseIn:
		{
			Value = CircularEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::CircularEaseOut:
		{
			Value = CircularEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::CircularEaseInOut:
		{
			Value = CircularEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::ExponentialEaseIn:
		{
			Value = ExponentialEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::ExponentialEaseOut:
		{
			Value = ExponentialEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::ExponentialEaseInOut:
		{
			Value = ExponentialEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::ElasticEaseIn:
		{
			Value = ElasticEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::ElasticEaseOut:
		{
			Value = ElasticEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::ElasticEaseInOut:
		{
			Value = ElasticEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::BackEaseIn:
		{
			Value = BackEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::BackEaseOut:
		{
			Value = BackEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::BackEaseInOut:
		{
			Value = BackEaseInOut(Value);
			break;
		}
		case EControlRigAnimEasingType::BounceEaseIn:
		{
			Value = BounceEaseIn(Value);
			break;
		}
		case EControlRigAnimEasingType::BounceEaseOut:
		{
			Value = BounceEaseOut(Value);
			break;
		}
		case EControlRigAnimEasingType::BounceEaseInOut:
		{
			Value = BounceEaseInOut(Value);
			break;
		}
	}

	return Value;
}

FTransform FControlRigMathLibrary::LerpTransform(const FTransform& A, const FTransform& B, float T)
{
	FTransform Result = FTransform::Identity;
	Result.SetLocation(FMath::Lerp<FVector>(A.GetLocation(), B.GetLocation(), T));
	Result.SetRotation(FQuat::Slerp(A.GetRotation(), B.GetRotation(), T));
	Result.SetScale3D(FMath::Lerp<FVector>(A.GetScale3D(), B.GetScale3D(), T));
	return Result;
}

void FControlRigMathLibrary::SolveBasicTwoBoneIK(FTransform& BoneA, FTransform& BoneB, FTransform& Effector, const FVector& PoleVector, const FVector& PrimaryAxis, const FVector& SecondaryAxis, float SecondaryAxisWeight, float BoneALength, float BoneBLength, bool bEnableStretch, float StretchStartRatio, float StretchMaxRatio)
{
	FVector RootPos = BoneA.GetLocation();
	FVector ElbowPos = BoneB.GetLocation();
	FVector EffectorPos = Effector.GetLocation();

	AnimationCore::SolveTwoBoneIK(RootPos, ElbowPos, EffectorPos, PoleVector, EffectorPos, ElbowPos, EffectorPos, BoneALength, BoneBLength, bEnableStretch, StretchStartRatio, StretchMaxRatio);

	BoneB.SetLocation(ElbowPos);
	Effector.SetLocation(EffectorPos);

	FVector Axis = BoneA.TransformVectorNoScale(PrimaryAxis);
	FVector Target1 = BoneB.GetLocation() - BoneA.GetLocation();

	FVector BoneBLocation = BoneB.GetLocation();
	if (!Target1.IsNearlyZero() && !Axis.IsNearlyZero())
	{
		Target1 = Target1.GetSafeNormal();

		// If the elbow is placed in the segment between the root and the effector (the limb is completely extended)
		// offset the elbow towards the pole vector in order to compute the rotations
		{
			FVector BaseDirection = (EffectorPos - RootPos).GetSafeNormal();
			if (FMath::Abs(FVector::DotProduct(BaseDirection, Target1) - 1.0f) < KINDA_SMALL_NUMBER)
			{
				// Offset the bone location to use when calculating the rotations
				BoneBLocation += (PoleVector - BoneBLocation).GetSafeNormal() * KINDA_SMALL_NUMBER*2.f;
				Target1 = (BoneBLocation - BoneA.GetLocation()).GetSafeNormal();
			}
		}
		
		FQuat Rotation1 = FQuat::FindBetweenNormals(Axis, Target1);
		BoneA.SetRotation((Rotation1 * BoneA.GetRotation()).GetNormalized());

		Axis = BoneA.TransformVectorNoScale(SecondaryAxis);

		if (SecondaryAxisWeight > SMALL_NUMBER)
		{
			FVector Target2 = BoneBLocation - (Effector.GetLocation() + BoneA.GetLocation()) * 0.5f;
			if (!Target2.IsNearlyZero() && !Axis.IsNearlyZero())
			{
				Target2 = Target2 - FVector::DotProduct(Target2, Target1) * Target1;
				Target2 = Target2.GetSafeNormal();

				FQuat Rotation2 = FQuat::FindBetweenNormals(Axis, Target2);
				if (!FMath::IsNearlyEqual(SecondaryAxisWeight, 1.f))
				{
					FVector RotationAxis = Rotation2.GetRotationAxis();
					float RotationAngle = Rotation2.GetAngle();
					Rotation2 = FQuat(RotationAxis, RotationAngle * FMath::Clamp<float>(SecondaryAxisWeight, 0.f, 1.f));
				}
				BoneA.SetRotation((Rotation2 * BoneA.GetRotation()).GetNormalized());
			}
		}
	}

	Axis = BoneB.TransformVectorNoScale(PrimaryAxis);
	Target1 = Effector.GetLocation() - BoneBLocation;
	if (!Target1.IsNearlyZero() && !Axis.IsNearlyZero())
	{
		Target1 = Target1.GetSafeNormal();
		FQuat Rotation1 = FQuat::FindBetweenNormals(Axis, Target1);
		BoneB.SetRotation((Rotation1 * BoneB.GetRotation()).GetNormalized());

		if (SecondaryAxisWeight > SMALL_NUMBER)
		{
			Axis = BoneB.TransformVectorNoScale(SecondaryAxis);
			FVector Target2 = BoneBLocation - (Effector.GetLocation() + BoneA.GetLocation()) * 0.5f;
			if (!Target2.IsNearlyZero() && !Axis.IsNearlyZero())
			{
				Target2 = Target2 - FVector::DotProduct(Target2, Target1) * Target1;
				Target2 = Target2.GetSafeNormal();

				FQuat Rotation2 = FQuat::FindBetweenNormals(Axis, Target2);
				if (!FMath::IsNearlyEqual(SecondaryAxisWeight, 1.f))
				{
					FVector RotationAxis = Rotation2.GetRotationAxis();
					float RotationAngle = Rotation2.GetAngle();
					Rotation2 = FQuat(RotationAxis, RotationAngle * FMath::Clamp<float>(SecondaryAxisWeight, 0.f, 1.f));
				}
				BoneB.SetRotation((Rotation2 * BoneB.GetRotation()).GetNormalized());
			}
		}
	}
}

FVector FControlRigMathLibrary::ClampSpatially(const FVector& Value, EAxis::Type Axis, EControlRigClampSpatialMode::Type Type, float Minimum, float Maximum, FTransform Space)
{
	FVector Local = Space.InverseTransformPosition(Value);

	switch (Type)
	{
		case EControlRigClampSpatialMode::Plane:
		{
			switch (Axis)
			{
				case EAxis::X:
				{
					Local.X = FMath::Clamp<float>(Local.X, Minimum, Maximum);
					break;
				}
				case EAxis::Y:
				{
					Local.Y = FMath::Clamp<float>(Local.Y, Minimum, Maximum);
					break;
				}
				default:
				{
					Local.Z = FMath::Clamp<float>(Local.Z, Minimum, Maximum);
					break;
				}
			}
			break;
		}
		case EControlRigClampSpatialMode::Cylinder:
		{
			switch (Axis)
			{
				case EAxis::X:
				{
					FVector OnPlane = Local * FVector(0.f, 1.f, 1.f);
					if (!OnPlane.IsNearlyZero())
					{
						float Length = OnPlane.Size();
						OnPlane = OnPlane * FMath::Clamp<float>(Length, Minimum, Maximum) / Length;
						Local.Y = OnPlane.Y;
						Local.Z = OnPlane.Z;
					}
					break;
				}
				case EAxis::Y:
				{
					FVector OnPlane = Local * FVector(1.f, 0.f, 1.f);
					if (!OnPlane.IsNearlyZero())
					{
						float Length = OnPlane.Size();
						OnPlane = OnPlane * FMath::Clamp<float>(Length, Minimum, Maximum) / Length;
						Local.X = OnPlane.X;
						Local.Z = OnPlane.Z;
					}
					break;
				}
				default:
				{
					FVector OnPlane = Local * FVector(1.f, 1.f, 0.f);
					if (!OnPlane.IsNearlyZero())
					{
						float Length = OnPlane.Size();
						OnPlane = OnPlane * FMath::Clamp<float>(Length, Minimum, Maximum) / Length;
						Local.X = OnPlane.X;
						Local.Y = OnPlane.Y;
					}
					break;
				}
			}
			break;
		}
		default:
		case EControlRigClampSpatialMode::Sphere:
		{
			if (!Local.IsNearlyZero())
			{
				float Length = Local.Size();
				Local = Local * FMath::Clamp<float>(Length, Minimum, Maximum) / Length;
			}
			break;
		}

	}

	return Space.TransformPosition(Local);
}

FQuat FControlRigMathLibrary::FindQuatBetweenVectors(const FVector& A, const FVector& B)
{
	return FindQuatBetweenNormals(A.GetSafeNormal(), B.GetSafeNormal());
}

FQuat FControlRigMathLibrary::FindQuatBetweenNormals(const FVector& A, const FVector& B)
{
	const FQuat::FReal Dot = FVector::DotProduct(A, B);
	FQuat::FReal W = 1 + Dot;
	FQuat Result;

	if (W < SMALL_NUMBER)
	{
		// A and B point in opposite directions
		W = 2 - W;
		Result = FQuat(
			-A.Y * B.Z + A.Z * B.Y,
			-A.Z * B.X + A.X * B.Z,
			-A.X * B.Y + A.Y * B.X,
			W).GetNormalized();

		const FVector Normal = FMath::Abs(A.X) > FMath::Abs(A.Y) ?
			FVector(0.f, 1.f, 0.f) : FVector(1.f, 0.f, 0.f);
		const FVector BiNormal = FVector::CrossProduct(A, Normal);
		const FVector TauNormal = FVector::CrossProduct(A, BiNormal);
		Result = Result * FQuat(TauNormal, PI);
	}
	else
	{
		//Axis = FVector::CrossProduct(A, B);
		Result = FQuat(
			A.Y * B.Z - A.Z * B.Y,
			A.Z * B.X - A.X * B.Z,
			A.X * B.Y - A.Y * B.X,
			W);
	}

	Result.Normalize();
	return Result;
}

FVector FControlRigMathLibrary::GetEquivalentEulerAngle(const FVector& InEulerAngle, const EEulerRotationOrder& InOrder)
{
	FVector TheOtherAngle;

	switch (InOrder)
	{
	case EEulerRotationOrder::XYZ:
		TheOtherAngle.X = InEulerAngle.X + 180;
		TheOtherAngle.Y = 180 - InEulerAngle.Y;
		TheOtherAngle.Z = InEulerAngle.Z + 180;
		break;
	case EEulerRotationOrder::XZY:
		TheOtherAngle.X = InEulerAngle.X + 180;
		TheOtherAngle.Z = 180 - InEulerAngle.Z;
		TheOtherAngle.Y = InEulerAngle.Y + 180;
		break;
	case EEulerRotationOrder::YXZ:
		TheOtherAngle.Y = InEulerAngle.Y + 180;
		TheOtherAngle.X = 180 - InEulerAngle.X;
		TheOtherAngle.Z = InEulerAngle.Z + 180;
		break;
	case EEulerRotationOrder::YZX:
		TheOtherAngle.Y = InEulerAngle.Y + 180;
		TheOtherAngle.Z = 180 - InEulerAngle.Z;
		TheOtherAngle.X = InEulerAngle.X + 180;
		break;
	case EEulerRotationOrder::ZXY:
		TheOtherAngle.Z = InEulerAngle.Z + 180;
		TheOtherAngle.X = 180 - InEulerAngle.X;
		TheOtherAngle.Y = InEulerAngle.Y + 180;
		break;
	case EEulerRotationOrder::ZYX:
		TheOtherAngle.Z = InEulerAngle.Z + 180;
		TheOtherAngle.Y = 180 - InEulerAngle.Y;
		TheOtherAngle.X = InEulerAngle.X + 180;
		break;
	};

	TheOtherAngle.UnwindEuler();
	
	return TheOtherAngle;
}

FVector& FControlRigMathLibrary::ChooseBetterEulerAngleForAxisFilter(const FVector& Base, FVector& A, FVector& B)
{
	// simply compare the sum of difference should be enough
	float Diff1 = (A - Base).GetAbs().Dot(FVector::OneVector);
	float Diff2 = (B - Base).GetAbs().Dot(FVector::OneVector);

	return Diff1 < Diff2 ? A : B;
}

