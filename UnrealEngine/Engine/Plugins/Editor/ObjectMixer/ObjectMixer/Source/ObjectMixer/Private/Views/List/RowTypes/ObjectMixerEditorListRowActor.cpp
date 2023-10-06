// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/List/RowTypes/ObjectMixerEditorListRowActor.h"

#include "Views/List/SObjectMixerEditorList.h"

#include "ScopedTransaction.h"
#include "GameFramework/Actor.h"

const FSceneOutlinerTreeItemType FObjectMixerEditorListRowActor::Type(&FActorTreeItem::Type);

void FObjectMixerEditorListRowActor::OnVisibilityChanged(const bool bNewVisibility)
{
	RowData.OnChangeVisibility(SharedThis(this), bNewVisibility);

	if (TSharedPtr<SObjectMixerEditorList> ListView = RowData.GetListView().Pin())
	{
		ListView->EvaluateAndSetEditorVisibilityPerRow();
	}
}
