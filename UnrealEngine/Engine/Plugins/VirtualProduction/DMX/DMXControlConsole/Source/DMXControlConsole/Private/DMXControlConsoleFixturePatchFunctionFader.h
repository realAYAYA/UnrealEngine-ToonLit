// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXAttribute.h"
#include "DMXControlConsoleFaderBase.h"

#include "DMXControlConsoleFixturePatchFunctionFader.generated.h"

struct FDMXAttributeName;
struct FDMXFixtureFunction;


/** A fader matching a Fixture Patch Function in the DMX Control Console. */
UCLASS()
class DMXCONTROLCONSOLE_API UDMXControlConsoleFixturePatchFunctionFader
	: public UDMXControlConsoleFaderBase
{
	GENERATED_BODY()

public:
	/** Sets Fader's properties values using the given Fixture Function */
	void SetPropertiesFromFixtureFunction(const FDMXFixtureFunction& FixtureFunction, const int32 InUniverseID, const int32 StartingChannel);

	/** Returns the name of the attribute mapped to this fader */
	const FDMXAttributeName& GetAttributeName() const { return Attribute; }

	// Property Name getters
	FORCEINLINE static FName GetAttributePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFixturePatchFunctionFader, Attribute); }

private:
	UPROPERTY(VisibleAnywhere, meta = (DisplayName = "Attribute Mapping", DisplayPriority = "2"), Category = "DMX Fader")
	FDMXAttributeName Attribute;
};