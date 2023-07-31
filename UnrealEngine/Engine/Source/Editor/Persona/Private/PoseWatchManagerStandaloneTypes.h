// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "UObject/ObjectKey.h"
#include "Templates/MaxSizeof.h"

struct IPoseWatchManagerTreeItem;
class IPoseWatchManager;

typedef TSharedPtr<IPoseWatchManagerTreeItem> FPoseWatchManagerTreeItemPtr;
typedef TSharedRef<IPoseWatchManagerTreeItem> FPoseWatchManagerTreeItemRef;

typedef TMap<FObjectKey, FPoseWatchManagerTreeItemPtr> FPoseWatchManagerTreeItemMap;

enum EPoseWatchTreeItemType
{
	PoseWatch,
	Folder,
	Element
};

struct PERSONA_API FPoseWatchManagerCommonLabelData
{
	TWeakPtr<IPoseWatchManager> WeakPoseWatchManager;
	static const FLinearColor DisabledColor;

	TOptional<FLinearColor> GetForegroundColor(const IPoseWatchManagerTreeItem& TreeItem) const;

	bool CanExecuteRenameRequest(const IPoseWatchManagerTreeItem& Item) const;
};


struct FPoseWatchManagerHierarchyChangedData
{
	enum
	{
		Added,
		FullRefresh
	} Type;

	// This event may pass one of two kinds of data, depending on the type of event
	TArray<FPoseWatchManagerTreeItemPtr> Items;

	/** Actions to apply to items */
	uint8 ItemActions = 0;
};
