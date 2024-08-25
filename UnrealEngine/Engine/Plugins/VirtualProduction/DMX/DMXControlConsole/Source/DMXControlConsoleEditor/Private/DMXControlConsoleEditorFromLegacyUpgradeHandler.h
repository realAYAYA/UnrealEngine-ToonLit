// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsole.h"
#include "UObject/WeakObjectPtrTemplates.h"

struct FDMXOutputConsoleFaderDescriptor;
class UDMXControlConsoleData;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleRawFader;


/** Handler to upgrade path from Control Console previous versions */
class FDMXControlConsoleEditorFromLegacyUpgradeHandler
{
public:
	/** Tries to upgrade configuration settings from previous DMXControlConsole versions. Returns true if the upgrade path was taken. */
	static bool TryUpgradePathFromLegacy();

private:
	/** Creates Fader Groups based on FaderDescriptor array. Used only for compatibility with DMXControlConsole previous versions */
	static UDMXControlConsoleData* CreateControlConsoleDataFromFaderDescriptorArray(const TArray<FDMXOutputConsoleFaderDescriptor>& FaderDescriptorArray);

	/** Creates a raw fader for the given Fader Group based on a FaderDescriptor. Used only for compatibility with DMXControlConsole previous versions */
	static UDMXControlConsoleRawFader* CreateRawFaderFromFaderDescriptor(UDMXControlConsoleFaderGroup* FaderGroup, const FDMXOutputConsoleFaderDescriptor& FaderDescriptor);

	/** Called when UpgradePathControlConsole asset is saved */
	static void OnUpgradePathControlConsoleSaved(const UDMXControlConsole* ControlConsole);

	/** Weak reference to the Control Console created by upgrade path process */
	static TWeakObjectPtr<UDMXControlConsole> UpgradePathControlConsole;
};
