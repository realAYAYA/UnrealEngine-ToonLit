// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class IWorldPartitionEditorModule;

class UNREALED_API SWorldPartitionViewportWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldPartitionViewportWidget) {}
	SLATE_ARGUMENT(bool, Clickable)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SWorldPartitionViewportWidget();

	EVisibility GetVisibility(UWorld* InWorld);

private:
	bool bClickable;
	FText Message;
	FText Tooltip;
	FName InvokeTab;
	IWorldPartitionEditorModule* WorldPartitionEditorModule;
};