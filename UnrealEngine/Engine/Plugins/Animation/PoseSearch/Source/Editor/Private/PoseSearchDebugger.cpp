// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDebugger.h"
#include "IAnimationProvider.h"
#include "IGameplayProvider.h"
#include "IRewindDebugger.h"
#include "PoseSearchDebuggerView.h"
#include "PoseSearchDebuggerViewModel.h"
#include "SSimpleTimeSlider.h"
#include "Styling/SlateIconFinder.h"
#include "Trace/PoseSearchTraceProvider.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PoseSearchDebugger"

namespace UE::PoseSearch
{

typedef SCurveTimelineView::FTimelineCurveData::CurvePoint FCurvePoint;
class SCostCurveTimelineView : public SCurveTimelineView
{
public:
	TRange<double> GetViewRange() const { return ViewRange.Get(); }
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Unhandled(); }
};

class SCostTimelineView : public SOverlay
{
public:
	SLATE_BEGIN_ARGS(SCostTimelineView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void UpdateInternal(uint64 ObjectId);

protected:
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	TSharedPtr<SCostCurveTimelineView> BestCostView;
	TSharedPtr<SCostCurveTimelineView::FTimelineCurveData> BestCostData;

	TSharedPtr<SCostCurveTimelineView> BruteForceCostView;
	TSharedPtr<SCostCurveTimelineView::FTimelineCurveData> BruteForceCostData;

	TSharedPtr<SCostCurveTimelineView> BestPosePosView;
	TSharedPtr<SCostCurveTimelineView::FTimelineCurveData> BestPosePosData;

	TSharedPtr<SToolTip> CostToolTip;

	FText ToolTipTime;
	FText ToolTipCost;
	FText ToolTipCostBruteForce;
	FText ToolTipBestPosePos;
};

void SCostTimelineView::Construct(const FArguments& InArgs)
{
	BestCostData = MakeShared<SCurveTimelineView::FTimelineCurveData>();
	BruteForceCostData = MakeShared<SCurveTimelineView::FTimelineCurveData>();
	BestPosePosData = MakeShared<SCurveTimelineView::FTimelineCurveData>();
	
	BestCostView = SNew(SCostCurveTimelineView)
	.CurveColor(FLinearColor::White)
	.ViewRange_Lambda([]()
	{
		return IRewindDebugger::Instance()->GetCurrentViewRange();
	})
	.RenderFill(false)
	.CurveData_Lambda([this]()
	{
		return BestCostData;
	});

	BruteForceCostView = SNew(SCostCurveTimelineView)
	.CurveColor(FLinearColor::Red)
	.ViewRange_Lambda([]()
	{
		return IRewindDebugger::Instance()->GetCurrentViewRange();
	})
	.RenderFill(false)
	.CurveData_Lambda([this]()
	{
		return BruteForceCostData;
	});

	BestPosePosView = SNew(SCostCurveTimelineView)
	.CurveColor(FLinearColor::Blue)
	.ViewRange_Lambda([]()
	{
		return IRewindDebugger::Instance()->GetCurrentViewRange();
	})
	.RenderFill(false)
	.CurveData_Lambda([this]()
	{
		return BestPosePosData;
	});
		
	AddSlot()
	[
		BruteForceCostView.ToSharedRef()
	];
	AddSlot()
	[
		BestCostView.ToSharedRef()
	];
	AddSlot()
	[
		BestPosePosView.ToSharedRef()
	];
}

void SCostTimelineView::UpdateInternal(uint64 ObjectId)
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();

	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	check(AnalysisSession);
	if (const FTraceProvider* PoseSearchProvider = AnalysisSession->ReadProvider<FTraceProvider>(FTraceProvider::ProviderName))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		TArray<FCurvePoint>& BestCostPoints = BestCostData->Points;
		BestCostPoints.Reset();

		TArray<FCurvePoint>& BruteForceCostPoints = BruteForceCostData->Points;
		BruteForceCostPoints.Reset();

		TArray<FCurvePoint>& BestPosePosPoints = BestPosePosData->Points;
		BestPosePosPoints.Reset();

