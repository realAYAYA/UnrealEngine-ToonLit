// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DMXAttribute.h"
#include "DMXControlConsoleFaderBase.h"

#include "DMXControlConsoleFixturePatchCellAttributeFader.generated.h"

enum class EDMXFixtureSignalFormat : uint8;
struct FDMXAttributeName;
struct FDMXFixtureCellAttribute;
class UDMXControlConsoleFixturePatchMatrixCell;


/** A fader matching a Fixture Cell Attribute in the DMX Control Console. */
UCLASS()
class DMXCONTROLCONSOLE_API UDMXControlConsoleFixturePatchCellAttributeFader
	: public UDMXControlConsoleFaderBase
{
	GENERATED_BODY()

public:
	//~ Begin DMXControlConsoleFaderBase interface
	virtual EDMXFixtureSignalFormat GetDataType() const { return DataType; }
	//~ End DMXControlConsoleFaderBase interface

	//~ Begin IDMXControlConsoleFaderGroupElementInterface
	virtual UDMXControlConsoleFaderGroup& GetOwnerFaderGroupChecked() const override;
	virtual int32 GetIndex() const override;
	virtual int32 GetUniverseID() const override { return UniverseID; }
	virtual int32 GetStartingAddress() const override { return StartingAddress; }
	virtual int32 GetEndingAddress() const override { return EndingAddress; }
	virtual void Destroy() override;
	//~ End IDMXControlConsoleFaderGroupElementInterface

	/** Constructor */
	UDMXControlConsoleFixturePatchCellAttributeFader();

	/** Sets Matrix Cell Fader's properties values using the given Fixture Cell Attribute */
	void SetPropertiesFromFixtureCellAttribute(const FDMXFixtureCellAttribute& FixtureCellAttribute, const int32 InUniverseID, const int32 StartingChannel);

	/** Gets the owner Matrix Cell of this Matrix Cell Fader */
	UDMXControlConsoleFixturePatchMatrixCell& GetOwnerMatrixCellChecked() const;

	/** Gets wheter this Fader uses LSB mode or not */
	bool GetUseLSBMode() const { return bUseLSBMode; }

	/** Returns the name of the attribute mapped to this fader */
	const FDMXAttributeName& GetAttributeName() const { return Attribute; }

	/** Returns the Universe this cell attribute resides in */

	// Property Name getters
	FORCEINLINE static FName GetStartingAddressPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFixturePatchCellAttributeFader, StartingAddress); }
	FORCEINLINE static FName GetAttributePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFixturePatchCellAttributeFader, Attribute); }

private:
	/** Sets this fader's number of channels */
	virtual void SetDataType(EDMXFixtureSignalFormat InDataType);

	/** Sets min/max range, according to the number of channels */
	virtual void SetValueRange();

	/** Sets starting/ending address range, according to the number of channels  */
	virtual void SetAddressRange(int32 InStartingAddress);

	/** The universe DMX data should be send to */
	UPROPERTY(VisibleAnywhere, meta = (DisplayPriority = "4"), Category = "DMX Fader")
	int32 UniverseID = 1;

	/** The starting channel Address to send DMX to */
	UPROPERTY(VisibleAnywhere, meta = (DisplayPriority = "4"), Category = "DMX Fader")
	int32 StartingAddress = 1;

	/** Use Least Significant Byte mode. Individual bytes(channels) be interpreted with the first bytes being the lowest part of the number(endianness). */
	UPROPERTY(VisibleAnywhere, meta = (DisplayPriority = "9"), Category = "DMX Fader")
	bool bUseLSBMode = false;

	/** Name of the attribute mapped to this Fader */
	UPROPERTY(VisibleAnywhere, meta = (DisplayName = "Attribute Mapping", DisplayPriority = "2"), Category = "DMX Fader")
	FDMXAttributeName Attribute;

	/** The number of channels this Fader uses */
	EDMXFixtureSignalFormat DataType;
};
