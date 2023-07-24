// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class FDMXControlConsoleEditorManager;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFaderGroup;


class FDMXControlConsoleEditorSelection
	: public TSharedFromThis<FDMXControlConsoleEditorSelection>
{
public:
	DECLARE_EVENT(FDMXControlConsoleEditorSelection, FDMXControlConsoleSelectionEvent)

	/** Constructor */
	FDMXControlConsoleEditorSelection(const TSharedRef<FDMXControlConsoleEditorManager>& InControlConsoleManager);

	/** Adds the given Fader Group to selection */
	void AddToSelection(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Adds the given Fader to selection */
	void AddToSelection(UDMXControlConsoleFaderBase* Fader);

	/** Removes the given Fader Group from selection */
	void RemoveFromSelection(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Removes the given Fader from selection */
	void RemoveFromSelection(UDMXControlConsoleFaderBase* Fader);

	/** Multiselects the Fader or Fader Group and the current selection */
	void Multiselect(UObject* FaderOrFaderGroupObject);

	/** Replaces the given selected Fader Group with the next available one */
	void ReplaceInSelection(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Replaces the given selected Fader with the next available one */
	void ReplaceInSelection(UDMXControlConsoleFaderBase* Fader);

	/** Gets wheter the given Fader Group is selected or not */
	bool IsSelected(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets wheter the given Fader is selected or not */
	bool IsSelected(UDMXControlConsoleFaderBase* Fader) const;

	/** Clears from selection alla Faders owned by the given FaderGroup */
	void ClearFadersSelection(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Clears all Selected Objects arrays */
	void ClearSelection();

	/** Gets Selected Fader Gorups array */
	TArray<TWeakObjectPtr<UObject>> GetSelectedFaderGroups() const { return SelectedFaderGroups; }

	/** Gets first selected Fader Group sorted by index */
	UDMXControlConsoleFaderGroup* GetFirstSelectedFaderGroup() const;
	
	/** Gets Selected Faders array */
	TArray<TWeakObjectPtr<UObject>> GetSelectedFaders() const { return SelectedFaders; }

	/** Gets first selected Fader sorted by index */
	UDMXControlConsoleFaderBase* GetFirstSelectedFader() const;

	/** Gets all selected Faders from the fiven Fader Group */
	TArray<UDMXControlConsoleFaderBase*> GetSelectedFadersFromFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Returns an event raised when the Selection changed */
	FDMXControlConsoleSelectionEvent& GetOnSelectionChanged() { return OnSelectionChanged; }

private:
	/** Updates the multi select anchor */
	void UpdateMultiSelectAnchor(UClass* PreferedClass);

	/** Weak reference to DMX DMX Control Console */
	TWeakPtr<FDMXControlConsoleEditorManager> WeakControlConsoleManager;

	/** Array of current selected Fader Groups */
	TArray<TWeakObjectPtr<UObject>> SelectedFaderGroups;

	/** Array of current selected Faders */
	TArray<TWeakObjectPtr<UObject>> SelectedFaders;

	/** Anchor while multi selecting */
	TWeakObjectPtr<UObject> MultiSelectAnchor;

	/** Called whenever current selection changes */
	FDMXControlConsoleSelectionEvent OnSelectionChanged;
};
