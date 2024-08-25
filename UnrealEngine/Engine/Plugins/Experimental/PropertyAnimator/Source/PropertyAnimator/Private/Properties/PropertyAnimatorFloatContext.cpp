// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/PropertyAnimatorFloatContext.h"

void UPropertyAnimatorFloatContext::SetMagnitude(float InMagnitude)
{
	Magnitude = FMath::Clamp(InMagnitude, 0.f, 1.f);
}

void UPropertyAnimatorFloatContext::SetAmplitudeMin(double InAmplitude)
{
	AmplitudeMin = InAmplitude;
}

void UPropertyAnimatorFloatContext::SetAmplitudeMax(double InAmplitude)
{
	AmplitudeMax = InAmplitude;
}

void UPropertyAnimatorFloatContext::SetFrequency(float InFrequency)
{
	Frequency = FMath::Max(0.f, InFrequency);
}

void UPropertyAnimatorFloatContext::SetTimeOffset(double InTimeOffset)
{
	TimeOffset = InTimeOffset;
}

void UPropertyAnimatorFloatContext::OnAnimatedPropertyLinked()
{
	Super::OnAnimatedPropertyLinked();

#if WITH_EDITOR
	const FPropertyAnimatorCoreData& Property = GetAnimatedProperty();

	const FProperty* LeafProperty = Property.GetLeafProperty();

	checkf(LeafProperty, TEXT("Animated leaf property must be valid"))

	// Assign Min and Max value based on editor meta data available
	if (LeafProperty->IsA<FNumericProperty>())
	{
		if (LeafProperty->HasMetaData(TEXT("ClampMin")))
		{
			AmplitudeMin = LeafProperty->GetFloatMetaData(FName("ClampMin"));
		}
		else if (LeafProperty->HasMetaData(TEXT("UIMin")))
		{
			AmplitudeMin = LeafProperty->GetFloatMetaData(FName("UIMin"));
		}

		if (LeafProperty->HasMetaData(TEXT("ClampMax")))
		{
			AmplitudeMax =  LeafProperty->GetFloatMetaData(FName("ClampMax"));
		}
		else if (LeafProperty->HasMetaData(TEXT("UIMax")))
		{
			AmplitudeMax =  LeafProperty->GetFloatMetaData(FName("UIMax"));
		}
	}
#endif
}
