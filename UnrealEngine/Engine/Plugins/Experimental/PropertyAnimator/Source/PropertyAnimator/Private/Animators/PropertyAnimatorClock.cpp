// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorClock.h"

#include "Internationalization/Regex.h"
#include "Misc/DateTime.h"
#include "Properties/Converters/PropertyAnimatorCoreConverterBase.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"

UPropertyAnimatorClock::UPropertyAnimatorClock()
{
	SetAnimatorDisplayName(DefaultControllerName);

	if (!IsTemplate())
	{
		OnModeChanged();
	}
}

void UPropertyAnimatorClock::SetMode(EPropertyAnimatorClockMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	OnModeChanged();
}

void UPropertyAnimatorClock::SetDisplayFormat(const FString& InDisplayFormat)
{
	DisplayFormat = InDisplayFormat;
}

void UPropertyAnimatorClock::SetCountdownDuration(const FString& InDuration)
{
	if (CountdownDuration == InDuration)
	{
		return;
	}

	CountdownDuration = InDuration;
	OnModeChanged();
}

bool UPropertyAnimatorClock::IsPropertyDirectlySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	return InPropertyData.IsA<FStrProperty>();
}

bool UPropertyAnimatorClock::IsPropertyIndirectlySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	// Check if a converter supports the conversion
	if (UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
	{
		static const FPropertyBagPropertyDesc AnimatorTypeDesc("", EPropertyBagPropertyType::String);
		const FPropertyBagPropertyDesc PropertyTypeDesc("", InPropertyData.GetLeafProperty());

		return AnimatorSubsystem->IsConversionSupported(AnimatorTypeDesc, PropertyTypeDesc);
	}

	return false;
}

void UPropertyAnimatorClock::PostLoad()
{
	Super::PostLoad();

	OnModeChanged();
}

#if WITH_EDITOR
void UPropertyAnimatorClock::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorClock, Mode)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorClock, CountdownDuration))
	{
		OnModeChanged();
	}
}
#endif

void UPropertyAnimatorClock::OnModeChanged()
{
	if (Mode == EPropertyAnimatorClockMode::LocalTime)
	{
		ActiveTimeSpan = FTimespan(FDateTime::Now().GetTicks()) - ElapsedTimeSpan;
	}
	else if (Mode == EPropertyAnimatorClockMode::Countdown)
	{
		ActiveTimeSpan = ParseTime(CountdownDuration);
	}
	else if (Mode == EPropertyAnimatorClockMode::Stopwatch)
	{
		ActiveTimeSpan = FTimespan::Zero();
	}
}

void UPropertyAnimatorClock::EvaluateProperties(const FPropertyAnimatorCoreEvaluationParameters& InParameters)
{
	FString FormattedDateTime;

	if (Mode == EPropertyAnimatorClockMode::Countdown)
	{
		ElapsedTimeSpan = ActiveTimeSpan - FTimespan::FromSeconds(InParameters.TimeElapsed);
		const FDateTime CountdownDateTime(ElapsedTimeSpan > FTimespan::Zero() ? ElapsedTimeSpan.GetTicks() : 0);
		FormattedDateTime = CountdownDateTime.ToFormattedString(*DisplayFormat);
	}
	else if (Mode == EPropertyAnimatorClockMode::Stopwatch || Mode == EPropertyAnimatorClockMode::LocalTime)
	{
		ElapsedTimeSpan = ActiveTimeSpan + FTimespan::FromSeconds(InParameters.TimeElapsed);
		const FDateTime StopWatchDateTime(ElapsedTimeSpan.GetTicks());
		FormattedDateTime = StopWatchDateTime.ToFormattedString(*DisplayFormat);
	}

	EvaluateEachLinkedProperty<UPropertyAnimatorCoreContext>([this, FormattedDateTime](
		UPropertyAnimatorCoreContext* InContext,
		const FPropertyAnimatorCoreData& InResolvedProperty,
		FInstancedPropertyBag& InEvaluatedValues)->bool
	{
		const FName DisplayName(InResolvedProperty.GetPathHash());

		InEvaluatedValues.AddProperty(DisplayName, EPropertyBagPropertyType::String);
		InEvaluatedValues.SetValueString(DisplayName, FormattedDateTime);

		return true;
	});
}

void UPropertyAnimatorClock::OnPropertyLinked(UPropertyAnimatorCoreContext* InLinkedProperty)
{
	Super::OnPropertyLinked(InLinkedProperty);

	const FPropertyAnimatorCoreData& Property = InLinkedProperty->GetAnimatedProperty();
	if (Property.IsA<FStrProperty>())
	{
		return;
	}

	const UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get();
	if (!AnimatorSubsystem)
	{
		return;
	}

	static const FPropertyBagPropertyDesc AnimatorTypeDesc("", EPropertyBagPropertyType::String);
	const FPropertyBagPropertyDesc PropertyTypeDesc("", Property.GetLeafProperty());
	const TSet<UPropertyAnimatorCoreConverterBase*> Converters = AnimatorSubsystem->GetSupportedConverters(AnimatorTypeDesc, PropertyTypeDesc);
	check(!Converters.IsEmpty())
	InLinkedProperty->SetConverterClass(Converters.Array()[0]->GetClass());
}

FTimespan UPropertyAnimatorClock::ParseTime(const FString& InFormat)
{
	// Regex patterns for different formats
	static const FRegexPattern HHMMSSPattern(TEXT("^(?:(\\d{2}):)?(\\d{2}):(\\d{2})$")); // 01:00 00:01:00
	static const FRegexPattern CombinedPattern(TEXT("(?:(\\d+)h)? ?(?:(\\d+)m)? ?(?:(\\d+)s)?")); // 1h 1m 1s

	FRegexMatcher HHMMSSMatcher(HHMMSSPattern, InFormat);
	FRegexMatcher CombinedMatcher(CombinedPattern, InFormat);

	FTimespan ParsedTimeSpan = FTimespan::Zero();

	if (InFormat.IsNumeric())
	{
		const int32 Seconds = FCString::Atoi(*InFormat);
		ParsedTimeSpan = FTimespan::FromSeconds(Seconds);
	}
	else if (HHMMSSMatcher.FindNext())
	{
		const int32 Hours = HHMMSSMatcher.GetCaptureGroup(1).IsEmpty() ? 0 : FCString::Atoi(*HHMMSSMatcher.GetCaptureGroup(1));
		const int32 Minutes = FCString::Atoi(*HHMMSSMatcher.GetCaptureGroup(2));
		const int32 Seconds = FCString::Atoi(*HHMMSSMatcher.GetCaptureGroup(3));
		ParsedTimeSpan = FTimespan::FromHours(Hours) + FTimespan::FromMinutes(Minutes) + FTimespan::FromSeconds(Seconds);
	}
	else if (CombinedMatcher.FindNext())
	{
		const int32 Hours = CombinedMatcher.GetCaptureGroup(1).IsEmpty() ? 0 : FCString::Atoi(*CombinedMatcher.GetCaptureGroup(1));
		const int32 Minutes = CombinedMatcher.GetCaptureGroup(2).IsEmpty() ? 0 : FCString::Atoi(*CombinedMatcher.GetCaptureGroup(2));
		const int32 Seconds = CombinedMatcher.GetCaptureGroup(3).IsEmpty() ? 0 : FCString::Atoi(*CombinedMatcher.GetCaptureGroup(3));
		ParsedTimeSpan = FTimespan::FromHours(Hours) + FTimespan::FromMinutes(Minutes) + FTimespan::FromSeconds(Seconds);
	}

	return ParsedTimeSpan;
}
