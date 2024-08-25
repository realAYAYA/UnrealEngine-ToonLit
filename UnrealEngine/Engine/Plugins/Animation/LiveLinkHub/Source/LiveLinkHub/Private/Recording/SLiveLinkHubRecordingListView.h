// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "ContentBrowserModule.h"
#include "Delegates/DelegateCombinations.h"
#include "LiveLinkHub.h"
#include "Recording/LiveLinkRecording.h"
#include "Recording/LiveLinkHubPlaybackController.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"


#define LOCTEXT_NAMESPACE "LiveLinkHub.RecordingListView"

class SLiveLinkHubRecordingListView : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnImportRecording, const struct FAssetData&);
	DECLARE_DELEGATE(FOnEject);
	DECLARE_DELEGATE_RetVal(bool, FCanEject)
	
	SLATE_BEGIN_ARGS(SLiveLinkHubRecordingListView)
		{}
		SLATE_EVENT(FOnImportRecording, OnImportRecording)
		SLATE_EVENT(FOnEject, OnEject)
		SLATE_EVENT(FCanEject, CanEject)
	SLATE_END_ARGS()

	//~ Begin SWidget interface
	void Construct(const FArguments& InArgs)
	{
		OnImportRecordingDelegate = InArgs._OnImportRecording;
		OnEjectDelegate = InArgs._OnEject;
		OnCanEjectDelegate = InArgs._CanEject;

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.Padding(4.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RecordingTitle", "Recordings"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(4.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("EjectButton", "Exit Playback"))
					.OnClicked(this, &SLiveLinkHubRecordingListView::OnEjectClicked)
					.IsEnabled(this, &SLiveLinkHubRecordingListView::CanEjectRecording)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				CreateRecordingPicker()
			]
		];
	}
	//~ End SWidget interface

private:
	/** Callback to notice the hub that we've selected a recording to play. */
	void OnImportRecording(const FAssetData& AssetData) const
	{
		OnImportRecordingDelegate.Execute(AssetData);
	}

	FReply OnEjectClicked()
	{
		OnEjectDelegate.ExecuteIfBound();
		return FReply::Handled();
	}

	bool CanEjectRecording() const
	{
		check(OnCanEjectDelegate.IsBound());
		return OnCanEjectDelegate.Execute();
	}

	/** Creates the asset picker widget for selecting a recording. */
	TSharedRef<SWidget> CreateRecordingPicker()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.SelectionMode = ESelectionMode::Single;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
			AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.bShowBottomToolbar = true;
			AssetPickerConfig.bAutohideSearchBar = false;
			AssetPickerConfig.bAllowDragging = false;
			AssetPickerConfig.bCanShowClasses = false;
			AssetPickerConfig.bShowPathInColumnView = true;
			AssetPickerConfig.bSortByPathInColumnView = false;
			AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoRecordings_Warning", "No Recordings Found");

			AssetPickerConfig.bForceShowEngineContent = true;
			AssetPickerConfig.bForceShowPluginContent = true;

			AssetPickerConfig.Filter.ClassPaths.Add(ULiveLinkRecording::StaticClass()->GetClassPathName());
			AssetPickerConfig.Filter.bRecursiveClasses = true;
			AssetPickerConfig.Filter.bRecursivePaths = true;
			AssetPickerConfig.OnAssetDoubleClicked = FOnAssetSelected::CreateRaw(this, &SLiveLinkHubRecordingListView::OnImportRecording);
		}

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("ImportRecording_MenuSection", "Import Recording"));
		{
			TSharedRef<SWidget> PresetPicker = SNew(SBox)
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				ContentBrowser.CreateAssetPicker(AssetPickerConfig)
			];

			MenuBuilder.AddWidget(PresetPicker, FText(), true, false);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

private:
	/** Delegate used for noticing the hub that a recording was selected for playback. */
	FOnImportRecording OnImportRecordingDelegate;
	/** Delegate used when ejecting a recording. */
	FOnEject OnEjectDelegate;
	/** Delegate used to determine a recording can be ejected. */
	FCanEject OnCanEjectDelegate;
};

#undef LOCTEXT_NAMESPACE /* LiveLinkHub.RecordingListView */
