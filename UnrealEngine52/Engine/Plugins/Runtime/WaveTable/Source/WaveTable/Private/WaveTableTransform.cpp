// Copyright Epic Games, Inc. All Rights Reserved.
#include "WaveTableTransform.h"

#include "Curves/CurveFloat.h"
#include "DSP/FloatArrayMath.h"
#include "WaveTableImporter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaveTableTransform)

void FWaveTableTransform::Apply(float& InOutValue, bool bInBipolar) const
{
	if (WaveTable.IsEmpty())
	{
		float DummyVal = 0.f;
		TArrayView<float> ValueView = { &InOutValue, 1 };
		bInBipolar ? SampleCurveBipolar(ValueView, DummyVal) : SampleCurveUnipolar(ValueView, DummyVal);
	}
	else
	{
		InOutValue *= WaveTable.Num(); // Convert to float index
		TArrayView<float> ValueView = { &InOutValue, 1 };
		WaveTable::FWaveTableSampler::Interpolate(WaveTable, ValueView);
	}
}

void FWaveTableTransform::SampleCurveBipolar(TArrayView<float> InOutTable, float& OutFinalValue) const
{
	// Clamp the input
	Audio::ArrayClampInPlace(InOutTable, 0.0f, 1.0f);

	float* ValueData = InOutTable.GetData();
	switch (Curve)
	{
		case EWaveTableCurve::Custom:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = CurveCustom.Eval(ValueData[i]);
			}
				
			OutFinalValue = CurveCustom.Eval(1.f);
			break;
		}

		case EWaveTableCurve::Shared:
		{
			if (CurveShared)
			{
				for (int32 i = 0; i < InOutTable.Num(); ++i)
				{
					ValueData[i] = CurveShared->FloatCurve.Eval(ValueData[i]);
				}
				
				OutFinalValue = CurveShared->FloatCurve.Eval(1.f);
			}
			break;
		}

		case EWaveTableCurve::Linear_Inv:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = FMath::Lerp(1.0f, -1.0f, ValueData[i]);
			}
				
			OutFinalValue = -1.f;
		}
		break;

		case EWaveTableCurve::Linear:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = FMath::Lerp(-1.0f, 1.0f, ValueData[i]);
			}
			
			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::Exp:
		{
			// Alpha is limited to between 0.0f and 1.0f and ExponentialScalar
			// between 0 and 10 to keep ValueData "sane" and avoid float boundary.
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] *= (FMath::Pow(10.0f, Scalar * (ValueData[i] - 1.0f)));
				ValueData[i] -= 0.5f;
			}

			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::Exp_Inverse:
		{
			// Alpha is limited to between 0.0f and 1.0f and ExponentialScalar
			// between 0 and 10 to keep ValueData "sane" and avoid float boundary.
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = ((ValueData[i] - 1.0f) * FMath::Pow(10.0f, -1.0f * Scalar * ValueData[i])) + 1.0f;
				ValueData[i] -= 0.5f;
			}

			OutFinalValue = -1.f;
		}
		break;

		case EWaveTableCurve::Log:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = (Scalar * FMath::LogX(10.0f, ValueData[i])) + 0.5f;
			}
				
			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::Sin:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = FMath::Sin(HALF_PI * ValueData[i]) - 0.5f;
			}

			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::SCurve:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = 0.5f * FMath::Sin(((PI * ValueData[i]) - HALF_PI));
			}

			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::Sin_Full:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = FMath::Sin(2.0f * PI * ValueData[i]);
			}

			OutFinalValue = 0.f;
		}
		break;

		case EWaveTableCurve::File:
		{
			if (WaveTable.IsEmpty())
			{
				Audio::ArraySetToConstantInplace(InOutTable, 0.0f);
				OutFinalValue = 0.f;
			}
			else
			{
				constexpr auto InterpMode = WaveTable::FWaveTableSampler::EInterpolationMode::MaxValue;
				WaveTable::FWaveTableSampler::Interpolate(WaveTable, InOutTable, InterpMode);
				OutFinalValue = WaveTable.Last();
			}
		}
		break;

		default:
		{
			static_assert(static_cast<int32>(EWaveTableCurve::Count) == 11, "Possible missing case coverage for output curve.");
		}
		break;
	}

	Audio::ArrayClampInPlace(InOutTable, -1.0f, 1.0f);
}

