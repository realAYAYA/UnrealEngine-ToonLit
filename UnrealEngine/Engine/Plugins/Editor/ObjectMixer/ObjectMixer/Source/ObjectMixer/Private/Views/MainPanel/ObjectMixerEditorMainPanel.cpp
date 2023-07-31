// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/MainPanel/ObjectMixerEditorMainPanel.h"

#include "ObjectMixerEditorSerializedData.h"
#include "Views/List/ObjectMixerEditorList.h"
#include "Views/MainPanel/SObjectMixerEditorMainPanel.h"

void FObjectMixerEditorMainPanel::Init()
{
	RegenerateListModel();
}

TSharedRef<SWidget> FObjectMixerEditorMainPanel::GetOrCreateWidget()
{
	if (!MainPanelWidget.IsValid())
	{
		SAssignNew(MainPanelWidget, SObjectMixerEditorMainPanel, SharedThis(this));
	}

	return MainPanelWidget.ToSharedRef();
}

void FObjectMixerEditorMainPanel::RegenerateListModel()
{
	EditorListModel.Reset();
	
	EditorListModel = MakeShared<FObjectMixerEditorList>(SharedThis(this));
}

void FObjectMixerEditorMainPanel::RequestRebuildList() const
{
	if (EditorListModel.IsValid())
	{
		EditorListModel->RequestRebuildList();
	}
}

void FObjectMixerEditorMainPanel::RefreshList() const
{
	if (EditorListModel.IsValid())
	{
		EditorListModel->RefreshList();
	}
}

void FObjectMixerEditorMainPanel::RequestSyncEditorSelectionToListSelection() const
{
	if (EditorListModel.IsValid())
	{
		EditorListModel->RequestSyncEditorSelectionToListSelection();
	}
}

void FObjectMixerEditorMainPanel::RebuildCollectionSelector()
{
	MainPanelWidget->RebuildCollectionSelector();
}

FText FObjectMixerEditorMainPanel::GetSearchTextFromSearchInputField() const
{
	return MainPanelWidget->GetSearchTextFromSearchInputField();
}

FString FObjectMixerEditorMainPanel::GetSearchStringFromSearchInputField() const
{
	return MainPanelWidget->GetSearchStringFromSearchInputField();
}

void FObjectMixerEditorMainPanel::OnClassSelectionChanged(UClass* InNewClass)
{
	SetObjectFilterClass(InNewClass);
}

TObjectPtr<UClass> FObjectMixerEditorMainPanel::GetClassSelection() const
{
	return GetObjectFilterClass();
}

bool FObjectMixerEditorMainPanel::IsClassSelected(UClass* InNewClass) const
{
	return InNewClass == GetClassSelection();
}

UObjectMixerObjectFilter* FObjectMixerEditorMainPanel::GetObjectFilter()
{
	if (!ObjectFilterPtr.IsValid())
	{
		CacheObjectFilterObject();
	}

	return ObjectFilterPtr.Get();
}

void FObjectMixerEditorMainPanel::CacheObjectFilterObject()
{
	if (ObjectFilterPtr.IsValid())
	{
		ObjectFilterPtr.Reset();
	}
	
	if (const UClass* Class = GetObjectFilterClass())
	{
		ObjectFilterPtr = TStrongObjectPtr(NewObject<UObjectMixerObjectFilter>(GetTransientPackage(), Class));
	}
}

const TArray<TSharedRef<IObjectMixerEditorListFilter>>& FObjectMixerEditorMainPanel::GetListFilters() const
{
	return MainPanelWidget->GetListFilters();	
}

TArray<TWeakPtr<IObjectMixerEditorListFilter>> FObjectMixerEditorMainPanel::GetWeakActiveListFiltersSortedByName() const
{
	return MainPanelWidget->GetWeakActiveListFiltersSortedByName();
}

UObjectMixerEditorSerializedData* FObjectMixerEditorMainPanel::GetSerializedDataOutputtingFilterName(FName& OutFilterName) const
{
	if (const TSubclassOf<UObjectMixerObjectFilter> Filter = GetObjectFilterClass())
	{
		OutFilterName = Filter->GetFName();
		return GetMutableDefault<UObjectMixerEditorSerializedData>();
	}

	return nullptr;
}

bool FObjectMixerEditorMainPanel::RequestAddObjectsToCollection(const FName& CollectionName, const TSet<FSoftObjectPath>& ObjectsToAdd) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->AddObjectsToCollection(FilterName, CollectionName, ObjectsToAdd);
	}

	return false;
}

bool FObjectMixerEditorMainPanel::RequestRemoveObjectsFromCollection(const FName& CollectionName, const TSet<FSoftObjectPath>& ObjectsToRemove) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->RemoveObjectsFromCollection(FilterName, CollectionName, ObjectsToRemove);
	}

	return false;
}

bool FObjectMixerEditorMainPanel::RequestRemoveCollection(const FName& CollectionName) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->RemoveCollection(FilterName, CollectionName);
	}

	return false;
}

bool FObjectMixerEditorMainPanel::RequestDuplicateCollection(const FName& CollectionToDuplicateName, FName& DesiredDuplicateName) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->DuplicateCollection(FilterName, CollectionToDuplicateName, DesiredDuplicateName);
	}

	return false;
}

bool FObjectMixerEditorMainPanel::RequestReorderCollection(const FName& CollectionToMoveName, const FName& CollectionInsertBeforeName) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->ReorderCollection(FilterName, CollectionToMoveName, CollectionInsertBeforeName);
	}

	return false;
}

bool FObjectMixerEditorMainPanel::RequestRenameCollection(const FName& CollectionNameToRename,
	const FName& NewCollectionName) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->RenameCollection(FilterName, CollectionNameToRename, NewCollectionName);
	}

	return false;
}

bool FObjectMixerEditorMainPanel::DoesCollectionExist(const FName& CollectionName) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->DoesCollectionExist(FilterName, CollectionName);
	}

	return false;
}

bool FObjectMixerEditorMainPanel::IsObjectInCollection(const FName& CollectionName, const FSoftObjectPath& InObject) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->IsObjectInCollection(FilterName, CollectionName, InObject);
	}

	return false;
}

TSet<FName> FObjectMixerEditorMainPanel::GetCollectionsForObject(const FSoftObjectPath& InObject) const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->GetCollectionsForObject(FilterName, InObject);
	}

	return {};
}

TArray<FName> FObjectMixerEditorMainPanel::GetAllCollectionNames() const
{
	FName FilterName;
	if (UObjectMixerEditorSerializedData* SerializedData = GetSerializedDataOutputtingFilterName(FilterName))
	{
		return SerializedData->GetAllCollectionNames(FilterName);
	}

	return {};
}

TSet<TSharedRef<FObjectMixerEditorListFilter_Collection>> FObjectMixerEditorMainPanel::GetCurrentCollectionSelection() const
{
	return MainPanelWidget->GetCurrentCollectionSelection();
}
