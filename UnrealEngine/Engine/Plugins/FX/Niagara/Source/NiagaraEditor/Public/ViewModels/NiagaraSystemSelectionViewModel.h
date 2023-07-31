// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectKey.h"
#include "NiagaraSystemSelectionViewModel.generated.h"


class FNiagaraSystemViewModel;
class UNiagaraStackEntry;
class UNiagaraStackSelection;
class UNiagaraStackViewModel;

UCLASS()
class NIAGARAEDITOR_API UNiagaraSystemSelectionViewModel : public UObject
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnSelectionChanged);

private:
	struct FSelectionEntry
	{
		explicit FSelectionEntry(UNiagaraStackEntry* SelectedEntry);

		const TWeakObjectPtr<UNiagaraStackEntry> Entry;
		const FGuid EmitterGuid;
		const FString EditorDataKey;
	};

public:
	GENERATED_BODY()

	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);

	void Finalize();

	bool ContainsEntry(UNiagaraStackEntry* StackEntry) const;

	void GetSelectedEntries(TArray<UNiagaraStackEntry*>& OutSelectedEntries) const;

	bool GetSystemIsSelected() const;

	const TArray<FGuid>& GetSelectedEmitterHandleIds() const;

	void UpdateSelectedEntries(const TArray<UNiagaraStackEntry*> InSelectedEntries, const TArray<UNiagaraStackEntry*> InDeselectedEntries, bool bClearCurrentSelection);

	UNiagaraStackViewModel* GetSelectionStackViewModel();

	void EmptySelection();

	void RemoveEntriesFromSelection(const TArray<UNiagaraStackEntry*>& InEntriesToRemove);

	void RemoveEntryFromSelectionByDisplayedObject(const UObject* InObject);

	void AddEntriesToSelectionByDisplayedObjectsDeferred(const TArray<const UObject*>& InObjects);

	void AddEntryToSelectionByDisplayedObjectDeferred(const UObject* InObjects);

	void AddEntriesToSelectionByDisplayedObjectKeysDeferred(const TArray<FObjectKey>& InObjectKeys);

	void AddEntryToSelectionByDisplayedObjectKeyDeferred(const FObjectKey& InObjectKey);

	void AddEntryToSelectionBySelectionIdDeferred(const FGuid& InSelectionId);

	bool Refresh();

	void RefreshDeferred();

	FOnSelectionChanged& OnEntrySelectionChanged();

	FOnSelectionChanged& OnEmitterHandleIdSelectionChanged();

	FOnSelectionChanged& OnSystemIsSelectedChanged();

	void Tick();

private:
	TSharedRef<FNiagaraSystemViewModel> GetSystemViewModel();

	void RemoveEntryFromSelectionInternal(UNiagaraStackEntry* DeselectedEntry);

	void UpdateExternalSelectionState();

private:
	TWeakPtr<FNiagaraSystemViewModel> SystemViewModelWeak;

	TArray<FSelectionEntry> SelectionEntries;

	bool bSystemIsSelected;

	TArray<FGuid> SelectedEmitterHandleIds;

	UPROPERTY()
	TObjectPtr<UNiagaraStackSelection> StackSelection;

	UPROPERTY()
	TObjectPtr<UNiagaraStackViewModel> SelectionStackViewModel;

	FOnSelectionChanged OnEntrySelectionChangedDelegate;

	FOnSelectionChanged OnEmitterHandleIdSelectionChangedDelegate;

	FOnSelectionChanged OnSystemIsSelectedChangedDelegate;

	TArray<FObjectKey> DeferredDisplayedObjectKeysToAddToSelection;

	TArray<FGuid> DeferredSelectionIdsToAddToSelection;

	bool bRefreshIsPending;
};