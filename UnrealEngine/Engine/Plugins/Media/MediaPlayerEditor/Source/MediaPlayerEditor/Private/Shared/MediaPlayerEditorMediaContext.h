// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ToolMenus.h"

#include "MediaPlayerEditorMediaContext.generated.h"

class SMediaPlayerEditorMedia;

UCLASS(BlueprintType)
class MEDIAPLAYEREDITOR_API UMediaPlayerEditorMediaContext : public UToolMenuContextBase
{
	GENERATED_BODY()

public:	

	void InitContext(UObject* InSelectedAsset, FName InStyleSetName = NAME_None)
	{
		SelectedAsset = InSelectedAsset;
		StyleSetName = InStyleSetName;
	}

	UPROPERTY(BlueprintReadWrite, Category = "Editor Menus")
	TObjectPtr<UObject> SelectedAsset;

	UPROPERTY(BlueprintReadWrite, Category = "Editor Menus")
	FName StyleSetName;

	TWeakPtr<SMediaPlayerEditorMedia> MediaPlayerEditorMedia;
};
