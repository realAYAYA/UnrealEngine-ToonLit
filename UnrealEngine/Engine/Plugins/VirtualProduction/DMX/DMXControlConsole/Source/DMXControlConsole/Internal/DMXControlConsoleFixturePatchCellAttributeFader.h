// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXAttribute.h"
#include "DMXControlConsoleFaderBase.h"

#include "DMXControlConsoleFixturePatchCellAttributeFader.generated.h"

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
	//~ Begin IDMXControlConsoleFaderGroupElementInterface
	virtual UDMXControlConsoleFaderGroup& GetOwnerFaderGroupChecked() const override;
	virtual int32 GetIndex() const override;
#if WITH_EDITOR
	virtual void SetIsMatchingFilter(bool bMatches) override;
#endif // WITH_EDITOR
	virtual void Destroy() override;
	//~ End IDMXControlConsoleFaderGroupElementInterface

	/** Sets Matrix Cell Fader's properties values using the given Fixture Cell Attribute */
	void SetPropertiesFromFixtureCellAttribute(const FDMXFixtureCellAttribute& FixtureCellAttribute, const int32 InUniverseID, const int32 StartingChannel);

	/** Gets the owner Matrix Cell of this Matrix Cell Fader */
	UDMXControlConsoleFixturePatchMatrixCell& GetOwnerMatrixCellChecked() const;

	/** Returns the name of the attribute mapped to this fader */
	const FDMXAttributeName& GetAttributeName() const { return Attribute; }

	// Property Name getters
	FORCEINLINE static FName GetAttributePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFixturePatchCellAttributeFader, Attribute); }

private:
	/** Name of the attribute mapped to this Fader */
	UPROPERTY(VisibleAnywhere, meta = (DisplayName = "Attribute Mapping", DisplayPriority = "2"), Category = "DMX Fader")
	FDMXAttributeName Attribute;
};
