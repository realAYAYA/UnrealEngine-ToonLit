// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCLogicPanelListBase.h"

#include "Commands/RemoteControlCommands.h"
#include "Interfaces/IMainFrameModule.h"
#include "SRCLogicPanelBase.h"
#include "UI/SRemoteControlPanel.h"

#define LOCTEXT_NAMESPACE "SRCLogicPanelListBase"

void SRCLogicPanelListBase::Construct(const FArguments& InArgs, const TSharedRef<SRCLogicPanelBase>& InLogicParentPanel, const TSharedRef<SRemoteControlPanel>& InPanel)
{
	LogicPanelWeakPtr = InLogicParentPanel;
	RemoteControlPanelWeakPtr = InPanel;
}

TSharedPtr<SWidget> SRCLogicPanelListBase::GetContextMenuWidget()
{
	IMainFrameModule& MainFrame = FModuleManager::Get().LoadModuleChecked<IMainFrameModule>("MainFrame");

	FMenuBuilder MenuBuilder(true, MainFrame.GetMainFrameCommandBindings());

	// Special menu options
	MenuBuilder.BeginSection("Advanced");
	AddSpecialContextMenuOptions(MenuBuilder);
	MenuBuilder.EndSection();

	// Generic options (based on UI Commands)
	MenuBuilder.BeginSection("Common");

	const FRemoteControlCommands& Commands = FRemoteControlCommands::Get();

	// 1. Copy
	MenuBuilder.AddMenuEntry(Commands.CopyItem);

	// 2. Paste
	FText PasteItemLabel = LOCTEXT("Paste", "Paste");
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = RemoteControlPanelWeakPtr.Pin())
	{
		if (TSharedPtr<SRCLogicPanelBase> LogicPanel = RemoteControlPanel->LogicClipboardItemSource)
		{
			const FText Suffix = LogicPanel->GetPasteItemMenuEntrySuffix();
			if (!Suffix.IsEmpty())
			{
				PasteItemLabel = FText::Format(FText::FromString("{0} ({1})"), PasteItemLabel, Suffix);
			}
		}
	}
	MenuBuilder.AddMenuEntry(Commands.PasteItem, NAME_None, PasteItemLabel);

	// 2. Duplicate
	MenuBuilder.AddMenuEntry(Commands.DuplicateItem);

	// 3. Update
	MenuBuilder.AddMenuEntry(Commands.UpdateValue);

	// 4. Delete
	MenuBuilder.AddMenuEntry(Commands.DeleteEntity);

	// 5. Delete All
	FUIAction Action(FExecuteAction::CreateSP(this, &SRCLogicPanelListBase::RequestDeleteAllItems)
	, FCanExecuteAction::CreateSP(this, &SRCLogicPanelListBase::CanDeleteAllItems));

	MenuBuilder.AddMenuEntry(LOCTEXT("DeleteAll", "Delete All"),
		LOCTEXT("ContextMenuEditTooltip", "Delete all the rows in this list"),
		FSlateIcon(), Action);

	MenuBuilder.EndSection();

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	ContextMenuWidgetCached = MenuWidget;

	return MenuWidget;
}

bool SRCLogicPanelListBase::CanDeleteAllItems() const
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = RemoteControlPanelWeakPtr.Pin())
	{
		return !RemoteControlPanel->IsInLiveMode();
	}

	return false;
}

void SRCLogicPanelListBase::RequestDeleteAllItems()
{
	if (TSharedPtr<SRCLogicPanelBase> LogicParentPanel = LogicPanelWeakPtr.Pin())
	{
		LogicParentPanel->RequestDeleteAllItems();
	}
}

#undef LOCTEXT_NAMESPACE