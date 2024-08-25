// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub.RecordingListView"

class SLiveLinkHubRecordingListView : public SCompounWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkHubRecordingListView)
		{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				CreateRecordingPicker()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(4.f, 0)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LoopRecordingLabel", "Loop"))
				]
				+ SHorizontalBox::Slot()
				.Padding(4.f, 0)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([]()
					{
						FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
						return LiveLinkHubModule.GetPlaybackController()->IsLooping() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([](ECheckBoxState InState)
					{
						FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
						return LiveLinkHubModule.GetPlaybackController()->SetLooping(InState == ECheckBoxState::Checked);
					})
					.Padding(FMargin(4.0f, 0.0f))
				]
			];
		]
	}

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
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FLiveLinkHubRecordingListController::OnImportRecording);
		}

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("ImportRecording_MenuSection", "Import Recording"));
		{
			TSharedRef<SWidget> Picker = SNew(SBox)
				.MinDesiredWidth(400.f)
				.MinDesiredHeight(400.f)
				[
					ContentBrowser.CreateAssetPicker(AssetPickerConfig)
				];

			MenuBuilder.AddWidget(Picker, FText(), true, false);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	void MakeRecordingList()
	{
		
	}
};

#undef LOCTEXT_NAMESPACE /*LiveLinkHub.RecordingListView*/
