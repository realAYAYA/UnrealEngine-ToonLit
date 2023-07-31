// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputModifiers.h"
#include "Curves/CurveFloat.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputModifiers)

#define LOCTEXT_NAMESPACE "EnhancedInputModifiers"

/*
* Scalar
*/

#if WITH_EDITOR
EDataValidationResult UInputModifierScalar::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(ValidationErrors), EDataValidationResult::Valid);

	// You cannot scale a boolean value
	if (UInputAction* IA = Cast<UInputAction>(GetOuter()))
	{
		if (IA->ValueType == EInputActionValueType::Boolean)
		{
			Result = EDataValidationResult::Invalid;
			ValidationErrors.Add(LOCTEXT("InputScalarInvalidActionType", "A Scalar modifier cannot be used on a 'Boolean' input action"));
		}
	}
	
	return Result;
}
#endif

FInputActionValue UInputModifierScalar::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	// Don't try and scale bools
	if (ensureMsgf(CurrentValue.GetValueType() != EInputActionValueType::Boolean, TEXT("Scale modifier doesn't support boolean values.")))
	{
		return CurrentValue.Get<FVector>() * Scalar;
	}
	return CurrentValue;
}

/*
* Scale by Delta Time
*/

FInputActionValue UInputModifierScaleByDeltaTime::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	// Don't try and scale bools
	if (ensureMsgf(CurrentValue.GetValueType() != EInputActionValueType::Boolean, TEXT("Scale By Delta Time modifier doesn't support boolean values.")))
	{
		return CurrentValue.Get<FVector>() * DeltaTime;
	}
	return CurrentValue;
};


/*
* Negate
*/

FInputActionValue UInputModifierNegate::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	return CurrentValue.Get<FVector>() * FVector(bX ? -1.f : 1.f, bY ? -1.f : 1.f, bZ ? -1.f : 1.f);
}


FLinearColor UInputModifierNegate::GetVisualizationColor_Implementation(FInputActionValue SampleValue, FInputActionValue FinalValue) const
{
	FVector Sample = SampleValue.Get<FVector>();
	FVector Final = FinalValue.Get<FVector>();
	return FLinearColor(Sample.X != Final.X ? 1.f : 0.f, Sample.Y != Final.Y ? 1.f : 0.f, Sample.Z != Final.Z ? 1.f : 0.f);
}


/*
* Dead zones
*/

FInputActionValue UInputModifierDeadZone::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	EInputActionValueType ValueType = CurrentValue.GetValueType();
	if (ValueType == EInputActionValueType::Boolean)
	{
		return CurrentValue;
	}

	auto DeadZoneLambda = [this](const float AxisVal)
	{
		// We need to translate and scale the input to the +/- 1 range after removing the dead zone.
		return FMath::Min(1.f, (FMath::Max(0.f, FMath::Abs(AxisVal) - LowerThreshold) / (UpperThreshold - LowerThreshold))) * FMath::Sign(AxisVal);
	};

	FVector NewValue = CurrentValue.Get<FVector>();
	switch (Type)
	{
	case EDeadZoneType::Axial:
		NewValue.X = DeadZoneLambda(NewValue.X);
		NewValue.Y = DeadZoneLambda(NewValue.Y);
		NewValue.Z = DeadZoneLambda(NewValue.Z);
		break;
	case EDeadZoneType::Radial:
		if (ValueType == EInputActionValueType::Axis3D)
		{
			NewValue = NewValue.GetSafeNormal() * DeadZoneLambda(NewValue.Size());
		}
		else if (ValueType == EInputActionValueType::Axis2D)
		{
			NewValue = NewValue.GetSafeNormal2D() * DeadZoneLambda(NewValue.Size2D());
		}
		else
		{
			NewValue.X = DeadZoneLambda(NewValue.X);
		}
		break;
	}

	return NewValue;
};

FLinearColor UInputModifierDeadZone::GetVisualizationColor_Implementation(FInputActionValue SampleValue, FInputActionValue FinalValue) const
{
	// Visualize as black when unmodified. Red when blocked (with differing intensities to indicate axes)
	// Mirrors visualization in https://www.gamasutra.com/blogs/JoshSutphin/20130416/190541/Doing_Thumbstick_Dead_Zones_Right.php.
	if (FinalValue.GetValueType() == EInputActionValueType::Boolean || FinalValue.GetValueType() == EInputActionValueType::Axis1D)
	{
		return FLinearColor(FinalValue.Get<float>() == 0.f ? 1.f : 0.f, 0.f, 0.f);
	}
	return FLinearColor((FinalValue.Get<FVector2D>().X == 0.f ? 0.5f : 0.f) + (FinalValue.Get<FVector2D>().Y == 0.f ? 0.5f : 0.f), 0.f, 0.f);
}


