// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaRundownPageDetails.h"
#include "Async/Async.h"
#include "IAvaMediaModule.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "RemoteControl/Controllers/SAvaRundownRCControllerPanel.h"
#include "Rundown/AvaRundown.h"
#include "Rundown/AvaRundownEditor.h"
#include "Rundown/AvaRundownManagedInstanceCache.h"
#include "Rundown/AvaRundownPage.h"
#include "Rundown/DetailsView/RemoteControl/Properties/SAvaRundownPageRemoteControlProps.h"
#include "Rundown/Pages/Slate/SAvaRundownInstancedPageList.h"
#include "Rundown/Pages/Slate/SAvaRundownPageList.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaRundownPageDetails"

void SAvaRundownPageDetails::Construct(const FArguments& InArgs, const TSharedPtr<FAvaRundownEditor>& InRundownEditor)
{
	RundownEditorWeak = InRundownEditor;
	ActivePageId = FAvaRundownPage::InvalidPageId;

	InRundownEditor->GetOnPageEvent().AddSP(this, &SAvaRundownPageDetails::OnPageEvent);
	IAvaMediaModule::Get().GetManagedInstanceCache().OnEntryInvalidated.AddSP(this, &SAvaRundownPageDetails::OnManagedInstanceCacheEntryInvalidated);

	TSharedRef<SHorizontalBox> AnimationHeader = SNew(SHorizontalBox);
	{
		static const TArray<FText> HeaderTitles
    	{
    		LOCTEXT("HeaderTitleAnimationName", "Animation Name"),
    		LOCTEXT("HeaderTitleLoopsToPlay", "Loops to Play"),
    		LOCTEXT("HeaderTitlePlaybackSpeed", "Playback Speed"),
    		LOCTEXT("HeaderTitlePlayMode", "Play Mode")
    	};

    	for (const FText& Title : HeaderTitles)
    	{
    		AnimationHeader->AddSlot()
    			.FillWidth(1.f)
    			[
    				SNew(STextBlock)
    				.Text(Title)
    				.Justification(ETextJustify::Center)
    			];
    	}
	}

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10.f, 10.f, 10.f, 0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.MaxWidth(75.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PageId", "Page Id"))
					.MinDesiredWidth(75.f)
				]
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(5.f, 0.f, 0.f, 0.f)
				.MaxWidth(70.f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("PageIdHint", "Page Id"))
					.OnTextCommitted(this, &SAvaRundownPageDetails::OnPageIdCommitted)
					.Text(this, &SAvaRundownPageDetails::GetPageId)
					.IsEnabled(this, &SAvaRundownPageDetails::HasSelectedPage)
				]
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(5.f, 0.f, 0.f, 0.f)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("DuplicatePageTooltip", "DuplicatePage"))
					.OnClicked(this, &SAvaRundownPageDetails::DuplicateSelectedPage)
					.IsEnabled(this, &SAvaRundownPageDetails::HasSelectedPage)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("GenericCommands.Duplicate"))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10.f, 3.f, 10.f, 0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.MaxWidth(75.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PageName", "Page Name"))
					.MinDesiredWidth(75.f)
				]
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(5.f, 0.f, 0.f, 0.f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("PageNameHint", "Page Name"))
					.OnTextChanged(this, &SAvaRundownPageDetails::OnPageNameChanged)
					.Text(this, &SAvaRundownPageDetails::GetPageDescription)
					.IsEnabled(this, &SAvaRundownPageDetails::HasSelectedPage)
				]
			]
			// Controllers
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
			[
				SAssignNew(RCControllerPanel, SAvaRundownRCControllerPanel, InRundownEditor)
			]
			// Exposed Properties
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(5.f, 0.f, 0.f, 0.f)
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(0)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.OnClicked(this, &SAvaRundownPageDetails::ToggleExposedPropertiesVisibility)
					.ToolTipText(LOCTEXT("VisibilityButtonToolTip", "Toggle Exposed Properties Visibility"))
					.Content()
					[
						SNew(SImage)
						.Image(this, &SAvaRundownPageDetails::GetExposedPropertiesVisibilityBrush)
					]
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(5.f, 0.f, 0.f, 0.f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Properties", "Properties"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(RemoteControlProps, SAvaRundownPageRemoteControlProps, InRundownEditor)
				.Visibility(EVisibility::Collapsed)
			]
		]
	];

	OnPageSelectionChanged({});
}

