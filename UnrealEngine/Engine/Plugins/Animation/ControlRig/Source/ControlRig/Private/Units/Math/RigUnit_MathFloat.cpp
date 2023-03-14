// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_MathFloat.h"
#include "Units/RigUnitContext.h"
#include "Units/Core/RigUnit_CoreDispatch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_MathFloat)

FRigUnit_MathFloatConstPi_Execute()
{
	Value = PI;
}

FRigUnit_MathFloatConstHalfPi_Execute()
{
	Value = HALF_PI;
}

FRigUnit_MathFloatConstTwoPi_Execute()
{
	Value = PI * 2.f;
}

FRigUnit_MathFloatConstE_Execute()
{
	Value = EULERS_NUMBER;
}

FRigUnit_MathFloatAdd_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A + B;
}

FRigUnit_MathFloatSub_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A - B;
}

FRigUnit_MathFloatMul_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A * B;
}

FRigUnit_MathFloatDiv_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(FMath::IsNearlyZero(B))
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B is nearly 0.f"));
		Result = 0.f;
		return;
	}
	Result = A / B;
}

FRigUnit_MathFloatMod_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(FMath::IsNearlyZero(B) || B < 0.f)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B needs to be greater than 0"));
		Result = 0.f;
		return;
	}
	Result = FMath::Fmod(A, B);
}

FRigUnit_MathFloatMin_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Min<float>(A, B);
}

FRigUnit_MathFloatMax_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Max<float>(A, B);
}

FRigUnit_MathFloatPow_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Pow(A, B);
}

FRigUnit_MathFloatSqrt_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Value < 0.f)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is below zero"));
		Result = 0.f;
		return;
	}
	Result = FMath::Sqrt(Value);
}

FRigUnit_MathFloatNegate_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = -Value;
}

FRigUnit_MathFloatAbs_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Abs(Value);
}

FRigUnit_MathFloatFloor_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::FloorToFloat(Value);
	Int = (int32)Result;
}

FRigUnit_MathFloatCeil_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::CeilToFloat(Value);
	Int = (int32)Result;
}

FRigUnit_MathFloatRound_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::RoundToFloat(Value);
	Int = (int32)Result;
}

FRigUnit_MathFloatToInt_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::FloorToInt(Value);
}

FRigUnit_MathFloatSign_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value < 0.f ? -1.f : 1.f;
}

FRigUnit_MathFloatClamp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Clamp<float>(Value, Minimum, Maximum);
}

FRigUnit_MathFloatLerp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Lerp<float>(A, B, T);
}

FRigUnit_MathFloatRemap_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	float Ratio = 0.f;
	if (FMath::IsNearlyEqual(SourceMinimum, SourceMaximum))
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("The source minimum and maximum are the same."));
	}
	else
	{
		Ratio = (Value - SourceMinimum) / (SourceMaximum - SourceMinimum);
	}
	if (bClamp)
	{
		Ratio = FMath::Clamp<float>(Ratio, 0.f, 1.f);
	}
	Result = FMath::Lerp<float>(TargetMinimum, TargetMaximum, Ratio);
}

FRigUnit_MathFloatEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A == B;
}

FRigVMStructUpgradeInfo FRigUnit_MathFloatEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreEquals::StaticStruct());
}

FRigUnit_MathFloatNotEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A != B;
}

FRigVMStructUpgradeInfo FRigUnit_MathFloatNotEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreNotEquals::StaticStruct());
}

FRigUnit_MathFloatGreater_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A > B;
}

FRigUnit_MathFloatLess_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A < B;
}

FRigUnit_MathFloatGreaterEqual_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A >= B;
}

FRigUnit_MathFloatLessEqual_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A <= B;
}

FRigUnit_MathFloatIsNearlyZero_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Tolerance < 0.f)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Tolerance is below zero"));
	}
	Result = FMath::IsNearlyZero(Value, FMath::Max<float>(Tolerance, SMALL_NUMBER));
}

FRigUnit_MathFloatIsNearlyEqual_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Tolerance < 0.f)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Tolerance is below zero"));
	}
	Result = FMath::IsNearlyEqual(A, B, FMath::Max<float>(Tolerance, SMALL_NUMBER));
}

FRigUnit_MathFloatSelectBool_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Condition ? IfTrue : IfFalse;
}

FRigVMStructUpgradeInfo FRigUnit_MathFloatSelectBool::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_MathFloatDeg_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::RadiansToDegrees(Value);
}

FRigUnit_MathFloatRad_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::DegreesToRadians(Value);
}

FRigUnit_MathFloatSin_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Sin(Value);
}

FRigUnit_MathFloatCos_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Cos(Value);
}

FRigUnit_MathFloatTan_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Tan(Value);
}

FRigUnit_MathFloatAsin_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (!FMath::IsWithinInclusive<float>(Value, -1.f, 1.f))
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is outside of valid range (-1 to 1)"));
		Result = 0.f;
		return;
	}
	Result = FMath::Asin(Value);
}

FRigUnit_MathFloatAcos_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (!FMath::IsWithinInclusive<float>(Value, -1.f, 1.f))
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is outside of valid range (-1 to 1)"));
		Result = 0.f;
		return;
	}
	Result = FMath::Acos(Value);
}

FRigUnit_MathFloatAtan_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Atan(Value);
}

FRigUnit_MathFloatLawOfCosine_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if ((A <= 0.f) || (B <= 0.f) || (C <= 0.f) || (A + B < C) || (A + C < B) || (B + C < A))
	{
		AlphaAngle = BetaAngle = GammaAngle = 0.f;
		bValid = false;
		return;
	}

	GammaAngle = FMath::Acos((A * A + B * B - C * C) / (2.f * A * B));
	BetaAngle = FMath::Acos((A * A + C * C - B * B) / (2.f * A * C));
	AlphaAngle = PI - GammaAngle - BetaAngle;
	bValid = true;
}

FRigUnit_MathFloatExponential_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Exp(Value);
}

