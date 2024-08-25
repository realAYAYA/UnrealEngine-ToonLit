// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorOscillate.h"

#include "Properties/PropertyAnimatorFloatContext.h"
#include "PropertyAnimatorShared.h"

UPropertyAnimatorOscillate::UPropertyAnimatorOscillate()
{
	SetAnimatorDisplayName(DefaultControllerName);
}

void UPropertyAnimatorOscillate::SetOscillateFunction(EPropertyAnimatorOscillateFunction InFunction)
{
	OscillateFunction = InFunction;
}

float UPropertyAnimatorOscillate::Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const
{
	double WaveResult = 0.f;

	using namespace UE::PropertyAnimator;

	const float Frequency = InOptions->GetFrequency() * GlobalFrequency;

	switch (OscillateFunction)
	{
		case EPropertyAnimatorOscillateFunction::Sine:
			WaveResult = Wave::Sine(InTimeElapsed, 1.f, Frequency, InOptions->GetTimeOffset());
		break;
		case EPropertyAnimatorOscillateFunction::Cosine:
			WaveResult = Wave::Cosine(InTimeElapsed, 1.f, Frequency, InOptions->GetTimeOffset());
		break;
		case EPropertyAnimatorOscillateFunction::Square:
			WaveResult = Wave::Square(InTimeElapsed, 1.f, Frequency, InOptions->GetTimeOffset());
		break;
		case EPropertyAnimatorOscillateFunction::InvertedSquare:
			WaveResult = Wave::InvertedSquare(InTimeElapsed, 1.f, Frequency, InOptions->GetTimeOffset());
		break;
		case EPropertyAnimatorOscillateFunction::Sawtooth:
			WaveResult = Wave::Sawtooth(InTimeElapsed, 1.f, Frequency, InOptions->GetTimeOffset());
		break;
		case EPropertyAnimatorOscillateFunction::Triangle:
			WaveResult = Wave::Triangle(InTimeElapsed, 1.f, Frequency, InOptions->GetTimeOffset());
		break;
		default:
			checkNoEntry();
	}

	// Remap from [-1, 1] to user amplitude from [Min, Max]
	return FMath::GetMappedRangeValueClamped(FVector2D(-1, 1), FVector2D(InOptions->GetAmplitudeMin(), InOptions->GetAmplitudeMax()), WaveResult);
}
