// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "DMXControlConsoleEditorGlobalLayoutRow.generated.h"

class UDMXControlConsoleEditorGlobalLayoutBase;
class UDMXControlConsoleFaderGroup;


/** A row of Fader Groups in the Control Console Global Layout */
UCLASS()
class UDMXControlConsoleEditorGlobalLayoutRow
	: public UObject
{
	GENERATED_BODY()

public:
	/** Adds the given Fader Group to the Layout Row */
	void AddToLayoutRow(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Adds the given array of Fader Groups to the Layout Row */
	void AddToLayoutRow(const TArray<UDMXControlConsoleFaderGroup*> InFaderGroups);

	/** Adds the given Fader Group to the Layout Row at the given index */
	void AddToLayoutRow(UDMXControlConsoleFaderGroup* FaderGroup, const int32 Index);

	/** Removes the given Fader Group from the Layout Row */
	void RemoveFromLayoutRow(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Removes the Fader Group at the given index from the Layout Row */
	void RemoveFromLayoutRow(const int32 Index);

	/** Gets Fader Groups array for this row */
	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& GetFaderGroups() const { return FaderGroups; }

	/** Gets the Index of this row according to the owner layout */
	int32 GetRowIndex() const;

	/** Gets the layout that owns this row */
	UDMXControlConsoleEditorGlobalLayoutBase& GetOwnerLayoutChecked() const;

	/** Gets the Fader Group at the given index, if valid */
	UDMXControlConsoleFaderGroup* GetFaderGroupAt(const int32 Index) const { return FaderGroups.IsValidIndex(Index) ? FaderGroups[Index].Get() : nullptr; }

	/** Gets index of the given Fader Group, if valid */
	int32 GetIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const;

private:
	/** Reference to Fader Groups array */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups;
};

