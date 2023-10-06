// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleFaderBase.h"

#include "DMXControlConsoleRawFader.generated.h"


/** A fader matching that sends Raw DMX */
UCLASS()
class DMXCONTROLCONSOLE_API UDMXControlConsoleRawFader
	: public UDMXControlConsoleFaderBase
{
	GENERATED_BODY()

public:
	/** Constructor */
	UDMXControlConsoleRawFader();

	//~ Begin DMXControlConsoleFaderBase interface
	virtual void SetDataType(EDMXFixtureSignalFormat InDataType) override { Super::SetDataType(InDataType); }
	virtual void SetUniverseID(int32 InUniversID) override { Super::SetUniverseID(InUniversID); }
	virtual void SetAddressRange(int32 InStartingAddress) override { Super::SetAddressRange(InStartingAddress); }
	//~ End DMXControlConsoleFaderBase interface
};
