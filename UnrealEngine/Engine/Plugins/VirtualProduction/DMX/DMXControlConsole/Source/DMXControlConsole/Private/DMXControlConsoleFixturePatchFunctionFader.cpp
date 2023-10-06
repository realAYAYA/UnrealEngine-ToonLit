// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchFunctionFader.h"

#include "Library/DMXEntityFixtureType.h"


void UDMXControlConsoleFixturePatchFunctionFader::SetPropertiesFromFixtureFunction(const FDMXFixtureFunction& FixtureFunction, const int32 InUniverseID, const int32 StartingChannel)
{
	// Order of initialization matters
	FaderName = FixtureFunction.Attribute.Name.ToString();
	Attribute = FixtureFunction.Attribute;

	SetUniverseID(InUniverseID);

	StartingAddress = StartingChannel + (FixtureFunction.Channel - 1);
	DefaultValue = FixtureFunction.DefaultValue;
	Value = DefaultValue;
	MinValue = 0;

	SetDataType(FixtureFunction.DataType);

	bUseLSBMode = FixtureFunction.bUseLSBMode;
}
