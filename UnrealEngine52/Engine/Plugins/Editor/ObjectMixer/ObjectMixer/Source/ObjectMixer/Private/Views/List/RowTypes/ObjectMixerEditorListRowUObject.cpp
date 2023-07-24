// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/List/RowTypes/ObjectMixerEditorListRowUObject.h"

const FSceneOutlinerTreeItemType FObjectMixerEditorListRowUObject::Type(&ISceneOutlinerTreeItem::Type);

FString FObjectMixerEditorListRowUObject::GetDisplayString() const
{
	return IsValid() ? ObjectSoftPtr.Get()->GetName() : FString();
}
