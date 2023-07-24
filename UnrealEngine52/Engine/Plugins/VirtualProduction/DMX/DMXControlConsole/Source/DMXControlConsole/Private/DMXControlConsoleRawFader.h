// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DMXControlConsoleFaderBase.h"

#include "DMXControlConsoleRawFader.generated.h"

enum class EDMXFixtureSignalFormat : uint8;


/** A fader matching that sends Raw DMX */
UCLASS()
class DMXCONTROLCONSOLE_API UDMXControlConsoleRawFader
	: public UDMXControlConsoleFaderBase
{
	GENERATED_BODY()

public:
	//~ Begin DMXControlConsoleFaderBase interface
	virtual EDMXFixtureSignalFormat GetDataType() const { return DataType; }
	int32 GetUniverseID() const { return UniverseID; }
	//~ End DMXControlConsoleFaderBase interface

	//~ Being IDMXControlConsoleFaderGroupElementInterface
	virtual int32 GetStartingAddress() const override { return StartingAddress; }
	//~ End IDMXControlConsoleFaderGroupElementInterface

	/** Constructor */
	UDMXControlConsoleRawFader();

	/** Sets a new universe ID, checking for its validity */
	virtual void SetUniverseID(int32 InUniversID);

	/** Sets starting/ending address range, according to the number of channels  */
	virtual void SetAddressRange(int32 InStartingAddress);

	/** Gets wheter this Fader uses LSB mode or not */
	bool GetUseLSBMode() const { return bUseLSBMode; }

	// Property Name getters
	FORCEINLINE static FName GetUniverseIDPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleRawFader, UniverseID); }
	FORCEINLINE static FName GetDataTypePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleRawFader, DataType); }
	FORCEINLINE static FName GetStartingAddressPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleRawFader, StartingAddress); }

protected:
	//~ Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject interface

private:
	/** Sets this fader's number of channels */
	virtual void SetDataType(EDMXFixtureSignalFormat InDataType);

	/** Sets min/max range, according to the number of channels */
	virtual void SetValueRange();

	/** The universe the should send to fader */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "3"), Category = "DMX Fader")
	int32 UniverseID = 1;

	/** The number of channels this Fader uses */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "2"), Category = "DMX Fader")
	EDMXFixtureSignalFormat DataType;

	/** The starting channel Address to send DMX to */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "4"), Category = "DMX Fader")
	int32 StartingAddress = 1;

	/** Use Least Significant Byte mode. Individual bytes(channels) be interpreted with the first bytes being the lowest part of the number(endianness). */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "9"), Category = "DMX Fader")
	bool bUseLSBMode = false;
};
