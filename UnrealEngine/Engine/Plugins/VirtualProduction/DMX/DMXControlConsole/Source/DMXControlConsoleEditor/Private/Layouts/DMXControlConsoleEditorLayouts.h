// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/DMXControlConsoleEditorLayoutsBase.h"

#include "DMXControlConsoleEditorLayouts.generated.h"

class UDMXControlConsoleData;
class UDMXControlConsoleEditorGlobalLayoutBase;


/** Control Console container class for layouts data */
UCLASS()
class UDMXControlConsoleEditorLayouts
	: public UDMXControlConsoleEditorLayoutsBase
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_OneParam(FDMXControlConsoleEditorLayoutDelegate, const UDMXControlConsoleEditorGlobalLayoutBase*);

	// Allow a UDMXControlConsoleEditorGlobalLayoutBase to read Editor Layouts data
	friend UDMXControlConsoleEditorGlobalLayoutBase;

public:
	/** Constructor */
	UDMXControlConsoleEditorLayouts();

	/** Adds a new User Layout */
	UDMXControlConsoleEditorGlobalLayoutBase* AddUserLayout(const FString& LayoutName);

	/** Deletes the given User Layout */
	void DeleteUserLayout(UDMXControlConsoleEditorGlobalLayoutBase* UserLayout);

	/** Finds the User Layout with the given name, if valid */
	UDMXControlConsoleEditorGlobalLayoutBase* FindUserLayoutByName(const FString& LayoutName) const;

	/** Clears UserLayouts array */
	void ClearUserLayouts();

	/** Gets a reference to the Default Layout */
	UDMXControlConsoleEditorGlobalLayoutBase& GetDefaultLayoutChecked() const;

	/** Gets User Layouts array */
	const TArray<UDMXControlConsoleEditorGlobalLayoutBase*>& GetUserLayouts() const { return UserLayouts; }

	/** Gets reference to the active Layout */
	UDMXControlConsoleEditorGlobalLayoutBase* GetActiveLayout() const { return ActiveLayout; }

	/** Sets the active Layout */
	void SetActiveLayout(UDMXControlConsoleEditorGlobalLayoutBase* InLayout);

	/** Updates the default Layout to Control Console Data */
	void UpdateDefaultLayout();

	/** Registers all the layouts */
	void Register(UDMXControlConsoleData* ControlConsoleData);

	/** Unregisters all the layouts */
	void Unregister(UDMXControlConsoleData* ControlConsoleData);

	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	//~ End UObject interface

	/** Called after the Active Layout has been changed */
	FDMXControlConsoleEditorLayoutDelegate& GetOnActiveLayoutChanged() { return OnActiveLayoutChanged; }

	/** Called after the Active Layouts's layout mode has been changed */
	FSimpleMulticastDelegate& GetOnLayoutModeChanged() { return OnLayoutModeChanged; }

private:
	/** Generates a unique name for a user layout */
	FString GenerateUniqueUserLayoutName(const FString& LayoutName);

	/** Called after the Active Layout has been changed */
	FDMXControlConsoleEditorLayoutDelegate OnActiveLayoutChanged;

	/** Called after the Active Layouts's layout mode has been changed */
	FSimpleMulticastDelegate OnLayoutModeChanged;

	/** Reference to the Default Layout */
	UPROPERTY()
	TObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> DefaultLayout;

	/** Array of User Layouts */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase>> UserLayouts;

	/** Reference to the active Layout in use */
	UPROPERTY()
	TObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> ActiveLayout;
};
