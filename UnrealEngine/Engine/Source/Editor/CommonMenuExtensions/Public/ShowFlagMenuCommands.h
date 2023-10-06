// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EditorViewportClient.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "ShowFlagFilter.h"
#include "ShowFlags.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Textures/SlateIcon.h"
#include "UObject/NameTypes.h"

class FEditorViewportClient;
class FShowFlagData;
class FUICommandInfo;
class FUICommandList;
class UToolMenu;
struct FToolMenuSection;

class COMMONMENUEXTENSIONS_API FShowFlagMenuCommands : public TCommands<FShowFlagMenuCommands>
{
public:
	struct FShowFlagCommand
	{
		FEngineShowFlags::EShowFlag FlagIndex;
		TSharedPtr<FUICommandInfo> ShowMenuItem;
		FText LabelOverride;

		FShowFlagCommand(FEngineShowFlags::EShowFlag InFlagIndex, const TSharedPtr<FUICommandInfo>& InShowMenuItem, const FText& InLabelOverride)
			: FlagIndex(InFlagIndex),
			  ShowMenuItem(InShowMenuItem),
			  LabelOverride(InLabelOverride)
		{
		}

		FShowFlagCommand(FEngineShowFlags::EShowFlag InFlagIndex, const TSharedPtr<FUICommandInfo>& InShowMenuItem)
			: FlagIndex(InFlagIndex),
			  ShowMenuItem(InShowMenuItem),
			  LabelOverride()
		{
		}
	};

	FShowFlagMenuCommands();

	virtual void RegisterCommands() override;

	void BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const;
	void BuildShowFlagsMenu(UToolMenu* Menu, const FShowFlagFilter& Filter = FShowFlagFilter(FShowFlagFilter::IncludeAllFlagsByDefault)) const;

private:
	static void StaticCreateShowFlagsSubMenu(UToolMenu* Menu, TArray<uint32> FlagIndices, int32 EntryOffset);
	static void ToggleShowFlag(TWeakPtr<FEditorViewportClient> WeakClient, FEngineShowFlags::EShowFlag EngineShowFlagIndex);
	static bool IsShowFlagEnabled(TWeakPtr<FEditorViewportClient> WeakClient, FEngineShowFlags::EShowFlag EngineShowFlagIndex);

	FSlateIcon GetShowFlagIcon(const FShowFlagData& Flag) const;

	void CreateShowFlagCommands();
	void UpdateCustomShowFlags() const;
	void CreateCommonShowFlagMenuItems(UToolMenu* Menu, const FShowFlagFilter& Filter) const;
	void CreateSubMenuIfRequired(FToolMenuSection& Section, const FShowFlagFilter& Filter, EShowFlagGroup Group, const FName SubMenuName, const FText& MenuLabel, const FText& ToolTip, const FName IconName) const;
	void CreateShowFlagsSubMenu(UToolMenu* Menu, TArray<uint32> MenuCommands, int32 EntryOffset) const;

	TArray<FShowFlagCommand> ShowFlagCommands;
	bool bCommandsInitialised;
};