		// convert time range to from rewind debugger times to profiler times
		TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
		double StartTime = TraceTimeRange.GetLowerBoundValue();
		double EndTime = TraceTimeRange.GetUpperBoundValue();

		PoseSearchProvider->EnumerateMotionMatchingStateTimelines(ObjectId, [StartTime, EndTime, &BestCostPoints, &BruteForceCostPoints, &BestPosePosPoints](const FTraceProvider::FMotionMatchingStateTimeline& InTimeline)
		{
			// this isn't very efficient, and it gets called every frame.  will need optimizing
			InTimeline.EnumerateEvents(StartTime, EndTime, [StartTime, EndTime, &BestCostPoints, &BruteForceCostPoints, &BestPosePosPoints](double InStartTime, double InEndTime, uint32 InDepth, const FTraceMotionMatchingStateMessage& InMessage)
			{
				if (InEndTime > StartTime && InStartTime < EndTime)
				{
					BestCostPoints.Add({ InMessage.RecordingTime, InMessage.SearchBestCost });
					BruteForceCostPoints.Add({ InMessage.RecordingTime, InMessage.SearchBruteForceCost });
					BestPosePosPoints.Add({ InMessage.RecordingTime, float(InMessage.SearchBestPosePos) });
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		float MinValue = UE_MAX_FLT;
		float MaxValue = -UE_MAX_FLT;
		for (const FCurvePoint& CurvePoint : BestCostPoints)
		{
			MinValue = FMath::Min(MinValue, CurvePoint.Value);
			MaxValue = FMath::Max(MaxValue, CurvePoint.Value);
		}
		for (const FCurvePoint& CurvePoint : BruteForceCostPoints)
		{
			MinValue = FMath::Min(MinValue, CurvePoint.Value);
			MaxValue = FMath::Max(MaxValue, CurvePoint.Value);
		}

		BestCostView->SetFixedRange(MinValue, MaxValue);
		BruteForceCostView->SetFixedRange(MinValue, MaxValue);
	}
}

FReply SCostTimelineView::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		// Mouse position in widget space
		const FVector2D HitPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

		// Range helper struct
		const SSimpleTimeSlider::FScrubRangeToScreen RangeToScreen(BestCostView->GetViewRange(), MyGeometry.GetLocalSize());

		// Mouse position from widget space to curve input space
		const double TargetTime = RangeToScreen.LocalXToInput(HitPosition.X);

		// Get curve value at given time
		const TArray<FCurvePoint>& CurvePoints = BestCostData->Points;
		const int32 NumPoints = CurvePoints.Num();

		if (NumPoints > 0)
		{
			for (int32 i = 1; i < NumPoints; ++i)
			{
				const FCurvePoint& Point1 = CurvePoints[i - 1];
				const FCurvePoint& Point2 = CurvePoints[i];

				// Find points that contain mouse hit-point time
				if (Point1.Time >= TargetTime && TargetTime <= Point2.Time)
				{
					// Choose point with the smallest delta
					const float Delta1 = abs(TargetTime - Point1.Time);
					const float Delta2 = abs(TargetTime - Point2.Time);

					// Get closest point index
					const int32 TargetPointIndex = Delta1 < Delta2 ? i - 1 : i;

					const float Time = CurvePoints[TargetPointIndex].Time;
					const float BestCost = CurvePoints[TargetPointIndex].Value;
					const float BruteForceCost = BruteForceCostData->Points[TargetPointIndex].Value;
					const int32 BestPosePos = FMath::RoundToInt(BestPosePosData->Points[TargetPointIndex].Value);

					// Tooltip text formatting
					FNumberFormattingOptions FormattingOptions;
					FormattingOptions.MaximumFractionalDigits = 3;

					ToolTipBestPosePos = FText::Format(LOCTEXT("CostTimelineViewToolTip_BestPosePosFormat", "Best Index: {0}"), FText::AsNumber(BestPosePos, &FormattingOptions));
					ToolTipTime = FText::Format(LOCTEXT("CostTimelineViewToolTip_TimeFormat", "Search Time: {0}"), FText::AsNumber(Time, &FormattingOptions));
					ToolTipCost = FText::Format(LOCTEXT("CostTimelineViewToolTip_CostFormat", "Search Cost: {0}"), FText::AsNumber(BestCost, &FormattingOptions));

					if (FMath::IsNearlyEqual(BestCost, BruteForceCost))
					{
						ToolTipCostBruteForce = FText::GetEmpty();
					}
					else
					{
						ToolTipCostBruteForce = FText::Format(LOCTEXT("CostTimelineViewToolTip_CostBruteForceFormat", "Search BruteForce Cost: {0}"), FText::AsNumber(BruteForceCost, &FormattingOptions));
					}

					// Update tooltip info
					if (!CostToolTip.IsValid())
					{
						SetToolTip(
							SAssignNew(CostToolTip, SToolTip)
							.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.BrightBackground"))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								[
									SNew(STextBlock)
									.Text_Lambda([this]() { return ToolTipTime; })
									.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
									.ColorAndOpacity(FLinearColor::Black)
								]
								+ SVerticalBox::Slot()
								[
									SNew(STextBlock)
									.Text_Lambda([this]() { return ToolTipBestPosePos; })
									.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
									.ColorAndOpacity(FLinearColor::Black)
								]
								+ SVerticalBox::Slot()
								[
									SNew(STextBlock)
									.Text_Lambda([this]() { return ToolTipCost; })
									.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
									.ColorAndOpacity(FLinearColor::Black)
								]
								+ SVerticalBox::Slot()
								[
									SNew(STextBlock)
									.Visibility_Lambda([this]() { return ToolTipCostBruteForce.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
									.Text_Lambda([this]() { return ToolTipCostBruteForce; })
									.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
									.ColorAndOpacity(FLinearColor::Black)
								]
							]);
					}

					break;
				}
			}
		}
	}

	return FReply::Unhandled();
}

FDebugger* FDebugger::Debugger;
void FDebugger::Initialize()
{
	Debugger = new FDebugger;
	IModularFeatures::Get().RegisterModularFeature(IRewindDebuggerExtension::ModularFeatureName, Debugger);
}

void FDebugger::Shutdown()
{
	IModularFeatures::Get().UnregisterModularFeature(IRewindDebuggerExtension::ModularFeatureName, Debugger);
	delete Debugger;
}

void FDebugger::RecordingStarted(IRewindDebugger*)
{
	UE::Trace::ToggleChannel(TEXT("PoseSearch"), true);
}

void FDebugger::RecordingStopped(IRewindDebugger*)
{
	UE::Trace::ToggleChannel(TEXT("PoseSearch"), false);
}

bool FDebugger::IsPIESimulating()
{
	return Debugger->RewindDebugger->IsPIESimulating();
}

bool FDebugger::IsRecording()
{
	return Debugger->RewindDebugger->IsRecording();
}

double FDebugger::GetRecordingDuration()
{
	return Debugger->RewindDebugger->GetRecordingDuration();
}

UWorld* FDebugger::GetWorld()
{
	return Debugger->RewindDebugger->GetWorldToVisualize();
}

const IRewindDebugger* FDebugger::GetRewindDebugger()
{
	return Debugger->RewindDebugger;
}

void FDebugger::Update(float DeltaTime, IRewindDebugger* InRewindDebugger)
{
	// Update active rewind debugger in use
	RewindDebugger = InRewindDebugger;
}

void FDebugger::OnViewClosed(uint64 InAnimInstanceId)
{
	TArray<TSharedRef<FDebuggerViewModel>>& Models = Debugger->ViewModels;
	for (int32 i = 0; i < Models.Num(); ++i)
	{
		if (Models[i]->AnimInstanceId == InAnimInstanceId)
		{
			Models.RemoveAtSwap(i);
			return;
		}
	}
	// Should always be a valid remove
	checkNoEntry();
}

TSharedPtr<FDebuggerViewModel> FDebugger::GetViewModel(uint64 InAnimInstanceId)
{
	TArray<TSharedRef<FDebuggerViewModel>>& Models = Debugger->ViewModels;
	for (int32 i = 0; i < Models.Num(); ++i)
	{
		if (Models[i]->AnimInstanceId == InAnimInstanceId)
		{
			return Models[i];
		}
	}
	return nullptr;
}

TSharedPtr<SDebuggerView> FDebugger::GenerateInstance(uint64 InAnimInstanceId)
{
	ViewModels.Add_GetRef(MakeShared<FDebuggerViewModel>(InAnimInstanceId))->RewindDebugger.BindStatic(&FDebugger::GetRewindDebugger);

	TSharedPtr<SDebuggerView> DebuggerView;

	SAssignNew(DebuggerView, SDebuggerView, InAnimInstanceId)
		.ViewModel_Static(&FDebugger::GetViewModel, InAnimInstanceId)
		.OnViewClosed_Static(&FDebugger::OnViewClosed);

	return DebuggerView;
}

FDebuggerTrack::FDebuggerTrack(uint64 InObjectId)
: ObjectId(InObjectId)
{
	CostTimelineView = SNew(SCostTimelineView);
}

FSlateIcon FDebuggerTrack::GetIconInternal()
{
#if WITH_EDITOR
	return FSlateIconFinder::FindIconForClass(UAnimInstance::StaticClass());
#else
	return FSlateIcon();
#endif
}

bool FDebuggerTrack::UpdateInternal()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PoseSearchDebuggerTrack::UpdateInternal);
	CostTimelineView->UpdateInternal(ObjectId);

