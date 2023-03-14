// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/TileView.h"
#include "CommonTileView.generated.h"

/**
 * TileView specialized to navigate on focus for consoles & enable scrolling when not focused for touch
 */
UCLASS(meta = (DisableNativeTick))
class COMMONUI_API UCommonTileView : public UTileView
{
	GENERATED_BODY()

public:
	UCommonTileView(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<STableViewBase> RebuildListWidget() override;
	virtual UUserWidget& OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<STableViewBase>& OwnerTable) override;
};