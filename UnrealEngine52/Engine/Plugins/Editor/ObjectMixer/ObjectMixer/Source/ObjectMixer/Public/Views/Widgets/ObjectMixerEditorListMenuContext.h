// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/List/ObjectMixerEditorListRowData.h"

#include "ToolMenuEntry.h"
#include "Types/SlateEnums.h"

#include "ObjectMixerEditorListMenuContext.generated.h"	

class UToolMenu;

UCLASS()
class OBJECTMIXEREDITOR_API UObjectMixerEditorListMenuContext : public UObject
{

	GENERATED_BODY()
	
public:

	struct FObjectMixerEditorListMenuContextData
	{
		TArray<TSharedPtr<ISceneOutlinerTreeItem>> SelectedItems;
		TWeakPtr<class FObjectMixerEditorList> ListModelPtr;
	};

	static void RegisterFoldersOnlyContextMenu();
	static void RegisterObjectMixerDynamicCollectionsContextMenuExtension(const FName& MenuName);

	FObjectMixerEditorListMenuContextData Data;

private:

	static bool DoesSelectionHaveType(const FObjectMixerEditorListMenuContextData& InData, UClass* Type);
	static void AddCollectionWidget(const FName& Key, const FObjectMixerEditorListMenuContextData& ContextData, UToolMenu* Menu);
	static void CreateSelectCollectionsSubMenu(UToolMenu* Menu, FObjectMixerEditorListMenuContextData ContextData);
	static void AddCollectionsMenuItem(UToolMenu* InMenu, const FObjectMixerEditorListMenuContextData& ContextData);
	static void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType, const FObjectMixerEditorListMenuContextData ContextData);
	
	static void OnCollectionMenuEntryCheckStateChanged(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);
	static void AddObjectsToCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);
	static void RemoveObjectsFromCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);
	static bool AreAllObjectsInCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);
	static ECheckBoxState GetCheckStateForCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);

	static FToolMenuEntry MakeCustomEditMenu(const FObjectMixerEditorListMenuContextData& ContextData);
	static void ReplaceEditSubMenu(const FObjectMixerEditorListMenuContextData& ContextData);
};
