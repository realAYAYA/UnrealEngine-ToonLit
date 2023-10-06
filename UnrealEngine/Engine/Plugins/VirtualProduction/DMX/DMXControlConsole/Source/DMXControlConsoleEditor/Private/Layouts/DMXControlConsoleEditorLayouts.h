// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/DMXControlConsoleEditorLayoutsBase.h"

#include "DMXControlConsoleEditorLayouts.generated.h"

class UDMXControlConsoleData;
class UDMXControlConsoleEditorGlobalLayoutBase;
class UDMXControlConsoleEditorGlobalLayoutDefault;
class UDMXControlConsoleEditorGlobalLayoutUser;


/** Control Console container class for layouts data */
UCLASS()
class UDMXControlConsoleEditorLayouts
	: public UDMXControlConsoleEditorLayoutsBase
{
	GENERATED_BODY()

	// Allow a UDMXControlConsoleEditorGlobalLayoutBase to read Editor Layouts data
	friend UDMXControlConsoleEditorGlobalLayoutBase;

public:
	/** Constructor */
	UDMXControlConsoleEditorLayouts();

	/** Adds a new User Layout */
	UDMXControlConsoleEditorGlobalLayoutUser* AddUserLayout(const FString& LayoutName);

	/** Deletes the given User Layout */
	void DeleteUserLayout(UDMXControlConsoleEditorGlobalLayoutUser* UserLayout);

	/** Finds the User Layout with the given name, if valid */
	UDMXControlConsoleEditorGlobalLayoutUser* FindUserLayoutByName(const FString& LayoutName) const;

	/** Clears UserLayouts array */
	void ClearUserLayouts();

	/** Gets reference to Default Layout */
	UDMXControlConsoleEditorGlobalLayoutDefault* GetDefaultLayout() const { return DefaultLayout; }

	/** Gets User Layouts array */
	const TArray<UDMXControlConsoleEditorGlobalLayoutUser*>& GetUserLayouts() const { return UserLayouts; }

	/** Gets reference to active Layout */
	UDMXControlConsoleEditorGlobalLayoutBase* GetActiveLayout() const { return ActiveLayout; }

	/** Sets the active Layout */
	void SetActiveLayout(UDMXControlConsoleEditorGlobalLayoutBase* InLayout);

	/** Updates the default Layout to the given Control Console Data */
	void UpdateDefaultLayout(const UDMXControlConsoleData* ControlConsoleData);

	/** Called after the Active Layout has been changed */
	FSimpleMulticastDelegate& GetOnActiveLayoutChanged() { return OnActiveLayoutChanged; }

	/** Called after the Active Layouts's layout mode has been changed */
	FSimpleMulticastDelegate& GetOnLayoutModeChanged() { return OnLayoutModeChanged; }

protected:
	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	//~ End UObject interface

private:
	/** Called after the Active Layout has been changed */
	FSimpleMulticastDelegate OnActiveLayoutChanged;

	/** Called after the Active Layouts's layout mode has been changed */
	FSimpleMulticastDelegate OnLayoutModeChanged;

	/** Reference to Default Layout */
	UPROPERTY()
	TObjectPtr<UDMXControlConsoleEditorGlobalLayoutDefault> DefaultLayout;

	/** Array of User Layouts */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleEditorGlobalLayoutUser>> UserLayouts;

	/** Reference to active Layout in use */
	UPROPERTY()
	TObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> ActiveLayout;
};
