// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaPlaylistEditorTracks.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "MediaPlaylist.h"
#include "MediaSource.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SMediaPlaylistEditorTracks"

void SMediaPlaylistEditorTracks::Construct(const FArguments& InArgs, UMediaPlaylist* InMediaPlaylist, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlaylistPtr = InMediaPlaylist;

	ChildSlot
		[
			SNew(SScrollBox)

			// Buttons to manipulate playlist.
			+ SScrollBox::Slot()
				.Padding(2)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						.HAlign(HAlign_Center)
						[
							// Add button to add to the playlist.
							SNew(SButton)
							.ToolTipText(LOCTEXT("Add_ToolTip", "Add an entry to the playlist."))
								.VAlign(VAlign_Center)
								.OnClicked_Lambda([this]() -> FReply
								{
									AddToPlaylist();
									return FReply::Handled();
								})
								[
									SNew(SImage)
										.ColorAndOpacity(FSlateColor::UseForeground())
										.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
								]
						]
				]

			// Container for our media sources.
			+ SScrollBox::Slot()
				[
					SAssignNew(SourcesContainer, SVerticalBox)
				]
		];

	RefreshPlaylist();
}

void SMediaPlaylistEditorTracks::RefreshPlaylist()
{
	SourcesContainer->ClearChildren();

	UMediaPlaylist* MediaPlaylist = MediaPlaylistPtr.Get();
	if (MediaPlaylist != nullptr)
	{
		// Add each item in the playlist.
		for (int32 Index = 0; Index < MediaPlaylist->Num(); ++Index)
		{
			UMediaSource* MediaSource = MediaPlaylist->Get(Index);

			SourcesContainer->AddSlot()
				.AutoHeight()
				.Padding(2)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						.HAlign(HAlign_Left)
						[
							// Add asset picker.
							SNew(SObjectPropertyEntryBox)
								.AllowedClass(UMediaSource::StaticClass())
								.ObjectPath(this, &SMediaPlaylistEditorTracks::GetMediaSourcePath, Index)
								.OnObjectChanged(this, &SMediaPlaylistEditorTracks::OnMediaSourceChanged, Index)
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						.HAlign(HAlign_Left)
						[
							PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
								FExecuteAction::CreateLambda([this, Index]()
								{
									UMediaPlaylist* MediaPlaylist = MediaPlaylistPtr.Get();
									if (MediaPlaylist != nullptr)
									{
										MediaPlaylist->Insert(nullptr, Index);
										MediaPlaylist->MarkPackageDirty();
										RefreshPlaylist();
									}
								}),
								FExecuteAction::CreateLambda([this, Index]()
								{
									UMediaPlaylist* MediaPlaylist = MediaPlaylistPtr.Get();
									if (MediaPlaylist != nullptr)
									{
										MediaPlaylist->RemoveAt(Index);
										MediaPlaylist->MarkPackageDirty();
										RefreshPlaylist();
									}
								}),
								FExecuteAction::CreateLambda([this, Index]()
								{
									UMediaPlaylist* MediaPlaylist = MediaPlaylistPtr.Get();
									if (MediaPlaylist != nullptr)
									{
										MediaPlaylist->Insert(MediaPlaylist->Get(Index), Index);
										MediaPlaylist->MarkPackageDirty();
										RefreshPlaylist();
									}
								}))
						]
				];
		}
	}
}

void SMediaPlaylistEditorTracks::AddToPlaylist()
{
	UMediaPlaylist* MediaPlaylist = MediaPlaylistPtr.Get();
	if (MediaPlaylist != nullptr)
	{
		MediaPlaylist->Insert(nullptr, MediaPlaylist->Num());
		MediaPlaylist->MarkPackageDirty();
		RefreshPlaylist();
	}
}

FString SMediaPlaylistEditorTracks::GetMediaSourcePath(int32 Index) const
{
	FString Path;

	UMediaPlaylist* MediaPlaylist = MediaPlaylistPtr.Get();
	if (MediaPlaylist != nullptr)
	{
		UMediaSource* MediaSource = MediaPlaylist->Get(Index);
		if (MediaSource != nullptr)
		{
			Path = MediaSource->GetPathName();
		}
	}

	return Path;
}

void SMediaPlaylistEditorTracks::OnMediaSourceChanged(const FAssetData& AssetData, int32 Index)
{
	UMediaPlaylist* MediaPlaylist = MediaPlaylistPtr.Get();
	if (MediaPlaylist != nullptr)
	{
		UMediaSource* MediaSource = Cast<UMediaSource>(AssetData.GetAsset());
		
		MediaPlaylist->Replace(Index, MediaSource);
		MediaPlaylist->MarkPackageDirty();
	}
}



#undef LOCTEXT_NAMESPACE
