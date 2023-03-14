// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Simulation/RigUnit_Kalman.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Kalman)

FRigUnit_KalmanFloat_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(BufferSize <= 0)
	{
		if (Context.State == EControlRigState::Init)
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("BufferSize is too small."));
		}

		Result = Value;
		return;
	}

	Buffer.SetNum(BufferSize);

	if (Context.State == EControlRigState::Init)
	{
		for (float& Element : Buffer)
		{
			Element = FLT_MAX;
			LastInsertIndex = -1;
		}
	}
	else
	{
		if(LastInsertIndex == Buffer.Num() - 1)
		{
			LastInsertIndex = -1;
		}
		Buffer[++LastInsertIndex] = Value;

		Result = 0.f;
		int32 NumberValidEntries = 0;
		for(const float F : Buffer)
		{
			if (F == FLT_MAX)
			{
				break;
			}
			Result += F;
			NumberValidEntries++;
		}

		if (NumberValidEntries > 0)
		{
			Result = Result / float(NumberValidEntries);
		}
	}
}

FRigUnit_KalmanVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(BufferSize <= 0)
	{
		if (Context.State == EControlRigState::Init)
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("BufferSize is too small."));
		}

		Result = Value;
		return;
	}

	Buffer.SetNum(BufferSize);

	if (Context.State == EControlRigState::Init)
	{
		for (FVector& Element : Buffer)
		{
			Element.X = FLT_MAX;
			LastInsertIndex = -1;
		}
	}
	else
	{
		if(LastInsertIndex == Buffer.Num() - 1)
		{
			LastInsertIndex = -1;
		}
		Buffer[++LastInsertIndex] = Value;

		Result = FVector::ZeroVector;
		int32 NumberValidEntries = 0;
		for (const FVector& F : Buffer)
		{
			if (F.X == FLT_MAX)
			{
				break;
			}
			Result += F;
			NumberValidEntries++;
		}

		if (NumberValidEntries > 0)
		{
			Result = Result / float(NumberValidEntries);
		}
	}
}

FRigUnit_KalmanTransform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if(BufferSize <= 0)
	{
		if (Context.State == EControlRigState::Init)
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("BufferSize is too small."));
		}

		Result = Value;
		return;
	}

	Buffer.SetNum(BufferSize);

	if (Context.State == EControlRigState::Init)
	{
		for (FTransform& Element : Buffer)
		{
			Element.SetTranslation(FVector(FLT_MAX));
			LastInsertIndex = -1;
		}
	}
	else
	{
		if(LastInsertIndex == Buffer.Num() - 1)
		{
			LastInsertIndex = -1;
		}
		Buffer[++LastInsertIndex] = Value;

		FVector Location = FVector::ZeroVector;
		FVector AxisX = FVector::ZeroVector;
		FVector AxisY = FVector::ZeroVector;
		FVector Scale = FVector::ZeroVector;
		
		int32 NumberValidEntries = 0;

		for(const FTransform& F : Buffer)
		{
			if (F.GetLocation().X == FLT_MAX)
			{
				break;
			}
			Location += F.GetLocation();
			AxisX += F.TransformVectorNoScale(FVector(1.f, 0.f, 0.f));
			AxisY += F.TransformVectorNoScale(FVector(0.f, 1.f, 0.f));
			Scale += F.GetScale3D();
			NumberValidEntries++;
		}

		if (NumberValidEntries > 0)
		{
			Location = Location / float(NumberValidEntries);
			AxisX = (AxisX / float(NumberValidEntries)).GetSafeNormal();
			AxisY = (AxisY / float(NumberValidEntries)).GetSafeNormal();
			Scale = Scale / float(NumberValidEntries);
		}

		Result.SetLocation(Location);
		Result.SetRotation(FRotationMatrix::MakeFromXY(AxisX, AxisY).ToQuat());
		Result.SetScale3D(Scale);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Units/RigUnitTest.h"

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_KalmanFloat)
{
	Unit.Value = 1.f;
	Unit.BufferSize = 4;
	InitAndExecute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Result, 1.f), TEXT("unexpected average result"));
	InitAndExecute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Result, 1.f), TEXT("unexpected average result"));
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Result, 1.f), TEXT("unexpected average result"));
	Unit.Value = 4.f;
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Result, 2.f), TEXT("unexpected average result"));
	Unit.Value = 6.f;
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Result, 3.f), TEXT("unexpected average result"));
	Unit.Value = 5.f;
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Result, 4.f), TEXT("unexpected average result"));
	return true;
}

#endif
