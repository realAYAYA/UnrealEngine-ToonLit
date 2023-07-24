// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeSplineDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "LandscapeEdMode.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "ILandscapeSplineInterface.h"

#define LOCTEXT_NAMESPACE "LandscapeSplineDetails"


TSharedRef<IDetailCustomization> FLandscapeSplineDetails::MakeInstance()
{
	return MakeShareable(new FLandscapeSplineDetails);
}

void FLandscapeSplineDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& LandscapeSplineCategory = DetailBuilder.EditCategory("LandscapeSpline", FText::GetEmpty(), ECategoryPriority::Transform);

	LandscapeSplineCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0, 0, 2, 0)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(STextBlock)
			.Text_Raw(this, &FLandscapeSplineDetails::OnGetSplineOwningLandscapeText)
		]
	];

	LandscapeSplineCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0, 0, 2, 0)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectAll", "Select all connected:"))
		]
		+ SHorizontalBox::Slot()
		.Padding(0, 2, 0, 2)
		.FillWidth(1)
		[
			SNew(SButton)
			.Text(LOCTEXT("ControlPoints", "Control Points"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &FLandscapeSplineDetails::OnSelectConnectedControlPointsButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.Padding(0, 2, 0, 2)
		.FillWidth(1)
		[
			SNew(SButton)
			.Text(LOCTEXT("Segments", "Segments"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(this, &FLandscapeSplineDetails::OnSelectConnectedSegmentsButtonClicked)
		]
	];

	LandscapeSplineCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SButton)
			.Text(LOCTEXT("Move Selected ControlPnts+Segs to Current level", "Move to current level"))
			.HAlign(HAlign_Center)
			.OnClicked(this, &FLandscapeSplineDetails::OnMoveToCurrentLevelButtonClicked)
			.IsEnabled(this, &FLandscapeSplineDetails::IsMoveToCurrentLevelButtonEnabled)
		]
	];
	LandscapeSplineCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SButton)
			.Text(LOCTEXT("Move Spline Mesh Components to Proper level", "Update Spline Mesh Levels"))
			.HAlign(HAlign_Center)
			.OnClicked(this, &FLandscapeSplineDetails::OnUpdateSplineMeshLevelsButtonClicked)
			.IsEnabled(this, &FLandscapeSplineDetails::IsUpdateSplineMeshLevelsButtonEnabled)
		]
	];

	IDetailCategoryBuilder& LandscapeSplineSegmentCategory = DetailBuilder.EditCategory("LandscapeSplineSegment", FText::GetEmpty(), ECategoryPriority::Default);
	LandscapeSplineSegmentCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0, 0, 2, 0)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SButton)
			.Text(LOCTEXT("FlipSegment", "Flip Selected Segment(s)"))
			.HAlign(HAlign_Center)
			.OnClicked(this, &FLandscapeSplineDetails::OnFlipSegmentButtonClicked)
			.IsEnabled(this, &FLandscapeSplineDetails::IsFlipSegmentButtonEnabled)
		]
	];
}

class FEdModeLandscape* FLandscapeSplineDetails::GetEditorMode() const
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FReply FLandscapeSplineDetails::OnFlipSegmentButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->FlipSelectedSplineSegments();
	}
	return FReply::Handled();
}

bool FLandscapeSplineDetails::IsFlipSegmentButtonEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	return LandscapeEdMode && LandscapeEdMode->HasSelectedSplineSegments();
}

FText FLandscapeSplineDetails::OnGetSplineOwningLandscapeText() const
{
	TSet<AActor*> SplineOwners;
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LandscapeEdMode->GetSelectedSplineOwners(SplineOwners);
	}

	FString SplineOwnersStr;
	for (AActor* Owner : SplineOwners)
	{
		if (Owner)
		{
			if (!SplineOwnersStr.IsEmpty())
			{
				SplineOwnersStr += ", ";
			}

			SplineOwnersStr += Owner->GetActorLabel();
		}
	}
	
	return FText::FromString("Owner: " + SplineOwnersStr);
}

FReply FLandscapeSplineDetails::OnSelectConnectedControlPointsButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LandscapeEdMode->SelectAllConnectedSplineControlPoints();
	}

	return FReply::Handled();
}

FReply FLandscapeSplineDetails::OnSelectConnectedSegmentsButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LandscapeEdMode->SelectAllConnectedSplineSegments();
	}

	return FReply::Handled();
}

FReply FLandscapeSplineDetails::OnMoveToCurrentLevelButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid() && LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetCurrentLevelLandscapeProxy(true))
	{
		LandscapeEdMode->SplineMoveToCurrentLevel();
	}

	return FReply::Handled();
}

bool FLandscapeSplineDetails::IsMoveToCurrentLevelButtonEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid() && LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetCurrentLevelLandscapeProxy(true))
	{
		return LandscapeEdMode->CanMoveSplineToCurrentLevel();
	}

	return false;
}

FReply FLandscapeSplineDetails::OnUpdateSplineMeshLevelsButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LandscapeEdMode->UpdateSplineMeshLevels();
	}
	
	return FReply::Handled();
}

bool FLandscapeSplineDetails::IsUpdateSplineMeshLevelsButtonEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	return (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid());
}

#undef LOCTEXT_NAMESPACE
