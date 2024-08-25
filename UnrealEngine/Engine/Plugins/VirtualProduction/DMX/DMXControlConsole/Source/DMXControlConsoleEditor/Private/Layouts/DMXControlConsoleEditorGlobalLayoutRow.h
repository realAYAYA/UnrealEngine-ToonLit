// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "DMXControlConsoleEditorGlobalLayoutRow.generated.h"

class UDMXControlConsoleEditorGlobalLayoutBase;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFaderGroupController;


/** A row of Fader Group Controllers in the Control Console Global Layout */
UCLASS()
class UDMXControlConsoleEditorGlobalLayoutRow
	: public UObject
{
	GENERATED_BODY()
	
public:
	/** Creates a Controller for the given Fader Group */
	UDMXControlConsoleFaderGroupController* CreateFaderGroupController(UDMXControlConsoleFaderGroup* InFaderGroup, const FString& ControllerName = "", const int32 Index = INDEX_NONE);

	/** Creates a Controller for the given array of Fader Groups */
	UDMXControlConsoleFaderGroupController* CreateFaderGroupController(const TArray<UDMXControlConsoleFaderGroup*> InFaderGroups, const FString& ControllerName = "", const int32 Index = INDEX_NONE);

	/** Deletes the given Fader Group Controller */
	void DeleteFaderGroupController(UDMXControlConsoleFaderGroupController* FaderGroupController);

	/** Gets Fader Group Controllers array for this row */
	const TArray<UDMXControlConsoleFaderGroupController*>& GetFaderGroupControllers() const { return FaderGroupControllers; }

	/** Gets the Index of this row according to the owner layout */
	int32 GetRowIndex() const;

	/** Gets the layout that owns this row */
	UDMXControlConsoleEditorGlobalLayoutBase& GetOwnerLayoutChecked() const;

	/** Gets the Fader Group Controller at the given index, if valid */
	UDMXControlConsoleFaderGroupController* GetFaderGroupControllerAt(int32 Index) const;

	/** Gets index of the given Fader Group Controller, if valid */
	int32 GetIndex(const UDMXControlConsoleFaderGroupController* FaderGroupController) const;

	// Property Name getters
	FORCEINLINE static FName GetFaderGroupControllersPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleEditorGlobalLayoutRow, FaderGroupControllers); }

private:
	/** Reference to the Fader Group Controllers array */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleFaderGroupController>> FaderGroupControllers;
};