/*
* Smooth
*/

void UInputModifierSmooth::ClearSmoothedAxis()
{
	ZeroTime = 0.f;
	AverageValue.Reset();
	Samples = 0;
	TotalSampleTime = SMOOTH_TOTAL_SAMPLE_TIME_DEFAULT;
}

FInputActionValue UInputModifierSmooth::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	uint8 SampleCount = 1;//KeyState->SampleCountAccumulator;	// TODO: Need access to axis sample count accumulator here.

	// TODO: This could be fired multiple times if modifiers are badly set up, breaking sample count/deltatime updates.

	if(AverageValue.GetMagnitudeSq() != 0.f)
	{
		TotalSampleTime += DeltaTime;
		Samples += SampleCount;
	}

	if (DeltaTime < 0.25f)
	{
		if (Samples > 0 && TotalSampleTime > 0.0f)
		{
			// this is seconds/sample
			const float AxisSamplingTime = TotalSampleTime / Samples;
			check(AxisSamplingTime > 0.0f);

			if (CurrentValue.GetMagnitudeSq() && SampleCount > 0)
			{
				ZeroTime = 0.0f;
				if (AverageValue.GetMagnitudeSq())
				{
					// this isn't the first tick with non-zero mouse movement
					if (DeltaTime < AxisSamplingTime * (SampleCount + 1))
					{
						// smooth mouse movement so samples/tick is constant
						CurrentValue *=  DeltaTime / (AxisSamplingTime * SampleCount);
						SampleCount = 1;
					}
				}

				AverageValue = CurrentValue * (1.f / SampleCount);
			}
			else
			{
				// no mouse movement received
				if (ZeroTime < AxisSamplingTime)
				{
					// zero mouse movement is possibly because less than the mouse sampling interval has passed
					CurrentValue = AverageValue.ConvertToType(CurrentValue) * (DeltaTime / AxisSamplingTime);
				}
				else
				{
					ClearSmoothedAxis();
				}

				ZeroTime += DeltaTime;		// increment length of time we've been at zero
			}
		}
	}
	else
	{
		// if we had an abnormally long frame, clear everything so it doesn't distort the results
		ClearSmoothedAxis();
	}

	// TODO: FortPlayerInput clears the sample count accumulator here!
	//KeyState->SampleCountAccumulator = 0;

	return CurrentValue;
}

/*
* Response curves
*/

FInputActionValue UInputModifierResponseCurveExponential::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	FVector ResponseValue = CurrentValue.Get<FVector>();
	switch (CurrentValue.GetValueType())
	{
	case EInputActionValueType::Axis3D:
		ResponseValue.Z = CurveExponent.Z != 1.f ? FMath::Sign(ResponseValue.Z) * FMath::Pow(FMath::Abs(ResponseValue.Z), CurveExponent.Z) : ResponseValue.Z;
		//[[fallthrough]];
	case EInputActionValueType::Axis2D:
		ResponseValue.Y = CurveExponent.Y != 1.f ? FMath::Sign(ResponseValue.Y) * FMath::Pow(FMath::Abs(ResponseValue.Y), CurveExponent.Y) : ResponseValue.Y;
		//[[fallthrough]];
	case EInputActionValueType::Axis1D:
		ResponseValue.X = CurveExponent.X != 1.f ? FMath::Sign(ResponseValue.X) * FMath::Pow(FMath::Abs(ResponseValue.X), CurveExponent.X) : ResponseValue.X;
		break;
	}
	return ResponseValue;
};

FInputActionValue UInputModifierResponseCurveUser::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	FVector ResponseValue = CurrentValue.Get<FVector>();
	switch (CurrentValue.GetValueType())
	{
	case EInputActionValueType::Axis3D:
		ResponseValue.Z = ResponseZ ? ResponseZ->GetFloatValue(ResponseValue.Z) : 0.0f;
		//[[fallthrough]];
	case EInputActionValueType::Axis2D:
		ResponseValue.Y = ResponseY ? ResponseY->GetFloatValue(ResponseValue.Y) : 0.0f;
		//[[fallthrough]];
	case EInputActionValueType::Axis1D:
	case EInputActionValueType::Boolean:
		ResponseValue.X = ResponseX ? ResponseX->GetFloatValue(ResponseValue.X) : 0.0f;
		break;
	}
	return ResponseValue;
};

/*
* FOV
*/

