// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "DMXControlConsoleEditorGlobalLayoutBase.generated.h"

class UDMXControlConsoleData;
class UDMXControlConsoleEditorGlobalLayoutRow;
class UDMXControlConsoleEditorLayouts;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFaderGroupController;
class UDMXEntity;
class UDMXEntityFixturePatch;
class UDMXLibrary;


/** Enum for DMX Control Console layout modes */
UENUM()
enum class EDMXControlConsoleLayoutMode : uint8
{
	Horizontal,
	Vertical,
	Grid,
	None
};

/** Base class for the Control Console layout */
UCLASS()
class UDMXControlConsoleEditorGlobalLayoutBase
	: public UObject
{
	GENERATED_BODY()

public:
	/** Adds a new Controller for the given Fader Group to the layout, at the given row/column index */
	UDMXControlConsoleFaderGroupController* AddToLayout(UDMXControlConsoleFaderGroup* InFaderGroup, const FString& ControllerName = "", const int32 RowIndex = INDEX_NONE, const int32 ColumnIndex = INDEX_NONE);

	/** Adds a new Controller for the given array of Fader Groups to the layout, at the given row/column index */
	UDMXControlConsoleFaderGroupController* AddToLayout(const TArray<UDMXControlConsoleFaderGroup*> InFaderGroups, const FString& ControllerName = "", const int32 RowIndex = INDEX_NONE, const int32 ColumnIndex = INDEX_NONE);

	/** Adds a new Layout Row at the given index */
	UDMXControlConsoleEditorGlobalLayoutRow* AddNewRowToLayout(const int32 RowIndex = INDEX_NONE);

	/** Gets the editor layouts object which owns this layout */
	UDMXControlConsoleEditorLayouts& GetOwnerEditorLayoutsChecked() const;

	/** Gets an array of all the Layout Rows in this layout */
	const TArray<UDMXControlConsoleEditorGlobalLayoutRow*>& GetLayoutRows() const { return LayoutRows; }

	/** Gets the Layout Row which owns the given Fader Group Controller, if valid */
	UDMXControlConsoleEditorGlobalLayoutRow* GetLayoutRow(const UDMXControlConsoleFaderGroupController* FaderGroupController) const;

	/** Gets an array of all the Fader Group Controllers in this layout */
	TArray<UDMXControlConsoleFaderGroupController*> GetAllFaderGroupControllers() const;

	/** Adds the given Fader Group Controller to the array of active Controllers */
	void AddToActiveFaderGroupControllers(UDMXControlConsoleFaderGroupController* FaderGroupController);

	/** Removes the given Fader Group Controller form the array of active Controllers */
	void RemoveFromActiveFaderGroupControllers(UDMXControlConsoleFaderGroupController* FaderGroupController);

	/** Gets an array of all the active Fader Group Controllers in this layout */
	TArray<UDMXControlConsoleFaderGroupController*> GetAllActiveFaderGroupControllers() const;

	/** Sets the activity state of all the Fader Group Controllers in the layout */
	void SetActiveFaderGroupControllersInLayout(bool bActive);

	/** Gets the row index of the given Fader Group Controller, if valid */
	int32 GetFaderGroupControllerRowIndex(const UDMXControlConsoleFaderGroupController* FaderGroupController) const;

	/** Gets the column index of the given Fader Group Controller, if valid */
	int32 GetFaderGroupControllerColumnIndex(const UDMXControlConsoleFaderGroupController* FaderGroupController) const;

	/** Gets the current layout mode */
	EDMXControlConsoleLayoutMode GetLayoutMode() const { return LayoutMode; }

	/** Sets the current layout mode */
	void SetLayoutMode(const EDMXControlConsoleLayoutMode NewLayoutMode);

	/** True if the layout contains the given Fader Group Controller */
	bool ContainsFaderGroupController(const UDMXControlConsoleFaderGroupController* FaderGroupController) const;

	/** True if one of the Controllers in the layout possesses the given Fader Group */
	bool ContainsFaderGroup(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Finds the Fader Group Controller matching the given Fixture Patch in the layout, if valid */
	UDMXControlConsoleFaderGroupController* FindFaderGroupControllerByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const;

	/** Generates Layout Rows by the given Control Console Data */
	void GenerateLayoutByControlConsoleData(const UDMXControlConsoleData* ControlConsoleData);

	/** Clears all Layout Rows */
	void ClearAll(const bool bOnlyPatchedFaderGroups = false);

	/** Clears all empty Layout Rows in the layout */
	void ClearEmptyLayoutRows();

	/** Registers this layout */
	void Register(UDMXControlConsoleData* ControlConsoleData);

	/** Unregisters this layout */
	void Unregister(UDMXControlConsoleData* ControlConsoleData);

	/** True if this layout is registered to the DMX Library delegates */
	bool IsRegistered() const { return bIsRegistered; }

	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	//~ End UObject interface

	// Property Name getters
	FORCEINLINE static FName GetLayoutModePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleEditorGlobalLayoutBase, LayoutMode); }
	FORCEINLINE static FName GetLayoutNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleEditorGlobalLayoutBase, LayoutName); }

	/** Name identifier of this Layout */
	UPROPERTY()
	FString LayoutName;

private:
	/** Called when the active layout has changed */
	void OnActiveLayoutchanged(const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout);

	/** Called when a Fixture Patch was removed from a DMX Library */
	void OnFixturePatchRemovedFromLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities);

	/** Called when a Fader Group was added to the Control Console Data */
	void OnFaderGroupAddedToData(const UDMXControlConsoleFaderGroup* FaderGroup, UDMXControlConsoleData* ControlConsoleData);

	/** Called to clean this layout from all the unpatched Fader Groups */
	void CleanLayoutFromUnpatchedFaderGroupControllers();

	/** Reference to the Layout Rows array */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow>> LayoutRows;

	/** Array of the currently active Fader Group Controllers in the layout */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroupController>> ActiveFaderGroupControllers;

	/** Current layout sorting method for this layout */
	UPROPERTY()
	EDMXControlConsoleLayoutMode LayoutMode = EDMXControlConsoleLayoutMode::Grid;

	/** True if the layout is registered to the DMX Library delegates */
	bool bIsRegistered = false;
};
