// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/List/ObjectMixerEditorListRow.h"

#include "Types/SlateEnums.h"

#include "ObjectMixerEditorListMenuContext.generated.h"	

UCLASS()
class OBJECTMIXEREDITOR_API UObjectMixerEditorListMenuContext : public UObject
{

	GENERATED_BODY()
	
public:

	struct FObjectMixerEditorListMenuContextData
	{
		TArray<FObjectMixerEditorListRowPtr> SelectedItems;
		TWeakPtr<class FObjectMixerEditorMainPanel> MainPanelPtr;
	};

	static TSharedPtr<SWidget> CreateContextMenu(const FObjectMixerEditorListMenuContextData InData);
	static TSharedPtr<SWidget> BuildContextMenu(const FObjectMixerEditorListMenuContextData& InData);
	static void RegisterObjectMixerContextMenu();

	FObjectMixerEditorListMenuContextData Data;
	
	static FName DefaultContextBaseMenuName;

private:

	static void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType, const FObjectMixerEditorListMenuContextData ContextData);
	
	static void OnClickCollectionMenuEntry(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);
	static void AddObjectsToCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);
	static void RemoveObjectsFromCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);
	static bool AreAllObjectsInCollection(const FName Key, const FObjectMixerEditorListMenuContextData ContextData);
};