FInputActionValue UInputModifierFOVScaling::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	const APlayerController* PC = PlayerInput->GetOuterAPlayerController();

	if (!PC)
	{
		return CurrentValue;
	}

	const float FOVAngle = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : 1.f;
	float Scale = FOVScale;

	switch(FOVScalingType)
	{
	case EFOVScalingType::Standard:
		// TODO: Fortnite falls back to old style FOV scaling for mouse input. Presumably for back compat, but this needs checking.
		if (PC->PlayerCameraManager)
		{
			// This is the proper way to scale based off FOV changes.
			const float kPlayerInput_BaseFOV = 80.0f;
			const float BaseHalfFOV = kPlayerInput_BaseFOV * 0.5f;
			const float HalfFOV = FOVAngle * 0.5f;
			const float BaseTanHalfFOV = FMath::Tan(FMath::DegreesToRadians(BaseHalfFOV));
			const float TanHalfFOV = FMath::Tan(FMath::DegreesToRadians(HalfFOV));

			check(BaseTanHalfFOV > 0.0f);
			Scale *= (TanHalfFOV / BaseTanHalfFOV);
		}
		break;
	case EFOVScalingType::UE4_BackCompat:
		Scale *= FOVAngle;
		break;
	default:
		checkf(false, TEXT("Unsupported FOV scaling type '%s'"), *UEnum::GetValueAsString(TEXT("EnhancedInput.EFovScalingType"), FOVScalingType));
		break;
	}

	return CurrentValue * Scale;
}

/*
* ToWorldSpace axis swizzling
*/

FInputActionValue UInputModifierToWorldSpace::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	FVector Converted = CurrentValue.Get<FVector>();
	switch (CurrentValue.GetValueType())
	{
	case EInputActionValueType::Axis3D:
		// Input Device Z = World Forward (X), Device X = World Right (Y), Device Y = World Up (Z)
		Converted = FVector(Converted.Z, Converted.X, Converted.Y);
		break;
	case EInputActionValueType::Axis2D:
		// Swap axes so Input Device Y axis becomes World Forward (X), Device X becomes World Right (Y)
		Swap(Converted.X, Converted.Y);
		break;
	case EInputActionValueType::Axis1D:
	case EInputActionValueType::Boolean:
		// No conversion required
		break;
	}
	return FInputActionValue(CurrentValue.GetValueType(), Converted);
}

FLinearColor UInputModifierToWorldSpace::GetVisualizationColor_Implementation(FInputActionValue SampleValue, FInputActionValue FinalValue) const
{
	// Draw a cross with X/Y colors inverted (Green on X axis, Red on Y axis)
	const float CrossSize = 0.1f;
	FVector Sample = SampleValue.Get<FVector>();
	// Draw arrows at the ends for aesthetics
	const float ArrowStart = 0.8f;
	const float ArrowOffset = 1.f - (1.f - ArrowStart) * 0.5f;
	const float ArrowX = Sample.Y <= -ArrowStart ? ArrowOffset + Sample.Y : (Sample.Y >= 0.95f ? -CrossSize : 0.f);	// At -ve end
	const float ArrowY = Sample.X >= ArrowStart ? ArrowOffset - Sample.X : (Sample.X <= -0.95f ? -CrossSize : 0.f);	// At +ve end
	return FLinearColor(FMath::Abs(Sample.X) <= CrossSize + ArrowX ? 1.f : 0.f, FMath::Abs(Sample.Y) <= CrossSize + ArrowY ? 1.f : 0.f, 0.f);
}

/*
* Generic Axis swizzling
*/ 

FInputActionValue UInputModifierSwizzleAxis::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	FVector Value = CurrentValue.Get<FVector>();
	switch (Order)
	{
	case EInputAxisSwizzle::YXZ:
		Swap(Value.X, Value.Y);
		break;
	case EInputAxisSwizzle::ZYX:
		Swap(Value.X, Value.Z);
		break;
	case EInputAxisSwizzle::XZY:
		Swap(Value.Y, Value.Z);
		break;
	case EInputAxisSwizzle::YZX:
		Value = FVector(Value.Y, Value.Z, Value.X);
		break;
	case EInputAxisSwizzle::ZXY:
		Value = FVector(Value.Z, Value.X, Value.Y);
		break;
	}
	return FInputActionValue(CurrentValue.GetValueType(), Value);
}

FLinearColor UInputModifierSwizzleAxis::GetVisualizationColor_Implementation(FInputActionValue SampleValue, FInputActionValue FinalValue) const
{
	// Blend Red to Green
	// TODO: Color blend per swizzle type?
	float SampleX = (FMath::Abs(SampleValue.Get<float>()) + 1.f) * 0.5f;
	return FLinearColor(SampleX, 1.f - SampleX, 0.f);
}

#undef LOCTEXT_NAMESPACE
