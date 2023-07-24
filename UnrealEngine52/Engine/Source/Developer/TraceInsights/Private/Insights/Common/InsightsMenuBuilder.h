// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UICommandInfo.h"

class FWorkspaceItem;
class FMenuBuilder;

struct FUIAction;

class FInsightsMenuBuilder : public TSharedFromThis<FInsightsMenuBuilder>
{
public:
	FInsightsMenuBuilder();

	TSharedRef<FWorkspaceItem> GetInsightsToolsGroup();
	TSharedRef<FWorkspaceItem> GetWindowsGroup();

	void PopulateMenu(FMenuBuilder& InOutMenuBuilder);
	void BuildOpenTraceFileSubMenu(FMenuBuilder& InOutMenuBuilder);

	// Adds a menu entry with a custom key binding text.
	static void AddMenuEntry(
		FMenuBuilder& InOutMenuBuilder,
		const FUIAction& InAction,
		const TAttribute<FText>& InLabel,
		const TAttribute<FText>& InToolTipText,
		const TAttribute<FText>& InKeybinding,
		const EUserInterfaceActionType InUserInterfaceActionType);

private:
#if !WITH_EDITOR
	TSharedRef<FWorkspaceItem> InsightsToolsGroup;
	TSharedRef<FWorkspaceItem> WindowsGroup;
#endif
};
