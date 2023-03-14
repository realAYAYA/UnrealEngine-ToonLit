// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEditorUtilities.h"

/** A set of selected objects which calls a delegate any time it is changed. */
template<typename SelectedItemType>
class TNiagaraSelection
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnSelectedObjectsChanged);

public:
	/** Gets the set of selected objects. */
	const TSet<SelectedItemType>& GetSelectedObjects() const
	{
		return SelectedObjects;
	}

	const void* GetAdditionalSelectionInfo() const
	{
		return AdditionalSelectionInfo;
	}

	/** Replaces the currently selected set of objects with the supplied object. */
	void SetSelectedObject(SelectedItemType SelectedObject, const void* InSelectionInfo)
	{
		AdditionalSelectionInfo = InSelectionInfo;
		if (SelectedObjects.Num() == 1 && SelectedObjects.Contains(SelectedObject))
		{
			// Refresh the delegate, in case a different object selection has been used in 
			// a shared panel (but using a different selection, so this selection would not change)
			OnSelectedObjectsChangedDelegate.Broadcast();
			return;
		}

		SelectedObjects.Empty();
		SelectedObjects.Add(SelectedObject);
		OnSelectedObjectsChangedDelegate.Broadcast();
	}

	/** Replaces the currently selected set of objects with the supplied object. */
	void SetSelectedObject(SelectedItemType SelectedObject)
	{
		SetSelectedObject(SelectedObject, nullptr);
	}

	/** Replaces the currently selected set of objects with the supplied set. */
	void SetSelectedObjects(const TSet<SelectedItemType>& InSelectedObjects)
	{
		if (FNiagaraEditorUtilities::SetsMatch(SelectedObjects, InSelectedObjects) == false)
		{
			SelectedObjects.Empty();
			AdditionalSelectionInfo = nullptr;
			SelectedObjects = InSelectedObjects;
			OnSelectedObjectsChangedDelegate.Broadcast();
		}
	}

	/** Replaces the currently selected set of objects with the supplied array. */
	void SetSelectedObjects(const TArray<SelectedItemType>& InSelectedObjects)
	{
		if (FNiagaraEditorUtilities::ArrayMatchesSet(InSelectedObjects, SelectedObjects) == false)
		{
			SelectedObjects.Empty();
			AdditionalSelectionInfo = nullptr;
			SelectedObjects.Append(InSelectedObjects);
			OnSelectedObjectsChangedDelegate.Broadcast();
		}
	}

	/** Empties the currently selected set of objects. */
	void ClearSelectedObjects()
	{
		if (SelectedObjects.Num() > 0)
		{
			SelectedObjects.Empty();
			AdditionalSelectionInfo = nullptr;
			OnSelectedObjectsChangedDelegate.Broadcast();
		}
	}

	/** Gets a multicast delegate which is called any time the set of selected objects is changed. */
	FOnSelectedObjectsChanged& OnSelectedObjectsChanged()
	{
		return OnSelectedObjectsChangedDelegate;
	}

	/** Refresh all views subscribed to OnSelectedObjectsChanged. */
	void Refresh()
	{
		OnSelectedObjectsChangedDelegate.Broadcast();
	}

private:
	/** The set of selected objects. */
	TSet<SelectedItemType> SelectedObjects;

	/** The delegate which is called whenever the set of selected objects changes. */
	FOnSelectedObjectsChanged OnSelectedObjectsChangedDelegate;

	/** Any additional data payload for the selection. */
	const void* AdditionalSelectionInfo = nullptr;
};

class FNiagaraObjectSelection : public TNiagaraSelection<UObject*>
{
};