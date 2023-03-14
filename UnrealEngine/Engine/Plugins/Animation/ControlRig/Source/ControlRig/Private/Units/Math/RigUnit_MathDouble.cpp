// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_MathDouble.h"
#include "Units/RigUnitContext.h"
#include "Units/Core/RigUnit_CoreDispatch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_MathDouble)

FRigUnit_MathDoubleConstPi_Execute()
{
	Value = PI;
}

FRigUnit_MathDoubleConstHalfPi_Execute()
{
	Value = HALF_PI;
}

FRigUnit_MathDoubleConstTwoPi_Execute()
{
	Value = PI * 2.0;
}

FRigUnit_MathDoubleConstE_Execute()
{
	Value = EULERS_NUMBER;
}

FRigUnit_MathDoubleAdd_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A + B;
}

FRigUnit_MathDoubleSub_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A - B;
}

FRigUnit_MathDoubleMul_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A * B;
}

FRigUnit_MathDoubleDiv_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(FMath::IsNearlyZero(B))
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B is nearly 0.0"));
		Result = 0.0;
		return;
	}
	Result = A / B;
}

FRigUnit_MathDoubleMod_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(FMath::IsNearlyZero(B) || B < 0.0)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("B needs to be greater than 0"));
		Result = 0.0;
		return;
	}
	Result = FMath::Fmod(A, B);
}

FRigUnit_MathDoubleMin_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Min<double>(A, B);
}

FRigUnit_MathDoubleMax_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Max<double>(A, B);
}

FRigUnit_MathDoublePow_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Pow(A, B);
}

FRigUnit_MathDoubleSqrt_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Value < 0.0)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is below zero"));
		Result = 0.0;
		return;
	}
	Result = FMath::Sqrt(Value);
}

FRigUnit_MathDoubleNegate_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = -Value;
}

FRigUnit_MathDoubleAbs_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Abs(Value);
}

FRigUnit_MathDoubleFloor_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::FloorToDouble(Value);
	Int = (int32)Result;
}

FRigUnit_MathDoubleCeil_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::CeilToDouble(Value);
	Int = (int32)Result;
}

FRigUnit_MathDoubleRound_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::RoundToDouble(Value);
	Int = (int32)Result;
}

FRigUnit_MathDoubleToInt_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::FloorToInt(Value);
}

FRigUnit_MathDoubleSign_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = Value < 0.0 ? -1.0 : 1.0;
}

FRigUnit_MathDoubleClamp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Clamp<double>(Value, Minimum, Maximum);
}

FRigUnit_MathDoubleLerp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Lerp<double>(A, B, T);
}

FRigUnit_MathDoubleRemap_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	double Ratio = 0.0;
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
		Ratio = FMath::Clamp<double>(Ratio, 0.0, 1.0);
	}
	Result = FMath::Lerp<double>(TargetMinimum, TargetMaximum, Ratio);
}

FRigUnit_MathDoubleEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A == B;
}

FRigVMStructUpgradeInfo FRigUnit_MathDoubleEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreEquals::StaticStruct());
}

FRigUnit_MathDoubleNotEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A != B;
}

FRigVMStructUpgradeInfo FRigUnit_MathDoubleNotEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreNotEquals::StaticStruct());
}

FRigUnit_MathDoubleGreater_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A > B;
}

FRigUnit_MathDoubleLess_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A < B;
}

FRigUnit_MathDoubleGreaterEqual_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A >= B;
}

FRigUnit_MathDoubleLessEqual_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A <= B;
}

FRigUnit_MathDoubleIsNearlyZero_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Tolerance < 0.0)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Tolerance is below zero"));
	}
	Result = FMath::IsNearlyZero(Value, FMath::Max<double>(Tolerance, SMALL_NUMBER));
}

FRigUnit_MathDoubleIsNearlyEqual_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(Tolerance < 0.0)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Tolerance is below zero"));
	}
	Result = FMath::IsNearlyEqual(A, B, FMath::Max<double>(Tolerance, SMALL_NUMBER));
}

FRigUnit_MathDoubleDeg_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::RadiansToDegrees(Value);
}

FRigUnit_MathDoubleRad_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::DegreesToRadians(Value);
}

FRigUnit_MathDoubleSin_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Sin(Value);
}

FRigUnit_MathDoubleCos_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Cos(Value);
}

FRigUnit_MathDoubleTan_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Tan(Value);
}

FRigUnit_MathDoubleAsin_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (!FMath::IsWithinInclusive<double>(Value, -1.0, 1.0))
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is outside of valid range (-1 to 1)"));
		Result = 0.0;
		return;
	}
	Result = FMath::Asin(Value);
}

FRigUnit_MathDoubleAcos_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (!FMath::IsWithinInclusive<double>(Value, -1.0, 1.0))
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Value is outside of valid range (-1 to 1)"));
		Result = 0.0;
		return;
	}
	Result = FMath::Acos(Value);
}

FRigUnit_MathDoubleAtan_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Atan(Value);
}

FRigUnit_MathDoubleLawOfCosine_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if ((A <= 0.0) || (B <= 0.0) || (C <= 0.0) || (A + B < C) || (A + C < B) || (B + C < A))
	{
		AlphaAngle = BetaAngle = GammaAngle = 0.0;
		bValid = false;
		return;
	}

	GammaAngle = FMath::Acos((A * A + B * B - C * C) / (2.0 * A * B));
	BetaAngle = FMath::Acos((A * A + C * C - B * B) / (2.0 * A * C));
	AlphaAngle = PI - GammaAngle - BetaAngle;
	bValid = true;
}

FRigUnit_MathDoubleExponential_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = FMath::Exp(Value);
}