	if (TSharedPtr<IRewindDebuggerView> PinnedView = View.Pin())
	{
		PinnedView->SetTimeMarker(IRewindDebugger::Instance()->CurrentTraceTime());
	}

	return false;
}

FName FDebuggerTrack::GetNameInternal() const
{
	static const FName Name("PoseSearchDebugger");
	return Name;
}

FText FDebuggerTrack::GetDisplayNameInternal() const
{
	return LOCTEXT("PoseSearchDebuggerTabTitle", "Pose Search");
}

TSharedPtr<SWidget> FDebuggerTrack::GetTimelineViewInternal()
{
	return CostTimelineView;
}

TSharedPtr<SWidget> FDebuggerTrack::GetDetailsViewInternal()
{
	TSharedPtr<IRewindDebuggerView> RewindDebuggerView = FDebugger::Get()->GenerateInstance(ObjectId);
	View = RewindDebuggerView;
	return RewindDebuggerView;
}

// FDebuggerTrackCreator
///////////////////////////////////////////////////

FName FDebuggerTrackCreator::GetTargetTypeNameInternal() const
{
	static FName TargetTypeName = "AnimInstance";
	return TargetTypeName;
}
	
static const FName PoseSearchDebuggerName("PoseSearchDebugger");

FName FDebuggerTrackCreator::GetNameInternal() const
{
	return PoseSearchDebuggerName;
}

