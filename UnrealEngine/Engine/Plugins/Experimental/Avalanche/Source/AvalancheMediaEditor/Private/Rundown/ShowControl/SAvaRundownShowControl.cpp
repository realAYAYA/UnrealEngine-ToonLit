// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaRundownShowControl.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Text.h"
#include "Playback/AvaPlaybackGraph.h"
#include "Rundown/AvaRundownCommands.h"
#include "Rundown/AvaRundownEditor.h"
#include "Rundown/Pages/Slate/SAvaRundownInstancedPageList.h"
#include "Rundown/Pages/Slate/SAvaRundownPageList.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "SAvaRundownShowControl"

void SAvaRundownShowControl::Construct(const FArguments& InArgs, const TSharedPtr<FAvaRundownEditor>& InRundownEditor)
{
	RundownEditorWeak = InRundownEditor;
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildShowControlToolBar(InRundownEditor->GetToolkitCommands())
		]
	];
}

TSharedRef<SWidget> SAvaRundownShowControl::BuildShowControlToolBar(const TSharedRef<FUICommandList>& InCommandList)
{
	FSlimHorizontalToolBarBuilder ToolBarBuilder(InCommandList, FMultiBoxCustomization::None);
	ToolBarBuilder.SetStyle(&FAvaMediaEditorStyle::Get(), "AvaMediaEditor.ToolBar");

	const FAvaRundownCommands& RundownCommands = FAvaRundownCommands::Get();

	ToolBarBuilder.BeginSection(TEXT("ShowControl"));
	{
		ToolBarBuilder.BeginStyleOverride("AvaMediaEditor.ToolBarRedButtonOverride");
		{
			ToolBarBuilder.AddToolBarButton(RundownCommands.Play);
		}
		ToolBarBuilder.EndStyleOverride();

		ToolBarBuilder.AddToolBarButton(RundownCommands.Continue);
		ToolBarBuilder.AddToolBarButton(RundownCommands.Stop);

		ToolBarBuilder.BeginStyleOverride("AvaMediaEditor.ToolBarRedButtonOverride");
		{
			ToolBarBuilder.AddToolBarButton(RundownCommands.PlayNext);
		}
		ToolBarBuilder.EndStyleOverride();

		ToolBarBuilder.AddSeparator();
		ToolBarBuilder.AddWidget(CreateActiveListWidget());
		ToolBarBuilder.AddSeparator();
		ToolBarBuilder.AddWidget(CreateNextPageWidget());
	}
	ToolBarBuilder.EndSection();

	return ToolBarBuilder.MakeWidget();
}

TSharedRef<SWidget> SAvaRundownShowControl::CreateActiveListWidget()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(5.f, 0.f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ActiveView", "Active View:"))
		]
		+ SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(0.f, 0.f, 5.f, 0.f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(this, &SAvaRundownShowControl::GetActiveListName)
		];
}

FText SAvaRundownShowControl::GetActiveListName() const
{
	if (TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin())
	{
		UAvaRundown* Rundown = RundownEditor->GetRundown();

		if (IsValid(Rundown))
		{
			const FAvaRundownPageListReference& ActiveList = Rundown->GetActivePageListReference();

			// Can't happen. Here for completeness.
			if (ActiveList.Type == EAvaRundownPageListType::Template)
			{
				static const FText Templates(LOCTEXT("Templates", "Templates"));
				return Templates;
			}

			if (ActiveList.Type == EAvaRundownPageListType::Instance)
			{
				static const FText AllPages(LOCTEXT("AllPages", "All Pages"));
				return AllPages;
			}

			return FText::Format(LOCTEXT("RundownSubListDocument_TabLabel", "Page View {0}"), FText::AsNumber(ActiveList.SubListIndex + 1));
		}
	}

	return FText::GetEmpty();
}

TSharedRef<SWidget> SAvaRundownShowControl::CreateNextPageWidget()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(5.f, 0.f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NextUp", "Next Up:"))
		]
		+ SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(0.f, 0.f, 5.f, 0.f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(this, &SAvaRundownShowControl::GetNextPageName)
		];
}

FText SAvaRundownShowControl::GetNextPageName() const
{
	static const FText NoPage(LOCTEXT("None", "-"));

	if (TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin())
	{
		if (const UAvaRundown* Rundown = RundownEditor->GetRundown(); IsValid(Rundown))
		{
			if (Rundown->GetInstancedPages().Pages.IsEmpty())
			{
				return NoPage;
			}

			const TSharedPtr<SAvaRundownInstancedPageList> ActiveSubListWidget = RundownEditor->GetActiveListWidget();

			if (!ActiveSubListWidget.IsValid() || ActiveSubListWidget->GetPlayingPageIds().IsEmpty())
			{
				return NoPage;
			}

			TArray<int32> NextUps = ActiveSubListWidget->GetPageIdsToTakeNext();

			FText NextUpMessage = FText::GetEmpty();
			FFormatOrderedArguments FormatOrderedArguments;
			
			for (const int32 NextUp : NextUps)
			{
				if (const int32* PageIndex = Rundown->GetInstancedPages().PageIndices.Find(NextUp))
				{
					FormatOrderedArguments.Add(FText::Format(
							LOCTEXT("NextUpFormat", "{0}: {1}"),
							FText::AsNumber(NextUp, &UE::AvaRundown::FEditorMetrics::PageIdFormattingOptions),
							FText::FromString(Rundown->GetInstancedPages().Pages[*PageIndex].GetPageName())
						));
				}
			}

			if (FormatOrderedArguments.Num() > 0)
			{
				return FText::Join(LOCTEXT("NextUpDelimiter", "\n"), FormatOrderedArguments);
			}
		}
	}

	return NoPage;
}

#undef LOCTEXT_NAMESPACE
