// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "ToolMenus.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UAvaOutlinerItemsContext;

/** Context menu actions in outliner related to property animator */
class FAvaPropertyAnimatorEditorOutlinerContextMenu
{
public:
	/** Extend outliner context menu */
	static void OnExtendOutlinerContextMenu(UToolMenu* InToolMenu);

	/** Gets all pertinent context object in this context */
	static void GetContextObjects(const UAvaOutlinerItemsContext* InContext, TSet<UObject*>& OutObjects);
};