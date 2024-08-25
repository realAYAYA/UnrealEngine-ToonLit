// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDPlaybackControllerInstigator.h"
#include "ChaosVDPlaybackControllerObserver.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FChaosVDTrackInfo;
class FChaosVDPlaybackController;

/** Widget that Generates a expandable list of solver controls, based on the existing solver data
 * on the ChaosVDPlaybackController
 */
class SChaosVDSolverTracks : public SCompoundWidget, public FChaosVDPlaybackControllerObserver, public IChaosVDPlaybackControllerInstigator
{
public:

	SLATE_BEGIN_ARGS( SChaosVDSolverTracks ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FChaosVDPlaybackController> InPlaybackController);

private:

	void UpdatedCachedTrackInfoData(TWeakPtr<FChaosVDPlaybackController> InPlaybackController, const FChaosVDTrackInfo* UpdatedTrackInfo);
	virtual void HandlePlaybackControllerDataUpdated(TWeakPtr<FChaosVDPlaybackController> InPlaybackController) override;
	virtual void HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InPlaybackController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid) override;

	TSharedRef<ITableRow> MakeSolverTrackControlsFromTrackInfo(TSharedPtr<FChaosVDTrackInfo> TrackInfo, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<SListView<TSharedPtr<FChaosVDTrackInfo>>> SolverTracksListWidget;

	TArray<TSharedPtr<FChaosVDTrackInfo>> CachedTrackInfoArray;
};
