// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleRawFader.h"

#include "DMXProtocolTypes.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleRawFader"

UDMXControlConsoleRawFader::UDMXControlConsoleRawFader()
	: DataType(EDMXFixtureSignalFormat::E8Bit)
{
	FaderName = TEXT("Fader");
}

void UDMXControlConsoleRawFader::SetUniverseID(int32 InUniverseID)
{
	UniverseID = FMath::Clamp(InUniverseID, 1, DMX_MAX_UNIVERSE);
}

void UDMXControlConsoleRawFader::SetValueRange()
{
	const uint8 NumChannels = static_cast<uint8>(DataType) + 1;
	MaxValue = MinValue + ((uint32)FMath::Pow(2.f, 8.f * NumChannels) - 1);
}

void UDMXControlConsoleRawFader::SetDataType(EDMXFixtureSignalFormat InDataType)
{
	DataType = InDataType;
	SetAddressRange(StartingAddress);
	SetValueRange();
}

void UDMXControlConsoleRawFader::SetAddressRange(int32 InStartingAddress)
{
	uint8 NumChannels = static_cast<uint8>(DataType);
	StartingAddress = FMath::Clamp(InStartingAddress, 1, DMX_MAX_ADDRESS - NumChannels);
	EndingAddress = StartingAddress + NumChannels;
}

#if WITH_EDITOR
void UDMXControlConsoleRawFader::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GetStartingAddressPropertyName() || 
		PropertyName == GetDataTypePropertyName())
	{
		SetDataType(DataType);
	}
	else if (PropertyName == GetUniverseIDPropertyName())
	{
		SetUniverseID(UniverseID);
	}
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