SAvaRundownPageDetails::~SAvaRundownPageDetails()
{
	if (const TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin())
	{
		RundownEditor->GetOnPageEvent().RemoveAll(this);
	}
	if (IAvaMediaModule::IsModuleLoaded())
	{
		IAvaMediaModule::Get().GetManagedInstanceCache().OnEntryInvalidated.RemoveAll(this);
	}
}

void SAvaRundownPageDetails::OnPageEvent(const TArray<int32>& InSelectedPageIds, UE::AvaRundown::EPageEvent InPageEvent)
{
	if (InPageEvent == UE::AvaRundown::EPageEvent::SelectionChanged || InPageEvent == UE::AvaRundown::EPageEvent::ReimportRequest)
	{
		OnPageSelectionChanged(InSelectedPageIds);
		RemoteControlProps->Refresh(InSelectedPageIds);
		RCControllerPanel->Refresh(InSelectedPageIds);
	}
}

void SAvaRundownPageDetails::OnPageSelectionChanged(const TArray<int32>& InSelectedPageIds)
{
	ActivePageId = InSelectedPageIds.IsEmpty() ? FAvaRundownPage::InvalidPageId : InSelectedPageIds[0];
}

void SAvaRundownPageDetails::OnManagedInstanceCacheEntryInvalidated(const FSoftObjectPath& InAssetPath)
{
	if (!bRefreshSelectedPageQueued)
	{
		if (TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin())
		{
			UAvaRundown* Rundown = RundownEditor->GetRundown();

			if (IsValid(Rundown))
			{
				const FAvaRundownPage& SelectedPage = GetSelectedPage();

				if (SelectedPage.IsValidPage())
				{
					if (SelectedPage.GetAssetPath(Rundown) == InAssetPath)
					{
						bRefreshSelectedPageQueued = true;
						// Queue a refresh on next tick.
						// We don't want to refresh immediately to avoid issues with
						// cascading events within the managed instance cache.
						TWeakPtr<SWidget> ThisWeak(AsShared());
						AsyncTask(ENamedThreads::GameThread, [ThisWeak]()
							{
								if (const TSharedPtr<SWidget> ThisWidget = ThisWeak.Pin())
								{
									SAvaRundownPageDetails* AvaPageDetails = static_cast<SAvaRundownPageDetails*>(ThisWidget.Get());
									AvaPageDetails->RefreshSelectedPage();
									AvaPageDetails->bRefreshSelectedPageQueued = false;
								}
							});
					}
				}
			}
		}
	}
}

FReply SAvaRundownPageDetails::ToggleExposedPropertiesVisibility()
{
	if (RemoteControlProps->GetVisibility() == EVisibility::Collapsed)
	{
		RemoteControlProps->SetVisibility(EVisibility::SelfHitTestInvisible);
	}
	else
	{
		RemoteControlProps->SetVisibility(EVisibility::Collapsed);
	}

	return FReply::Handled();
}

const FSlateBrush* SAvaRundownPageDetails::GetExposedPropertiesVisibilityBrush() const
{
	if (RemoteControlProps->GetVisibility() == EVisibility::Collapsed)
	{
		return FAppStyle::GetBrush("Level.NotVisibleHighlightIcon16x");
	}
	else
	{
		return FAppStyle::GetBrush("Level.VisibleHighlightIcon16x");
	}
}

const FAvaRundownPage& SAvaRundownPageDetails::GetSelectedPage() const
{
	const TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin();

	if (HasSelectedPage() && RundownEditor && RundownEditor->IsRundownValid())
	{
		return RundownEditor->GetRundown()->GetPage(ActivePageId);
	}

	return FAvaRundownPage::NullPage;
}

FAvaRundownPage& SAvaRundownPageDetails::GetMutableSelectedPage() const
{
	const TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin();

	if (HasSelectedPage() && RundownEditor && RundownEditor->IsRundownValid())
	{
		return RundownEditor->GetRundown()->GetPage(ActivePageId);
	}

	return FAvaRundownPage::NullPage;
}

