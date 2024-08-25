// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "IContentBrowserSingleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LiveLinkHub.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkHubPlaybackController.h"
#include "LiveLinkHubRecordingListController.h"
#include "SLiveLinkHubRecordingListView.h"
#include "LiveLinkRecording.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub.RecordingList"

class FLiveLinkHubRecordingListController
{
public:
	FLiveLinkHubRecordingListController(const TSharedRef<FLiveLinkHub>& InLiveLinkHub)
		: LiveLinkHub(InLiveLinkHub)
	{
	}

	/** Create the list's widget. */
	TSharedRef<SWidget> MakeRecordingList()
	{
		return SNew(SLiveLinkHubRecordingListView)
			.OnImportRecording_Raw(this, &FLiveLinkHubRecordingListController::OnImportRecording)
			.OnEject_Raw(this, &FLiveLinkHubRecordingListController::OnEjectRecording)
			.CanEject_Raw(this, &FLiveLinkHubRecordingListController::CanEjectRecording);
	}

private:
	/** Handler called when a recording a clicked to start the recording.  */
	void OnImportRecording(const FAssetData& AssetData)
	{
		UObject* RecordingAssetData = AssetData.GetAsset();
		if (!RecordingAssetData)
		{
			UE_LOG(LogLiveLinkHub, Warning, TEXT("Failed to import recording %s"), *AssetData.AssetName.ToString());
			return;
		}

		ULiveLinkRecording* ImportedRecording = Cast<ULiveLinkRecording>(RecordingAssetData);
		
		if (TSharedPtr<FLiveLinkHub> HubPtr = LiveLinkHub.Pin())
		{
			HubPtr->GetPlaybackController()->PreparePlayback(ImportedRecording);
		}
	}

	void OnEjectRecording()
	{
		if (const TSharedPtr<FLiveLinkHub> HubPtr = LiveLinkHub.Pin())
		{
			HubPtr->GetPlaybackController()->Eject();
		}
	}

	bool CanEjectRecording() const
	{
		const TSharedPtr<FLiveLinkHub> HubPtr = LiveLinkHub.Pin();
		return HubPtr && HubPtr->GetPlaybackController()->GetRecording().IsValid();
	}

private:
	/** LiveLinkHub object that holds the different controllers. */
	TWeakPtr<FLiveLinkHub> LiveLinkHub;
};

#undef LOCTEXT_NAMESPACE /* LiveLinkHub.RecordingList */