void FDebuggerTrackCreator::GetTrackTypesInternal(TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const
{
	Types.Add({PoseSearchDebuggerName, LOCTEXT("Pose Search", "Pose Search")});
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FDebuggerTrackCreator::CreateTrackInternal(uint64 ObjectId) const
{
	return MakeShared<FDebuggerTrack>(ObjectId);
}

bool FDebuggerTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PoseSearchDebugger::HasDebugInfoInternal);
	// Get provider and validate
	const TraceServices::IAnalysisSession* Session = IRewindDebugger::Instance()->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

	const FTraceProvider* PoseSearchProvider = Session->ReadProvider<FTraceProvider>(FTraceProvider::ProviderName);
	const IAnimationProvider* AnimationProvider = Session->ReadProvider<IAnimationProvider>("AnimationProvider");
	const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider");
	if (!(PoseSearchProvider && AnimationProvider && GameplayProvider))
	{
		return false;
	}

	bool bHasData = false;

	PoseSearchProvider->EnumerateMotionMatchingStateTimelines(ObjectId, [&bHasData](const FTraceProvider::FMotionMatchingStateTimeline& InTimeline)
	{
		bHasData = true;
	});

	return bHasData;
}

} // namespace UE::PoseSearch

#undef LOCTEXT_NAMESPACE
