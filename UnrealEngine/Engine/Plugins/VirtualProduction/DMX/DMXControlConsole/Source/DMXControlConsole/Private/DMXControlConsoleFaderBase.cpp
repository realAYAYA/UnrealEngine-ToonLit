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
	: DataType(EDMXFixtureSignalFormat::E8Bit)
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
	if (!bIsLocked)
	{
		Value = FMath::Clamp(NewValue, MinValue, MaxValue);
	}
}

void UDMXControlConsoleFaderBase::SetMinValue(uint32 NewMinValue)
{
	if (!bIsLocked && NewMinValue >= 0)
	{
		MinValue = FMath::Clamp(NewMinValue, 0, MaxValue - 1);
		Value = FMath::Clamp(Value, MinValue, MaxValue);
	}
}

void UDMXControlConsoleFaderBase::SetMaxValue(uint32 NewMaxValue)
{
	if (!bIsLocked && NewMaxValue >= 0)
	{
		const uint8 NumChannels = static_cast<uint8>(DataType) + 1;
		const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
		MaxValue = FMath::Clamp(NewMaxValue, MinValue + 1, ValueRange);
		Value = FMath::Clamp(Value, MinValue, MaxValue);
	}
}

void UDMXControlConsoleFaderBase::SetMute(bool bMute)
{
	bIsMuted = bMute;
}

void UDMXControlConsoleFaderBase::ToggleMute()
{
	SetMute(!bIsMuted);
}

void UDMXControlConsoleFaderBase::SetLock(bool bLock)
{
	bIsLocked = bLock;
}

void UDMXControlConsoleFaderBase::ToggleLock()
{
	SetLock(!bIsLocked);
}

void UDMXControlConsoleFaderBase::ResetToDefault()
{
	Value = DefaultValue;
}

void UDMXControlConsoleFaderBase::PostInitProperties()
{
	Super::PostInitProperties();

	FaderName = GetName();
	DefaultValue = MinValue;
}

#if WITH_EDITOR
void UDMXControlConsoleFaderBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, FloatOscillatorClass))
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
		const uint32 ValueRange = (MaxValue - MinValue) + 1;
		Value = FMath::Clamp(MinValue + FloatOscillator->GetNormalizedValue(DeltaTime) * ValueRange, MinValue, MaxValue);
	}
}

bool UDMXControlConsoleFaderBase::IsTickable() const
{
	return
		!bIsLocked &&
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

void UDMXControlConsoleFaderBase::SetUniverseID(int32 InUniverseID)
{
	UniverseID = FMath::Clamp(InUniverseID, 1, DMX_MAX_UNIVERSE);
}

void UDMXControlConsoleFaderBase::SetValueRange()
{
	const uint8 NumChannels = static_cast<uint8>(DataType) + 1;
	const uint32 ValueRange = ((uint32)FMath::Pow(2.f, 8.f * NumChannels) - 1);
	MaxValue = ValueRange;
	SetMinValue(MinValue);
}

void UDMXControlConsoleFaderBase::SetDataType(EDMXFixtureSignalFormat InDataType)
{
	DataType = InDataType;
	SetAddressRange(StartingAddress);
	SetValueRange();
}

void UDMXControlConsoleFaderBase::SetAddressRange(int32 InStartingAddress)
{
	uint8 NumChannels = static_cast<uint8>(DataType);
	StartingAddress = FMath::Clamp(InStartingAddress, 1, DMX_MAX_ADDRESS - NumChannels);
	EndingAddress = StartingAddress + NumChannels;
}

#undef LOCTEXT_NAMESPACE
