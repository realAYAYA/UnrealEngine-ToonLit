// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorFloatBase.h"

#include "Properties/PropertyAnimatorFloatContext.h"
#include "Properties/Converters/PropertyAnimatorCoreConverterBase.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"

#if WITH_EDITOR
void UPropertyAnimatorFloatBase::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	static const FName RandomTimeOffsetName = GET_MEMBER_NAME_CHECKED(UPropertyAnimatorFloatBase, bRandomTimeOffset);
	static const FName SeedName = GET_MEMBER_NAME_CHECKED(UPropertyAnimatorFloatBase, Seed);

	if (MemberName == SeedName
		|| MemberName == RandomTimeOffsetName)
	{
		OnSeedChanged();
	}
}
#endif

bool UPropertyAnimatorFloatBase::IsPropertyDirectlySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	return InPropertyData.IsA<FFloatProperty>();
}

bool UPropertyAnimatorFloatBase::IsPropertyIndirectlySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	// Check if a converter supports the conversion
	if (UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
	{
		static const FPropertyBagPropertyDesc AnimatorTypeDesc("", EPropertyBagPropertyType::Float);
		const FPropertyBagPropertyDesc PropertyTypeDesc("", InPropertyData.GetLeafProperty());

		return AnimatorSubsystem->IsConversionSupported(AnimatorTypeDesc, PropertyTypeDesc);
	}

	return false;
}

void UPropertyAnimatorFloatBase::SetGlobalMagnitude(float InMagnitude)
{
	if (FMath::IsNearlyEqual(GlobalMagnitude, InMagnitude))
	{
		return;
	}

	GlobalMagnitude = InMagnitude;
	OnMagnitudeChanged();
}

void UPropertyAnimatorFloatBase::SetGlobalFrequency(float InFrequency)
{
	if (FMath::IsNearlyEqual(GlobalFrequency, InFrequency))
	{
		return;
	}

	GlobalFrequency = InFrequency;
	OnGlobalFrequencyChanged();
}

void UPropertyAnimatorFloatBase::SetAccumulatedTimeOffset(double InOffset)
{
	if (FMath::IsNearlyEqual(AccumulatedTimeOffset, InOffset))
	{
		return;
	}

	AccumulatedTimeOffset = InOffset;
	OnAccumulatedTimeOffsetChanged();
}

void UPropertyAnimatorFloatBase::SetRandomTimeOffset(bool bInOffset)
{
	if (bRandomTimeOffset == bInOffset)
	{
		return;
	}

	bRandomTimeOffset = bInOffset;
	OnSeedChanged();
}

void UPropertyAnimatorFloatBase::SetSeed(int32 InSeed)
{
	if (Seed == InSeed)
	{
		return;
	}

	Seed = InSeed;
	OnSeedChanged();
}

TSubclassOf<UPropertyAnimatorCoreContext> UPropertyAnimatorFloatBase::GetPropertyContextClass(const FPropertyAnimatorCoreData& InUnlinkedProperty)
{
	return UPropertyAnimatorFloatContext::StaticClass();
}

void UPropertyAnimatorFloatBase::EvaluateProperties(const FPropertyAnimatorCoreEvaluationParameters& InParameters)
{
	double AccumulatedTimeOffsetRound = InParameters.TimeElapsed;
	const float AnimatorMagnitude = GlobalMagnitude * InParameters.AnimatorsMagnitude;
	RandomStream = FRandomStream(Seed);

	EvaluateEachLinkedProperty<UPropertyAnimatorFloatContext>([this, &AccumulatedTimeOffsetRound, &AnimatorMagnitude](
		UPropertyAnimatorFloatContext* InOptions
		, const FPropertyAnimatorCoreData& InResolvedProperty
		, FInstancedPropertyBag& InEvaluatedValues)->bool
	{
		const double RandomTimeOffset = bRandomTimeOffset ? RandomStream.GetFraction() : 0;
		AccumulatedTimeOffsetRound += AccumulatedTimeOffset + RandomTimeOffset;

		if (GlobalMagnitude != 0
			&& GlobalFrequency != 0
			&& InOptions->GetMagnitude() != 0
			&& InOptions->GetFrequency() != 0)
		{
			const float EvaluationResult = AnimatorMagnitude
				* InOptions->GetMagnitude()
				* Evaluate(AccumulatedTimeOffsetRound, InResolvedProperty, InOptions);

			const FName DisplayName(InResolvedProperty.GetPathHash());

			InEvaluatedValues.AddProperty(DisplayName, EPropertyBagPropertyType::Float);
			InEvaluatedValues.SetValueFloat(DisplayName, EvaluationResult);

			return true;
		}

		return false;
	});
}

void UPropertyAnimatorFloatBase::OnPropertyLinked(UPropertyAnimatorCoreContext* InLinkedProperty)
{
	Super::OnPropertyLinked(InLinkedProperty);

	const FPropertyAnimatorCoreData& Property = InLinkedProperty->GetAnimatedProperty();
	if (Property.IsA<FFloatProperty>())
	{
		return;
	}

	const UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get();
	if (!AnimatorSubsystem)
	{
		return;
	}

	static const FPropertyBagPropertyDesc AnimatorTypeDesc("", EPropertyBagPropertyType::Float);
	const FPropertyBagPropertyDesc PropertyTypeDesc("", Property.GetLeafProperty());
	const TSet<UPropertyAnimatorCoreConverterBase*> Converters = AnimatorSubsystem->GetSupportedConverters(AnimatorTypeDesc, PropertyTypeDesc);
	check(!Converters.IsEmpty())
	InLinkedProperty->SetConverterClass(Converters.Array()[0]->GetClass());
}
