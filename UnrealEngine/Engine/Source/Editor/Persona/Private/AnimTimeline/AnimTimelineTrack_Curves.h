// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimTimeline/AnimTimelineTrack.h"
#include "Animation/Skeleton.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

class FAnimTimelineTrack_Curves : public FAnimTimelineTrack
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_Curves, FAnimTimelineTrack);

public:
	FAnimTimelineTrack_Curves(const TSharedRef<FAnimModel>& InModel);

	/** FAnimTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem>& InRow) override;

private:
	TSharedRef<SWidget> BuildCurvesSubMenu();
	void FillMetadataEntryMenu(FMenuBuilder& Builder);
	void FillVariableCurveMenu(FMenuBuilder& Builder);
	void AddMetadataEntry(USkeleton::AnimCurveUID Uid);
	void CreateNewMetadataEntryClicked();
	void CreateNewMetadataEntry(const FText& CommittedText, ETextCommit::Type CommitType);
	void CreateNewCurveClicked();
	void CreateTrack(const FText& ComittedText, ETextCommit::Type CommitInfo);
	void AddVariableCurve(USkeleton::AnimCurveUID CurveUid);
	void DeleteAllCurves();

	/** Handlers for showing curve points */
	void HandleShowCurvePoints();
	bool IsShowCurvePointsEnabled() const;

	/** Curve Picker Callbacks */
	void OnMetadataCurveNamePicked(const FSmartName & InCurveSmartName);
	void OnVariableCurveNamePicked(const FSmartName & InCurveSmartName);
	bool IsCurveMarkedForExclusion(const FSmartName & InCurveSmartName);
	
private:
	TSharedPtr<SWidget>	OutlinerWidget;
};
