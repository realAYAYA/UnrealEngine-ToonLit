// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISceneOutlinerTreeItem.h"
#include "SceneOutlinerStandaloneTypes.h"

struct FGuid;

/** A tree item that represents an actor, loaded or unloaded */
struct SCENEOUTLINER_API IActorBaseTreeItem : ISceneOutlinerTreeItem
{
public:
	IActorBaseTreeItem(FSceneOutlinerTreeItemType InType) : ISceneOutlinerTreeItem(InType) {}

	/** Static type identifier for the base class tree item */
	static const FSceneOutlinerTreeItemType Type;

	virtual const FGuid& GetGuid() const =0;
};
