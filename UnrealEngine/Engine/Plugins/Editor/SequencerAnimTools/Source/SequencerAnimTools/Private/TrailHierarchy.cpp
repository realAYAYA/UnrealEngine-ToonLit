// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrailHierarchy.h"
#include "TrajectoryDrawInfo.h"
#include "Containers/Queue.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorModeManager.h"
#include "Tools/MotionTrailOptions.h"
#include "Widgets/Input/SButton.h"
#include "CanvasTypes.h"

#define LOCTEXT_NAMESPACE "MotionTrailEditorMode"

namespace UE
{
namespace SequencerAnimTools
{

IMPLEMENT_HIT_PROXY(HBaseTrailProxy, HHitProxy);
IMPLEMENT_HIT_PROXY(HMotionTrailProxy, HBaseTrailProxy);

void FTrailHierarchyRenderer::Render(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const FDateTime RenderStartTime = FDateTime::Now();
	
	const int32 NumEvalTimes = int32(OwningHierarchy->GetViewRange().Size<double>() / OwningHierarchy->GetSecondsPerSegment());
	const int32 NumLinesReserveSize = int32(NumEvalTimes * OwningHierarchy->GetAllTrails().Num() * 1.3);
	PDI->AddReserveLines(SDPG_Foreground, NumLinesReserveSize);

	TArray<FGuid> TrailsToRemove; //if trail is not visible we just get rid of it
	// <parent, current>
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : OwningHierarchy->GetAllTrails())
	{
		FTrajectoryDrawInfo* CurDrawInfo = GuidTrailPair.Value->GetDrawInfo();
		if (CurDrawInfo && OwningHierarchy->GetVisibilityManager().IsTrailVisible(GuidTrailPair.Key, GuidTrailPair.Value.Get()))
		{
			FDisplayContext DisplayContext = {
				GuidTrailPair.Key,
				FTrailScreenSpaceTransform(View),
				UMotionTrailToolOptions::GetTrailOptions()->SecondsPerMark,
				OwningHierarchy->GetViewRange(),
				OwningHierarchy->GetSecondsPerSegment()
			};

			const bool bHitTesting = PDI && PDI->IsHitTesting();
			TArray<FVector> PointsToDraw;
			TArray<double> TrajectoryTimes;
			GuidTrailPair.Value->GetTrajectoryPointsForDisplay(DisplayContext, PointsToDraw, TrajectoryTimes);
			FLinearColor Color = OwningHierarchy->GetVisibilityManager().IsTrailAlwaysVisible(GuidTrailPair.Key) ? CurDrawInfo->GetColor() : UMotionTrailToolOptions::GetTrailOptions()->TrailColor;
			Color = GuidTrailPair.Value->IsTrailSelected() ? FLinearColor(1.0f, 1.0f, 0.0f) : Color;
			if (PointsToDraw.Num() > 1)
			{
				const float TrailThickness = UMotionTrailToolOptions::GetTrailOptions()->TrailThickness;
				FVector LastPoint = PointsToDraw[0];
				for (int32 Idx = 1; Idx < PointsToDraw.Num(); Idx++)
				{
					if (bHitTesting)
					{
						const double CurSecond = TrajectoryTimes[Idx - 1];
						PDI->SetHitProxy(new HMotionTrailProxy(GuidTrailPair.Key, LastPoint, CurSecond));
					}
					const FVector CurPoint = PointsToDraw[Idx];
					PDI->DrawLine(LastPoint, CurPoint, Color, SDPG_Foreground,TrailThickness);
					LastPoint = CurPoint;
					if (bHitTesting)
					{
						PDI->SetHitProxy(nullptr);
					}
				}
			}
			GuidTrailPair.Value->Render(GuidTrailPair.Key,View,PDI);

		}
		else
		{
			TrailsToRemove.Add(GuidTrailPair.Key);
		}

	}
	for (const FGuid& Key : TrailsToRemove)
	{
		OwningHierarchy->RemoveTrail(Key);
	}
	const FTimespan RenderTimespan = FDateTime::Now() - RenderStartTime;
	OwningHierarchy->GetTimingStats().Add("FTrailHierarchyRenderer::Render", RenderTimespan);
}

void FTrailHierarchyRenderer::DrawHUD(const FSceneView* View, FCanvas* Canvas)
{

	if (UMotionTrailToolOptions::GetTrailOptions()->bShowMarks == false)
	{
		for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : OwningHierarchy->GetAllTrails())
		{
			GuidTrailPair.Value->DrawHUD( View, Canvas);
		}
		return;
	}	

