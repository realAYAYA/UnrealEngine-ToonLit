// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsavedAssetsTrackerModule.h"

#include "CoreGlobals.h"
#include "Misc/App.h"
#include "UnsavedAssetsTracker.h"
#include "SUnsavedAssetsStatusBarWidget.h"
#include "Widgets/SNullWidget.h"


void FUnsavedAssetsTrackerModule::StartupModule()
{
	UnsavedAssetTracker = MakeShared<FUnsavedAssetsTracker>();
}

void FUnsavedAssetsTrackerModule::ShutdownModule()
{
	UnsavedAssetTracker.Reset();
}

int32 FUnsavedAssetsTrackerModule::GetUnsavedAssetNum() const
{
	if (UnsavedAssetTracker)
	{
		return UnsavedAssetTracker->GetUnsavedAssetNum();
	}
	return 0;
}

TArray<FString> FUnsavedAssetsTrackerModule::GetUnsavedAssets() const
{
	if (UnsavedAssetTracker)
	{
		return UnsavedAssetTracker->GetUnsavedAssets();
	}
	return TArray<FString>();
}

TSharedRef<SWidget> FUnsavedAssetsTrackerModule::MakeUnsavedAssetsStatusBarWidget()
{
	if (!UnsavedAssetTracker)
	{
		return SNullWidget::NullWidget;
	}

	TWeakPtr<FUnsavedAssetsTracker> WeakUnsavedAssetTracker(UnsavedAssetTracker);
	return SNew(SUnsavedAssetsStatusBarWidget, UnsavedAssetTracker)
		.OnClicked_Lambda([WeakUnsavedAssetTracker]()
		{
			if (TSharedPtr<FUnsavedAssetsTracker> Tracker = WeakUnsavedAssetTracker.Pin())
			{
				Tracker->PrompToSavePackages();
			}
			return FReply::Handled();
		});
}

IMPLEMENT_MODULE(FUnsavedAssetsTrackerModule, UnsavedAssetsTracker)

