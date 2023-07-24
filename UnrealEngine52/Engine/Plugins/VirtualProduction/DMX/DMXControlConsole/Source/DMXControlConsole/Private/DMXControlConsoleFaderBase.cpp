// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderBase.h"

#include "DMXControlConsoleFaderGroup.h"
#include "Oscillators/DMXControlConsoleFloatOscillator.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFaderBase"

UDMXControlConsoleFaderGroup& UDMXControlConsoleFaderBase::GetOwnerFaderGroupChecked() const
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get fader owner correctly."), *GetName());

	return *Outer;
}

UDMXControlConsoleFaderBase::UDMXControlConsoleFaderBase()
{
	ThisFaderAsArray.Add(this);
}

int32 UDMXControlConsoleFaderBase::GetIndex() const
{
	int32 Index = -1;

	const UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	if (!ensureMsgf(Outer, TEXT("Invalid outer for '%s', cannot get fader index correctly."), *GetName()))
	{
		return Index;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = Outer->GetElements();
	Index = Elements.IndexOfByKey(this);

	return Index;
}

void UDMXControlConsoleFaderBase::Destroy()
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	if (!ensureMsgf(Outer, TEXT("Invalid outer for '%s', cannot destroy fader correctly."), *GetName()))
	{
		return;
	}

#if WITH_EDITOR
	Outer->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetElementsPropertyName()));
#endif // WITH_EDITOR

	Outer->DeleteElement(this);

#if WITH_EDITOR
	Outer->PostEditChange();
#endif // WITH_EDITOR
}

void UDMXControlConsoleFaderBase::SetFaderName(const FString& NewName)
{ 
	FaderName = NewName;
}

void UDMXControlConsoleFaderBase::SetValue(const uint32 NewValue)
{
	Value = FMath::Clamp(NewValue, MinValue, MaxValue);
}

void UDMXControlConsoleFaderBase::SetMute(bool bMute)
{
	bIsMuted = bMute;
}

void UDMXControlConsoleFaderBase::ToggleMute()
{
	bIsMuted = !bIsMuted;
}

void UDMXControlConsoleFaderBase::PostInitProperties()
{
	Super::PostInitProperties();

	FaderName = GetName();
}

#if WITH_EDITOR
void UDMXControlConsoleFaderBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, Value))
	{
		SetValue(Value);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, FloatOscillatorClass))
	{
		if (UClass* StrongFloatOscillatorClass = FloatOscillatorClass.Get())
		{
			FloatOscillator = NewObject<UDMXControlConsoleFloatOscillator>(this, StrongFloatOscillatorClass, NAME_None, RF_Transactional | RF_Public);
		}
		else
		{
			FloatOscillator = nullptr;
		}
	}
}
#endif // WITH_EDITOR

void UDMXControlConsoleFaderBase::Tick(float DeltaTime)
{
	if (FloatOscillator)
	{
		const EDMXFixtureSignalFormat DataType = GetDataType();
		uint32 AbsoluteMax;
		switch (DataType)
		{
		case EDMXFixtureSignalFormat::E8Bit:
			AbsoluteMax = TNumericLimits<uint8>::Max();
			break;
		case EDMXFixtureSignalFormat::E16Bit:
			AbsoluteMax = TNumericLimits<uint16>::Max();
			break;
		case EDMXFixtureSignalFormat::E24Bit:
			AbsoluteMax = 0x00FFFFFF;
			break;
		default:
			AbsoluteMax = TNumericLimits<uint32>::Max();
		}

		Value = FMath::Clamp(FloatOscillator->GetNormalizedValue(DeltaTime) * AbsoluteMax, 0, AbsoluteMax);
	}
}

bool UDMXControlConsoleFaderBase::IsTickable() const
{
	return 
		!bIsMuted &&
		FloatOscillator != nullptr;
}

TStatId UDMXControlConsoleFaderBase::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDMXControlConsoleFaderBase, STATGROUP_Tickables);
}

ETickableTickType UDMXControlConsoleFaderBase::GetTickableTickType() const
{
	return ETickableTickType::Conditional;
}

#undef LOCTEXT_NAMESPACE
