// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackParameterStoreEntryName.h"
#include "NiagaraEditorStyle.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackParameterStoreEntry.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SNiagaraParameterName.h"

#define LOCTEXT_NAMESPACE "NiagaraStackParameterStoreEntryName"

void SNiagaraStackParameterStoreEntryName::Construct(const FArguments& InArgs, UNiagaraStackParameterStoreEntry* InStackEntry, UNiagaraStackViewModel* InStackViewModel)
{
	StackEntry = InStackEntry;
	StackEntryItem = InStackEntry;
	StackViewModel = InStackViewModel;
	IsSelected = InArgs._IsSelected;

	ChildSlot
	[
		SNew(SHorizontalBox)
		// Name Label
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(NameTextBlock, SNiagaraParameterNameTextBlock)
			.EditableTextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterInlineEditableText")
			.ParameterText_UObject(StackEntry, &UNiagaraStackEntry::GetDisplayName)
			.IsReadOnly(StackEntry->SupportsRename() == false)
			.IsSelected(this, &SNiagaraStackParameterStoreEntryName::GetIsNameWidgetSelected)
			.OnTextCommitted(this, &SNiagaraStackParameterStoreEntryName::OnNameTextCommitted)
			.OnVerifyTextChanged(this, &SNiagaraStackParameterStoreEntryName::VerifyNameTextChanged)
			//.HighlightText_UObject(InStackViewModel, &UNiagaraStackViewModel::GetCurrentSearchText)
		]
	];
}

void SNiagaraStackParameterStoreEntryName::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// for remote initiating rename when the widget is added
	if (StackEntry->GetIsRenamePending() && NameTextBlock.IsValid())
	{
		NameTextBlock->EnterEditingMode();
	}
	StackEntry->SetIsRenamePending(false);
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

bool SNiagaraStackParameterStoreEntryName::GetIsNameWidgetSelected() const
{
	return IsSelected.Get();
}

bool SNiagaraStackParameterStoreEntryName::VerifyNameTextChanged(const FText& NewText, FText& OutErrorMessage)
{
	if (NewText.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("NPCNameEmptyWarn", "Cannot have empty name!");
		return false;
	}
	if (!StackEntry->IsUniqueName(NewText.ToString()))
	{
		OutErrorMessage = FText::Format(LOCTEXT("NPCNameConflictWarn", "\"{0}\" is already the name of another parameter in this collection."), NewText);
		return false;
	}

	return true;
}

void SNiagaraStackParameterStoreEntryName::OnNameTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if (!StackEntry->GetDisplayName().EqualTo(InText))
	{
		StackEntry->OnRenamed(InText);
		// toast notification
		FNotificationInfo Info(FText::Format(LOCTEXT("NiagaraRenamedUserParameter", "System exposed parameter was renamed.\n{0}\n(All links to inner variables were updated in the process.)"), StackEntry->GetDisplayName()));
		Info.ExpireDuration = 5.0f;
		Info.bFireAndForget = true;
		Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Note"));
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

#undef LOCTEXT_NAMESPACE