void FWaveTableTransform::SampleCurveUnipolar(TArrayView<float> InOutTable, float& OutFinalValue) const
{
	// Clamp the input
	Audio::ArrayClampInPlace(InOutTable, 0.0f, 1.0f);

	float* ValueData = InOutTable.GetData();
	switch (Curve)
	{
		case EWaveTableCurve::Custom:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = CurveCustom.Eval(ValueData[i]);
			}
				
			OutFinalValue = CurveCustom.Eval(1.f);
			break;
		}

		case EWaveTableCurve::Shared:
		{
			if (CurveShared)
			{
				for (int32 i = 0; i < InOutTable.Num(); ++i)
				{
					ValueData[i] = CurveShared->FloatCurve.Eval(ValueData[i]);
				}
				OutFinalValue = CurveShared->FloatCurve.Eval(1.f);
			}
			break;
		}

		case EWaveTableCurve::Linear_Inv:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = 1.0f - InOutTable[i];
			}
				
			OutFinalValue = 0.f;
		}
		break;

		case EWaveTableCurve::Linear:
		{
			// Do nothing, as output values == clamped input values
			OutFinalValue = 1.f;
		}
		break;
	
		case EWaveTableCurve::Exp:
		{
			// Alpha is limited to between 0.0f and 1.0f and ExponentialScalar
			// between 0 and 10 to keep ValueData[i]s "sane" and avoid float boundary.
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] *= (FMath::Pow(10.0f, Scalar * (ValueData[i] - 1.0f)));
			}

			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::Exp_Inverse:
		{
			// Alpha is limited to between 0.0f and 1.0f and ExponentialScalar
			// between 0 and 10 to keep ValueData[i]s "sane" and avoid float boundary.
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = ((ValueData[i] - 1.0f) * FMath::Pow(10.0f, -1.0f * Scalar * ValueData[i])) + 1.0f;
			}

			OutFinalValue = 0.f;
		}
		break;

		case EWaveTableCurve::Log:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = (Scalar * FMath::LogX(10.0f, ValueData[i])) + 1.0f;
			}

			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::Sin:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = FMath::Sin(HALF_PI * ValueData[i]);
			}
				
			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::SCurve:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = 0.5f * FMath::Sin(((PI * ValueData[i]) - HALF_PI)) + 0.5f;
			}

			OutFinalValue = 1.f;
		}
		break;

		case EWaveTableCurve::Sin_Full:
		{
			for (int32 i = 0; i < InOutTable.Num(); ++i)
			{
				ValueData[i] = 0.5f * FMath::Sin(2.0f * PI * ValueData[i]) + 0.5f;
			}

			OutFinalValue = 0.f;
		}
		break;

		case EWaveTableCurve::File:
		{
			if (WaveTable.IsEmpty())
			{
				Audio::ArraySetToConstantInplace(InOutTable, 0.0f);
				OutFinalValue = 0.f;
			}
			else
			{
				constexpr auto InterpMode = WaveTable::FWaveTableSampler::EInterpolationMode::MaxValue;
				WaveTable::FWaveTableSampler::Interpolate(WaveTable, InOutTable, InterpMode);
				OutFinalValue = WaveTable.Last();
			}
		}
		break;

		default:
		{
			static_assert(static_cast<int32>(EWaveTableCurve::Count) == 11, "Possible missing case coverage for output curve.");
		}
		break;
	}

	Audio::ArrayClampInPlace(InOutTable, 0.0f, 1.0f);
}

void FWaveTableTransform::CacheCurve()
{
	if (Curve == EWaveTableCurve::Shared && CurveShared)
	{
		CurveCustom = CurveShared->FloatCurve;
	}

	// Always clear shared curve regardless to avoid
	// holding a reference to the UObject when never
	// expected to be used.
	CurveShared = nullptr;
}

float FWaveTableTransform::GetFinalValue() const
{
	if (FMath::IsNearlyEqual(FinalValue, WaveTable::InvalidWaveTableValue))
	{
		if (WaveTable.IsEmpty())
		{
			return 0.f;
		}

		return WaveTable.Last();
	}

	return FinalValue;
}

void FWaveTableTransform::SetFinalValue(float InValue)
{
	FinalValue = InValue;
}

#if WITH_EDITOR
void FWaveTableTransform::CopyToWaveTable(TArrayView<float> InOutTable, float& OutFinalValue, bool bInBipolar) const
{
	const float Delta = 1.0f / InOutTable.Num();
	
	for (int32 i = 0; i < InOutTable.Num(); ++i)
	{
		InOutTable[i] = i * Delta;
	}

	bInBipolar ? SampleCurveBipolar(InOutTable, OutFinalValue) : SampleCurveUnipolar(InOutTable, OutFinalValue);
}

void FWaveTableTransform::CreateWaveTable(TArray<float>& InOutTable, float& OutFinalValue, bool bInBipolar) const
{
	switch (Curve)
	{
		case EWaveTableCurve::File:
		{
			WaveTable::FImporter Importer(WaveTableSettings, bInBipolar);
			Importer.Process(InOutTable);
		}
		break;

		case EWaveTableCurve::Custom:
		case EWaveTableCurve::Shared:
		case EWaveTableCurve::Exp:
		case EWaveTableCurve::Exp_Inverse:
		case EWaveTableCurve::Linear:
		case EWaveTableCurve::Linear_Inv:
		case EWaveTableCurve::Log:
		case EWaveTableCurve::SCurve:
		case EWaveTableCurve::Sin:
		case EWaveTableCurve::Sin_Full:
		default:
		{
			CopyToWaveTable(InOutTable, OutFinalValue, bInBipolar);
		}
		break;
	}
}
#endif // WITH_EDITOR
