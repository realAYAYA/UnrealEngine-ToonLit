// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimTimeline/AnimTimelineTrack_Curve.h"

struct FFloatCurve;
class SBorder;
class FCurveEditor;

class FAnimTimelineTrack_FloatCurve : public FAnimTimelineTrack_Curve
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_FloatCurve, FAnimTimelineTrack_Curve);

public:
	FAnimTimelineTrack_FloatCurve(const FFloatCurve* InCurve, const TSharedRef<FAnimModel>& InModel);

	/** FAnimTimelineTrack_Curve interface */
	virtual TSharedRef<SWidget> MakeTimelineWidgetContainer() override;
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SAnimOutlinerItem>& InRow) override;
	virtual TSharedRef<SWidget> BuildCurveTrackMenu() override;
	virtual FText GetLabel() const override;
	virtual bool CanEditCurve(int32 InCurveIndex) const override;
	virtual bool CanRename() const override { return true; }
	virtual void RequestRename() override;
	virtual void AddCurveTrackButton(TSharedPtr<SHorizontalBox> InnerHorizontalBox) override;
	virtual FLinearColor GetCurveColor(int32 InCurveIndex) const override;
	virtual void GetCurveEditInfo(int32 InCurveIndex, FSmartName& OutName, ERawCurveTrackTypes& OutType, int32& OutCurveIndex) const override;
	virtual bool SupportsCopy() const override { return true; }
	virtual void Copy(UAnimTimelineClipboardContent* InOutClipboard) const override;
	
	/** Access the curve we are editing */
	const FFloatCurve* GetFloatCurve() { return FloatCurve; }

	/** Helper function used to get a smart name for a curve */
	static FText GetFloatCurveName(const TSharedRef<FAnimModel>& InModel, const FSmartName& InSmartName);

	/** Get this curves name */
	FSmartName GetName() const { return CurveName; }

private:
	void ConvertCurveToMetaData();
	void ConvertMetaDataToCurve();
	void RemoveCurve();
	void OnCommitCurveName(const FText& InText, ETextCommit::Type CommitInfo);

private:
	/** The curve we are editing */
	const FFloatCurve* FloatCurve;

	/** The curve name and identifier */
	FSmartName CurveName;
	FAnimationCurveIdentifier CurveId;

	/** Label we can edit */
	TSharedPtr<SInlineEditableTextBlock> EditableTextLabel;
};
