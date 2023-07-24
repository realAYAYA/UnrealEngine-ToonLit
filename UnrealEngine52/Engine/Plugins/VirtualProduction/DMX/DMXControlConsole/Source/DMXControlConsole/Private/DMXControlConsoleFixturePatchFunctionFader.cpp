// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchFunctionFader.h"

#include "DMXProtocolTypes.h"
#include "Library/DMXEntityFixtureType.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFixturePatchFunctionFader"

UDMXControlConsoleFixturePatchFunctionFader::UDMXControlConsoleFixturePatchFunctionFader()
	: DataType(EDMXFixtureSignalFormat::E8Bit)
{}

void UDMXControlConsoleFixturePatchFunctionFader::SetPropertiesFromFixtureFunction(const FDMXFixtureFunction& FixtureFunction, const int32 InUniverseID, const int32 StartingChannel)
{
	// Order of initialization matters
	FaderName = FixtureFunction.Attribute.Name.ToString();
	Attribute = FixtureFunction.Attribute;

	SetUniverseID(InUniverseID);
	
	StartingAddress = StartingChannel + (FixtureFunction.Channel - 1);
	Value = FixtureFunction.DefaultValue;
	MinValue = 0;
	
	SetDataType(FixtureFunction.DataType);

	bUseLSBMode = FixtureFunction.bUseLSBMode;
}

void UDMXControlConsoleFixturePatchFunctionFader::SetUniverseID(int32 InUniverseID)
{
	UniverseID = FMath::Clamp(InUniverseID, 1, DMX_MAX_UNIVERSE);
}

void UDMXControlConsoleFixturePatchFunctionFader::SetValueRange()
{
	const uint8 NumChannels = static_cast<uint8>(DataType) + 1;
	MaxValue = MinValue + ((uint32)FMath::Pow(2.f, 8.f * NumChannels) - 1);
}

void UDMXControlConsoleFixturePatchFunctionFader::SetDataType(EDMXFixtureSignalFormat InDataType)
{
	DataType = InDataType;
	SetAddressRange(StartingAddress);
	SetValueRange();
}

void UDMXControlConsoleFixturePatchFunctionFader::SetAddressRange(int32 InStartingAddress)
{
	const uint8 NumChannels = static_cast<uint8>(DataType);
	StartingAddress = FMath::Clamp(InStartingAddress, 1, DMX_MAX_ADDRESS - NumChannels);
	EndingAddress = StartingAddress + NumChannels;
}

#undef LOCTEXT_NAMESPACE
