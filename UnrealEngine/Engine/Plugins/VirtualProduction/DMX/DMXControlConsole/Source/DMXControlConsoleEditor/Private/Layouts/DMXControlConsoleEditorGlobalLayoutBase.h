// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "DMXControlConsoleEditorGlobalLayoutBase.generated.h"

class UDMXControlConsoleData;
class UDMXControlConsoleEditorGlobalLayoutRow;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;


UENUM()
/** Enum for DMX Control Console layout modes */
enum class EDMXControlConsoleLayoutMode : uint8
{
	Horizontal,
	Vertical,
	Grid,
	None
};

/** Base class for Control Console layout */
UCLASS(Abstract)
class UDMXControlConsoleEditorGlobalLayoutBase
	: public UObject
{
	GENERATED_BODY()

public:
	/** Adds the given Fader Group to the layout at given row/column index */
	void AddToLayout(UDMXControlConsoleFaderGroup* FaderGroup, const int32 RowIndex, const int32 ColumnIndex);

	/** Adds the given Fader Group at the end of the Layout Row at the given index */
	void AddToLayout(UDMXControlConsoleFaderGroup* FaderGroup, const int32 RowIndex);

	/** Adds a new Layout Row to the layout */
	UDMXControlConsoleEditorGlobalLayoutRow* AddNewRowToLayout();

	/** Add a new Layout Row at the given index */
	UDMXControlConsoleEditorGlobalLayoutRow* AddNewRowToLayout(const int32 RowIndex);

	/** Removes the given Fader Group from the layout */
	void RemoveFromLayout(const UDMXControlConsoleFaderGroup* FaderGroup);

	/** Removes Fader Group at given column/row index, if valid, from the layout */
	void RemoveFromLayout(const int32 RowIndex, const int32 ColumnIndex);

	/** Gets an array of all Layout Rows in this layout */
	const TArray<UDMXControlConsoleEditorGlobalLayoutRow*>& GetLayoutRows() const { return LayoutRows; }

	/** Gets the Layout Row which owns the given Fader Group, if valid */
	UDMXControlConsoleEditorGlobalLayoutRow* GetLayoutRow(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets the Layout Row at given index, if valid */
	UDMXControlConsoleEditorGlobalLayoutRow* GetLayoutRow(const int32 RowIndex) const;

	/** Gets an array of all Fader Groups in this layout */
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> GetAllFaderGroups() const;

	/** Gets an array of all active Fader Groups in this layout */
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> GetAllActiveFaderGroups() const; 

	/** Gets the Fader Group at given row/column index, if valid */
	UDMXControlConsoleFaderGroup* GetFaderGroupAt(const int32 RowIndex, const int32 ColumnIndex) const;

	/** Gets row index for the given Fader Group, if valid */
	int32 GetFaderGroupRowIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets column index for the given Fader Group, if valid */
	int32 GetFaderGroupColumnIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets current layout mode */
	EDMXControlConsoleLayoutMode GetLayoutMode() const { return LayoutMode; }

	/** Sets current layout mode */
	void SetLayoutMode(const EDMXControlConsoleLayoutMode NewLayoutMode);

	/** Sets active state of all Fader Groups in the layout */
	virtual void SetActiveFaderGroupsInLayout(bool bActive);

	/** True if the layout contains the given Fader Group */
	bool ContainsFaderGroup(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Generates Layout Rows by the given Control Console Data */
	virtual void GenerateLayoutByControlConsoleData(const UDMXControlConsoleData* ControlConsoleData);

	/** Finds a patched Fader Group in the layout */
	UDMXControlConsoleFaderGroup* FindFaderGroupByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const;

	/** Clears all Layout Rows */
	void ClearAll();

	/** Clears all patched Fader Groups in the layout */
	void ClearAllPatchedFaderGroups();

	/** Clears all empty Layout Rows in the layout */
	void ClearEmptyLayoutRows();

	// Property Name getters
	FORCEINLINE static FName GetLayoutModePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleEditorGlobalLayoutBase, LayoutMode); }
	
private:
	/** Reference to Layout Rows array */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow>> LayoutRows;

	/** Current layout sorting method for this layout */
	UPROPERTY()
	EDMXControlConsoleLayoutMode LayoutMode = EDMXControlConsoleLayoutMode::Grid;
};