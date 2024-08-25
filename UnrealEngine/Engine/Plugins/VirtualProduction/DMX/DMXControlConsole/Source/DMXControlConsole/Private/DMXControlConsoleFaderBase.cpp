// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderBase.h"

#include "DMXControlConsoleFaderGroup.h"
#include "Layouts/Controllers/DMXControlConsoleControllerBase.h"
#include "Oscillators/DMXControlConsoleFloatOscillator.h"


UDMXControlConsoleFaderGroup& UDMXControlConsoleFaderBase::GetOwnerFaderGroupChecked() const
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get fader owner correctly."), *GetName());

	return *Outer;
}

UDMXControlConsoleControllerBase* UDMXControlConsoleFaderBase::GetElementController() const
{
	return CachedWeakElementController.Get();
}

void UDMXControlConsoleFaderBase::SetElementController(UDMXControlConsoleControllerBase* NewController)
{
	SoftControllerPtr = NewController;
	CachedWeakElementController = NewController;
}

UDMXControlConsoleFaderBase::UDMXControlConsoleFaderBase()
	: DataType(EDMXFixtureSignalFormat::E8Bit)
{
	ThisFaderAsArray.Add(this);
}

int32 UDMXControlConsoleFaderBase::GetIndex() const
{
	const UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();
	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = OwnerFaderGroup.GetElements();
	return Elements.IndexOfByKey(this);
}

void UDMXControlConsoleFaderBase::Destroy()
{
	UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();

#if WITH_EDITOR
	OwnerFaderGroup.PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetElementsPropertyName()));
#endif // WITH_EDITOR

	OwnerFaderGroup.DeleteElement(this);

#if WITH_EDITOR
	OwnerFaderGroup.PostEditChange();
#endif // WITH_EDITOR
}

void UDMXControlConsoleFaderBase::SetFaderName(const FString& NewName)
{
	FaderName = NewName;
}

void UDMXControlConsoleFaderBase::SetValue(uint32 NewValue)
{
	Value = FMath::Clamp(NewValue, MinValue, MaxValue);
}

void UDMXControlConsoleFaderBase::SetMinValue(uint32 NewMinValue)
{
	MinValue = FMath::Clamp(NewMinValue, 0, MaxValue - 1);
	Value = FMath::Clamp(Value, MinValue, MaxValue);
}

void UDMXControlConsoleFaderBase::SetMaxValue(uint32 NewMaxValue)
{
	const uint8 NumChannels = static_cast<uint8>(DataType) + 1;
	const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
	MaxValue = FMath::Clamp(NewMaxValue, MinValue + 1, ValueRange);
	Value = FMath::Clamp(Value, MinValue, MaxValue);
}

void UDMXControlConsoleFaderBase::SetEnabled(bool bEnable)
{
	bIsEnabled = bEnable;
}

bool UDMXControlConsoleFaderBase::IsLocked() const
{
	const UDMXControlConsoleControllerBase* ElementController = GetElementController();
	return ElementController && ElementController->IsLocked();
}

void UDMXControlConsoleFaderBase::SetLocked(bool bLock)
{
	bIsLocked = bLock;
}

void UDMXControlConsoleFaderBase::ResetToDefault()
{
	SetValue(DefaultValue);
}

void UDMXControlConsoleFaderBase::PostInitProperties()
{
	Super::PostInitProperties();

	FaderName = GetName();
	DefaultValue = MinValue;
}

void UDMXControlConsoleFaderBase::PostLoad()
{
	Super::PostLoad();

	CachedWeakElementController = Cast<UDMXControlConsoleControllerBase>(SoftControllerPtr.ToSoftObjectPath().TryLoad());
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
