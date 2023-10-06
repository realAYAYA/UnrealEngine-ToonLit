// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class FDMXControlConsoleEditorManager;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFaderGroup;


class FDMXControlConsoleEditorSelection final
	: public TSharedFromThis<FDMXControlConsoleEditorSelection>
{
public:
	DECLARE_EVENT(FDMXControlConsoleEditorSelection, FDMXControlConsoleSelectionEvent)

	/** Adds the given Fader Group to selection */
	void AddToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Adds the given Fader to selection */
	void AddToSelection(UDMXControlConsoleFaderBase* Fader, bool bNotifySelectionChange = true);

	/** Adds the elements in the given array to selection */
	void AddToSelection(const TArray<UObject*> Elements, bool bNotifySelectionChange = true);

	/** Adds to selection all Faders from the given Fader Group */
	void AddAllFadersFromFaderGroupToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bOnlyMatchingFilter = false, bool bNotifySelectionChange = true);

	/** Removes the given Fader Group from selection */
	void RemoveFromSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Removes the given Fader from selection */
	void RemoveFromSelection(UDMXControlConsoleFaderBase* Fader, bool bNotifySelectionChange = true);

	/** Removes the elements in the given array from selection */
	void RemoveFromSelection(const TArray<UObject*> Elements, bool bNotifySelectionChange = true);

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

	/** Selects all Fader Groups and Faders in the current Control Console Data */
	void SelectAll(bool bOnlyMatchingFilter = false);

	/** Cleans selection from array elements which are no longer valid */
	void RemoveInvalidObjectsFromSelection(bool bNotifySelectionChange = true);

	/** Clears from selection alla Faders owned by the given FaderGroup */
	void ClearFadersSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Clears all Selected Objects arrays */
	void ClearSelection(bool bNotifySelectionChange = true);

	/** Gets Selected Fader Gorups array */
	TArray<TWeakObjectPtr<UObject>> GetSelectedFaderGroups() const { return SelectedFaderGroups; }

	/** Gets first selected Fader Group sorted by index */
	UDMXControlConsoleFaderGroup* GetFirstSelectedFaderGroup(bool bReverse = false) const;

	/** Gets Selected Faders array */
	TArray<TWeakObjectPtr<UObject>> GetSelectedFaders() const { return SelectedFaders; }

	/** Gets first selected Fader sorted by index */
	UDMXControlConsoleFaderBase* GetFirstSelectedFader(bool bReverse = false) const;

	/** Gets all selected Faders from the fiven Fader Group */
	TArray<UDMXControlConsoleFaderBase*> GetSelectedFadersFromFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Returns an event raised when the Selection changed */
	FDMXControlConsoleSelectionEvent& GetOnSelectionChanged() { return OnSelectionChanged; }

private:
	/** Updates the multi select anchor */
	void UpdateMultiSelectAnchor(UClass* PreferedClass);

	/** Array of current selected Fader Groups */
	TArray<TWeakObjectPtr<UObject>> SelectedFaderGroups;

	/** Array of current selected Faders */
	TArray<TWeakObjectPtr<UObject>> SelectedFaders;

	/** Anchor while multi selecting */
	TWeakObjectPtr<UObject> MultiSelectAnchor;

	/** Called whenever current selection changes */
	FDMXControlConsoleSelectionEvent OnSelectionChanged;
};
