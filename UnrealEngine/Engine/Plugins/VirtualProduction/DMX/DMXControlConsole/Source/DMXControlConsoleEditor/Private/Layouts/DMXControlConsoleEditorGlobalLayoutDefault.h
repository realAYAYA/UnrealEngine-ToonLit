// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"

#include "DMXControlConsoleEditorGlobalLayoutDefault.generated.h"

class UDMXControlConsoleData;
class UDMXControlConsoleFaderGroup;
class UDMXEntity;
class UDMXLibrary;


/** A layout where Control Console data are sorted by default */
UCLASS()
class UDMXControlConsoleEditorGlobalLayoutDefault
	: public UDMXControlConsoleEditorGlobalLayoutBase
{
	GENERATED_BODY()

public:
	/** Registers this layout to DMX Library delegates  */
	void Register();

	/** Unregisters this layout from DMX Library delegates  */
	void Unregister();

	/** True if this layout is registered to DMX Library delegates */
	bool IsRegistered() const { return bIsRegistered; }

	/** Adds the Fader Group to the array of active Fader Groups */
	void AddToActiveFaderGroups(UDMXControlConsoleFaderGroup* FaderGroup);
	
	/** Removes the Fader Group form the array of active Fader Groups */
	void RemoveFromActiveFaderGroups(UDMXControlConsoleFaderGroup* FaderGroup);

	//~ Begin UDMXControlConsoleBaseGlobalLayout interface
	virtual void SetActiveFaderGroupsInLayout(bool bActive) override;
	virtual void GenerateLayoutByControlConsoleData(const UDMXControlConsoleData* ControlConsoleData) override;
	//~ End UDMXControlConsoleBaseGlobalLayout interface

protected:
	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	//~ End UObject interface

private:
	/** Called when a Fixture Patch was removed from a DMX Library */
	void OnFixturePatchRemovedFromLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities);
	
	/** Called when a Fader Group was added to Control Console Data */
	void OnFaderGroupAddedToData(const UDMXControlConsoleFaderGroup* FaderGroup, UDMXControlConsoleData* ControlConsoleData);

	/** Called to clean this layout from all unpatched Fader Groups */
	void CleanLayoutFromUnpatchedFaderGroups();

	/** Array of currently active Fader Groups in the layout */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> ActiveFaderGroups;

	/** True if the layout is registered to DMX Library delegates */
	bool bIsRegistered = false;
};
