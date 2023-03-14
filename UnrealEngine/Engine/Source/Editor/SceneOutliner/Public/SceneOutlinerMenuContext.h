// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "SceneOutlinerMenuContext.generated.h"

class SSceneOutliner;

UCLASS()
class SCENEOUTLINER_API USceneOutlinerMenuContext : public UObject
{
	GENERATED_BODY()
public:

	USceneOutlinerMenuContext(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		bShowParentTree = false;
		bRepresentingPartitionedWorld = false;
		bRepresentingGameWorld = false;
		NumSelectedItems = 0;
		NumSelectedFolders = 0;
		NumWorldsSelected = 0;
		NumPinnedItems = 0;
	}

	TWeakPtr<SSceneOutliner> SceneOutliner;

	bool bShowParentTree;
	bool bRepresentingGameWorld;
	bool bRepresentingPartitionedWorld;
	int32 NumSelectedItems;
	int32 NumSelectedFolders;
	int32 NumWorldsSelected;
	int32 NumPinnedItems;
};
