// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaPlateEditorPlaylist.h"
#include "MediaPlateComponent.h"
#include "MediaPlaylist.h"

#define LOCTEXT_NAMESPACE "SMediaPlateEditorPlaylist"

void SMediaPlateEditorPlaylist::Construct(const FArguments& InArgs, UMediaPlateComponent& InMediaPlate, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlate = &InMediaPlate;

	SMediaPlaylistEditorTracks::Construct(SMediaPlaylistEditorTracks::FArguments(), MediaPlate->MediaPlaylist, InStyle);
}

void SMediaPlateEditorPlaylist::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Call parent.
	SMediaPlaylistEditorTracks::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (MediaPlate != nullptr)
	{
		// Did the playlist change?
		UMediaPlaylist* MediaPlaylist = MediaPlaylistPtr.Get();
		if (MediaPlaylist != MediaPlate->MediaPlaylist)
		{
			// Refresh.
			MediaPlaylistPtr = MediaPlate->MediaPlaylist;
			RefreshPlaylist();
		}
	}
}

#undef LOCTEXT_NAMESPACE