void SAvaRundownPageDetails::RefreshSelectedPage()
{
	const FAvaRundownPage& SelectedPage = GetSelectedPage();

	if (SelectedPage.IsValidPage())
	{
		OnPageSelectionChanged({SelectedPage.GetPageId()});
		RemoteControlProps->UpdateDefaultValuesAndRefresh({SelectedPage.GetPageId()});
		RCControllerPanel->Refresh({SelectedPage.GetPageId()});
	}
}

bool SAvaRundownPageDetails::HasSelectedPage() const
{
	if (ActivePageId == FAvaRundownPage::InvalidPageId)
	{
		return false;
	}

	return RundownEditorWeak.IsValid();
}

FText SAvaRundownPageDetails::GetPageId() const
{
	const FAvaRundownPage& SelectedPage = GetSelectedPage();

	if (SelectedPage.IsValidPage())
	{
		return FText::AsNumber(SelectedPage.GetPageId(), &UE::AvaRundown::FEditorMetrics::PageIdFormattingOptions);
	}

	return FText::GetEmpty();
}

void SAvaRundownPageDetails::OnPageIdCommitted(const FText& InNewText, ETextCommit::Type InCommitType)
{
	switch (InCommitType)
	{
		case ETextCommit::OnEnter:
		case ETextCommit::OnUserMovedFocus:
			break;

		case ETextCommit::Default:
		case ETextCommit::OnCleared:
		default:
			return;
	}

	if (!InNewText.IsNumeric())
	{
		return;
	}

	FAvaRundownPage& SelectedPage = GetMutableSelectedPage();

	if (!SelectedPage.IsValidPage()) // Not FAvaRundownPage::NullPage
	{
		return;
	}

	int32 NewId = FCString::Atoi(*InNewText.ToString());

	if (NewId == SelectedPage.GetPageId())
	{
		return;
	}

	const TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin();

	if (!RundownEditor.IsValid() || !IsValid(RundownEditor->GetRundown()))
	{
		return;
	}

	const bool bSuccessful = RundownEditor->GetRundown()->RenumberPageId(SelectedPage.GetPageId(), NewId);

	if (bSuccessful)
	{
		TSharedPtr<SAvaRundownInstancedPageList> PageList = RundownEditor->GetActiveListWidget();
		
		if (PageList.IsValid())
		{
			PageList->SelectPage(NewId);
		}
	}
}

FText SAvaRundownPageDetails::GetPageDescription() const
{
	const FAvaRundownPage& SelectedPage = GetSelectedPage();

	if (SelectedPage.IsValidPage())
	{
		return SelectedPage.GetPageDescription();
	}

	return FText::GetEmpty();
}

void SAvaRundownPageDetails::OnPageNameChanged(const FText& InNewText)
{
	FAvaRundownPage& SelectedPage = GetMutableSelectedPage();

	if (SelectedPage.IsValidPage()) // Not FAvaRundownPage::NullPage
	{
		SelectedPage.SetPageFriendlyName(InNewText);
	}
}

FReply SAvaRundownPageDetails::DuplicateSelectedPage()
{
	FAvaRundownPage& SelectedPage = GetMutableSelectedPage();

	if (!SelectedPage.IsValidPage()) // Not FAvaRundownPage::NullPage
	{
		return FReply::Unhandled();
	}

	const TSharedPtr<FAvaRundownEditor> RundownEditor = RundownEditorWeak.Pin();

	if (!RundownEditor.IsValid())
	{
		return FReply::Unhandled();
	}

	TSharedPtr<SAvaRundownInstancedPageList> PageList = RundownEditor->GetActiveListWidget();

	if (!PageList.IsValid())
	{
		return FReply::Unhandled();
	}

	TArray<int32> SelectedPages = TArray<int32>(PageList->GetSelectedPageIds());
	PageList->SelectPage(SelectedPage.GetPageId());
	PageList->DuplicateSelectedPages();
	PageList->SelectPages(SelectedPages);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