	const FDateTime DrawHUDStartTime = FDateTime::Now();

	const double SecondsPerMark = UMotionTrailToolOptions::GetTrailOptions()->bLockMarksToFrames ? OwningHierarchy->GetSecondsPerFrame() : UMotionTrailToolOptions::GetTrailOptions()->SecondsPerMark;
	const int32 PredictedNumMarks = int32((OwningHierarchy->GetViewRange().Size<double>() / SecondsPerMark) * OwningHierarchy->GetAllTrails().Num() * 1.3); // Multiply by 1.3 to be safe
	Canvas->GetBatchedElements(FCanvas::EElementType::ET_Line)->AddReserveLines(PredictedNumMarks);

	for(const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : OwningHierarchy->GetAllTrails())
	{
		FTrajectoryDrawInfo* CurDrawInfo = GuidTrailPair.Value->GetDrawInfo();
		if (CurDrawInfo && OwningHierarchy->GetVisibilityManager().IsTrailVisible(GuidTrailPair.Key, GuidTrailPair.Value.Get()))
		{
			FDisplayContext DisplayContext = {
				GuidTrailPair.Key,
				FTrailScreenSpaceTransform(View, Canvas->GetDPIScale()),
				SecondsPerMark,
				OwningHierarchy->GetViewRange(),
				OwningHierarchy->GetSecondsPerSegment()

			};

			TArray<FVector2D> Marks, MarkNormals;
			GuidTrailPair.Value->GetTickPointsForDisplay(DisplayContext, Marks, MarkNormals);
			const FLinearColor Color = OwningHierarchy->GetVisibilityManager().IsTrailAlwaysVisible(GuidTrailPair.Key) ? CurDrawInfo->GetColor() : UMotionTrailToolOptions::GetTrailOptions()->MarkColor;
			for (int32 Idx = 0; Idx < Marks.Num(); Idx++)
			{
				const FVector2D StartPoint = Marks[Idx] - MarkNormals[Idx] * UMotionTrailToolOptions::GetTrailOptions()->MarkSize;
				const FVector2D EndPoint = Marks[Idx] + MarkNormals[Idx] * UMotionTrailToolOptions::GetTrailOptions()->MarkSize;
				FCanvasLineItem LineItem = FCanvasLineItem(StartPoint, EndPoint);
				LineItem.SetColor(Color);
				Canvas->DrawItem(LineItem);
			}
		}
		GuidTrailPair.Value->DrawHUD(View, Canvas);

	}

	const FTimespan DrawHUDTimespan = FDateTime::Now() - DrawHUDStartTime;
	OwningHierarchy->GetTimingStats().Add("FTrailHierarchyRenderer::DrawHUD", DrawHUDTimespan);
}

class STrailOptionsPopup : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STrailOptionsPopup)
	{}

	SLATE_ARGUMENT(FTrailVisibilityManager*, InVisibilityManager)
	SLATE_ARGUMENT(FGuid, InTrailGuid)
	SLATE_ARGUMENT(FTrajectoryDrawInfo*, InDrawInfo)

	SLATE_END_ARGS()


public:
	FTrailVisibilityManager* VisibilityManager;
	FGuid TrailGuid;
	FTrajectoryDrawInfo* DrawInfo;

	void SetTrailAlwaysVisible(ECheckBoxState NewState)
	{
		bool bSet = (NewState == ECheckBoxState::Checked);
		if (VisibilityManager)
		{
			VisibilityManager->SetTrailAlwaysVisible(TrailGuid,bSet);
			if (DrawInfo && bSet == true)
			{
				DrawInfo->SetColor(UMotionTrailToolOptions::GetTrailOptions()->TrailColor);
			}
		}
	}

	ECheckBoxState IsTrailAlwaysVisible() const
	{
		if (VisibilityManager)
		{
			return VisibilityManager->IsTrailAlwaysVisible(TrailGuid) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	FReply ShowOptions()
	{
		const FName ControlRigMotionTrailTab("ControlRigMotionTrailTab");
		FGlobalTabmanager::Get()->TryInvokeTab(ControlRigMotionTrailTab);
		return FReply::Handled();
	}

	void Construct(const FArguments& InArgs)
	{
		VisibilityManager = InArgs._InVisibilityManager;
		TrailGuid = InArgs._InTrailGuid;
		DrawInfo = InArgs._InDrawInfo;
		// Then make widget
		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Menu.Background")))
			.Padding(5)
			.Content()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &STrailOptionsPopup::SetTrailAlwaysVisible)
						.IsChecked(this, &STrailOptionsPopup::IsTrailAlwaysVisible)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(LOCTEXT("AlwaysVisible", "Always Visible"))
					]	
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 1.0f)
				[
					SNew(SButton)
					.ContentPadding(FMargin(8, 2))
					.Text(LOCTEXT("Show Options", "Show Options"))
					.OnClicked(this, &STrailOptionsPopup::ShowOptions)
				]
			]
		];
		
	}
};

