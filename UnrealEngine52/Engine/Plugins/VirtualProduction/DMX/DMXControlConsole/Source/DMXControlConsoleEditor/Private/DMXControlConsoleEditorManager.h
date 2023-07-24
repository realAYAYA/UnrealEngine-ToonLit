// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"

class UDMXControlConsole;
class UDMXControlConsoleData;
class UDMXControlConsoleEditorModel;
class FDMXControlConsoleEditorSelection;


/** Manages lifetime and provides access to the DMX Control Console */
class FDMXControlConsoleEditorManager final
	: public TSharedFromThis<FDMXControlConsoleEditorManager>
{
public:
	/** Destructor */
	~FDMXControlConsoleEditorManager();

	/** Gets a reference to DMX Editor's DMX Control Console manager */
	static FDMXControlConsoleEditorManager& Get();

	/** Returns the Control Console currently being edited, or nullptr if no console is being edited. */
	UDMXControlConsole* GetEditorConsole() const;

	/** Gets data of the Control Console currently being edited */
	UDMXControlConsoleData* GetEditorConsoleData() const;

	/** Gets a reference to the Selection Handler*/
	TSharedRef<FDMXControlConsoleEditorSelection> GetSelectionHandler();

	/** Sends DMX on the Control Console */
	void SendDMX();

	/** Stops sending DMX data from the Control Console */
	void StopDMX();

	/** Gets wheter the Control Console is sending DMX data or not */
	bool IsSendingDMX() const;

	/** True if DMX data sending can be played */
	bool CanSendDMX() const { return !IsSendingDMX(); }

	/** True if DMX data sending can be stopped */
	bool CanStopDMX() const { return IsSendingDMX(); };

	/** Resets DMX Control Console */
	void ClearAll();

private:
	/** Private constructor. Use FDMXControlConsoleManager::Get() instead. */
	FDMXControlConsoleEditorManager();

	/** Called before the engine is shut down */
	void OnEnginePreExit();

	/** Selection for the DMX Control Console */
	TSharedPtr<FDMXControlConsoleEditorSelection> SelectionHandler;

	/** The DMX Control Console manager instance */
	static TSharedPtr<FDMXControlConsoleEditorManager> Instance;
};