void FTrailHierarchy::OpenContextMenu(const FGuid& TrailGuid)
{

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr< ILevelEditor > LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	TSharedPtr<SWidget> MenuWidget;
	if(const TUniquePtr<FTrail>* Trail = GetAllTrails().Find(TrailGuid))
	{
		FTrajectoryDrawInfo* CurDrawInfo = (*Trail)->GetDrawInfo();

		MenuWidget =
			SNew(STrailOptionsPopup)
			.InVisibilityManager(&GetVisibilityManager())
			.InTrailGuid(TrailGuid)
			.InDrawInfo(CurDrawInfo);

		// Create as context menu
		FSlateApplication::Get().PushMenu(
			LevelEditor.ToSharedRef(),
			FWidgetPath(),
			MenuWidget.ToSharedRef(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);
	}
	
}

bool FTrailHierarchy::IsHitByClick(HHitProxy* InHitProxy)
{
	if (InHitProxy)
	{
		if (HBaseTrailProxy* HitProxy = HitProxyCast<HBaseTrailProxy>(InHitProxy))
		{
			return true;
		}
	}
	return false;
}

bool FTrailHierarchy::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* InHitProxy,FInputClick Click)
{
	bool bClickIsHandled = false;
	
	if (HMotionTrailProxy* HitProxy = HitProxyCast<HMotionTrailProxy>(InHitProxy))
	{
		if (Click.bIsRightMouse)
		{
			OpenContextMenu(HitProxy->Guid);
			return true;
		}
	}
	

	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair :GetAllTrails())
	{
		bool bIsClickHandled = GuidTrailPair.Value->HandleClick(GuidTrailPair.Key, InViewportClient, InHitProxy, Click);
		if (bIsClickHandled)
		{
			bClickIsHandled = true;
		}
	}

	return bClickIsHandled;
}

bool FTrailHierarchy::IsAnythingSelected() const
{
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		FVector Location;
		bool bHandled = GuidTrailPair.Value->IsAnythingSelected();
		if (bHandled)
		{
			return true;
		}
	}
	return false;
}

bool FTrailHierarchy::IsAnythingSelected(FVector& OutVectorPosition) const
{
	OutVectorPosition = FVector::ZeroVector;
	int NumSelected = 0;
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		FVector Location;
		bool bHandled = GuidTrailPair.Value->IsAnythingSelected(Location);
		if (bHandled)
		{
			OutVectorPosition += Location;
			++NumSelected;
		}
	}

	if (NumSelected > 0)
	{
		OutVectorPosition /= (double)NumSelected;
	}
	return (NumSelected > 0);
}

bool FTrailHierarchy::IsAnythingSelected(TArray<FVector>& OutVectorPositions) const
{
	OutVectorPositions.SetNum(0);
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		FVector Location;
		bool bHandled = GuidTrailPair.Value->IsAnythingSelected(Location);
		if (bHandled)
		{
			OutVectorPositions.Add(Location);
		}
	}
	return (OutVectorPositions.Num() > 0);
}


bool FTrailHierarchy::IsSelected(const FGuid& Key) const
{
	const TUniquePtr<FTrail>* Trail = GetAllTrails().Find(Key);
	if (Trail != nullptr)
	{
		return (*Trail)->IsAnythingSelected();
	}
	return false;
}
void FTrailHierarchy::SelectNone()
{
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		GuidTrailPair.Value->SelectNone();
	}
}
void FTrailHierarchy::TranslateSelectedKeys(bool bRight)
{
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{		
		GuidTrailPair.Value->TranslateSelectedKeys(bRight);
	}
}

void FTrailHierarchy::DeleteSelectedKeys()
{
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		GuidTrailPair.Value->DeleteSelectedKeys();
	}
}

bool FTrailHierarchy::IsAlwaysVisible(const FGuid Key) const
{
	return VisibilityManager.IsTrailAlwaysVisible(Key);
}

bool FTrailHierarchy::BoxSelect(FBox& InBox, bool InSelect)
{
	bool bIsHandled = false;
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		bool bHandled = GuidTrailPair.Value->BoxSelect(InBox,InSelect);
		if (bHandled)
		{
			bIsHandled = true;
		}
	}
	return bIsHandled;
}
bool FTrailHierarchy::FrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, bool InSelect)
{
	bool bIsHandled = false;
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		bool bHandled = GuidTrailPair.Value->FrustumSelect(InFrustum,InViewportClient,InSelect);
		if (bHandled)
		{
			bIsHandled = true;
		}
	}
	return bIsHandled;
}


bool FTrailHierarchy::StartTracking()
{
	bool bIsHandled = false;
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		bool bHandled = GuidTrailPair.Value->StartTracking();
		if (bHandled)
		{
			bIsHandled = true;
		}
	}
	return bIsHandled;
}

bool FTrailHierarchy::ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector &WidgetLocation)

{
	bool bIsHandled = false;
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		bool bHandled =  GuidTrailPair.Value->ApplyDelta(Pos,Rot,WidgetLocation);
		if (bHandled)
		{
			bIsHandled = true;
		}
	}
	return bIsHandled;
}

bool FTrailHierarchy::EndTracking()
{
	bool bIsHandled = false;
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		bool bHandled = GuidTrailPair.Value->EndTracking();
		if (bHandled)
		{
			bIsHandled = true;
		}
	}
	return bIsHandled;
}

void FTrailHierarchy::CalculateEvalRangeArray()
{
	// Generate times to evaluate
	SecondsPerSegment = GetSecondsPerSegment();

	EvalTimesArr.Reserve(int32(EvalRange.Size<double>() / SecondsPerSegment) + 1);
	EvalTimesArr.SetNum(0);
	for (double SecondsItr = EvalRange.GetLowerBoundValue(); SecondsItr <= EvalRange.GetUpperBoundValue() + SecondsPerSegment; SecondsItr += SecondsPerSegment)
	{
		EvalTimesArr.Add(SecondsItr);
	}

	if (LastSecondsPerSegment != SecondsPerSegment || EvalRange != LastEvalRange)
	{
		for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
		{
			GuidTrailPair.Value->ForceEvaluateNextTick();
		}
		LastSecondsPerSegment = SecondsPerSegment;
		LastEvalRange = EvalRange;
	}
}
void FTrailHierarchy::Update()
{
	const FDateTime UpdateStartTime = FDateTime::Now();
	
	CalculateEvalRangeArray();
	FTrailEvaluateTimes EvalTimes = FTrailEvaluateTimes(EvalTimesArr, SecondsPerSegment);

	VisibilityManager.InactiveMask.Reset();
	TArray<FGuid> DeadTrails;

	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		FGuid CurGuid = GuidTrailPair.Key;

		FTrail::FSceneContext SceneContext = {
			CurGuid,
			EvalTimes,
			this
		};

		if (!AllTrails.Contains(CurGuid))
		{
			continue;
		}

		// Update the trail
		ETrailCacheState CurCacheState = AllTrails[CurGuid]->UpdateTrail(SceneContext);
		if (CurCacheState == ETrailCacheState::Dead)
		{
			DeadTrails.Add(CurGuid);
		}

		if (CurCacheState == ETrailCacheState::NotUpdated)
		{
			VisibilityManager.InactiveMask.Add(CurGuid);
		}
	}

	for (const FGuid& TrailGuid : DeadTrails)
	{
		RemoveTrail(TrailGuid);
	}

	const FTimespan UpdateTimespan = FDateTime::Now() - UpdateStartTime;
	TimingStats.Add("FTrailHierarchy::Update", UpdateTimespan);
}

void FTrailHierarchy::AddTrail(const FGuid& Key, TUniquePtr<FTrail>&& TrailPtr)
{
	AllTrails.Add(Key, MoveTemp(TrailPtr));
}

void FTrailHierarchy::RemoveTrail(const FGuid& Key)
{
	AllTrails.Remove(Key);
}

void FTrailHierarchy::RemoveTrailIfNotAlwaysVisible(const FGuid& Key)
{
	if (IsAlwaysVisible(Key) == false)
	{
		RemoveTrail(Key);
	}
}

} // namespace SequencerAnimTools
} // namespace UE

#undef LOCTEXT_NAMESPACE
