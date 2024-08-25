// Copyright Epic Games, Inc. All Rights Reserved.

#include "MontagesTrack.h"
#include "SCurveTimelineView.h"
#include "IRewindDebugger.h"
#include "GameplayProvider.h"
#include "AnimationProvider.h"
#include "Styling/SlateIconFinder.h"
#include "SMontageView.h"
#if WITH_EDITOR
#include "AnimPreviewInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Editor.h"
#include "IAnimationEditor.h"
#include "IPersonaToolkit.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

#define LOCTEXT_NAMESPACE "MontagesTrack"

namespace RewindDebugger
{

FMontagesTrack::FMontagesTrack(uint64 InObjectId) : ObjectId(InObjectId)
{
#if WITH_EDITOR
	Icon = FSlateIconFinder::FindIconForClass(UAnimMontage::StaticClass());
#endif
}

void FMontagesTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for(TSharedPtr<FMontageTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

bool FMontagesTrack::UpdateInternal()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMontagesTrack::UpdateInternal);
	TArray<uint64> UniqueTrackIds;

	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();

	bool bChanged = false;
	
	// convert time range to from rewind debugger times to profiler times
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();
	
	// count number of unique animations in the current time range
	const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
	const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

	int AnimationCount = 0;

	if(GameplayProvider && AnimationProvider)
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		UniqueTrackIds.SetNum(0, EAllowShrinking::No);

		AnimationProvider->ReadMontageTimeline(ObjectId, [&UniqueTrackIds,&GameplayProvider, StartTime, EndTime](const FAnimationProvider::AnimMontageTimeline& InTimeline)
		{
			// this isn't very efficient, and it gets called every frame.  will need optimizing
			InTimeline.EnumerateEvents(StartTime, EndTime, [&UniqueTrackIds, StartTime, EndTime](double InStartTime, double InEndTime, uint32 InDepth, const FAnimMontageMessage& InMessage)
			{
				if (InEndTime > StartTime && InStartTime < EndTime)
				{
					UniqueTrackIds.AddUnique(InMessage.MontageId);
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		UniqueTrackIds.StableSort();
		
		AnimationCount = UniqueTrackIds.Num();

		if (Children.Num()!=AnimationCount)
			bChanged = true;
		
		Children.SetNum(AnimationCount);
		for(int i = 0; i < AnimationCount; i++)
		{
			if (!Children[i].IsValid() || Children[i].Get()->GetAssetId() != UniqueTrackIds[i])
			{
				Children[i] = MakeShared<FMontageTrack>(ObjectId, UniqueTrackIds[i]);
				bChanged = true;
			}

			if (Children[i]->Update())
			{
				bChanged = true;
			}
		}
	}

	return bChanged;
}

TSharedPtr<SWidget> FMontagesTrack::GetDetailsViewInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	return SNew(SMontageView, ObjectId, RewindDebugger->CurrentTraceTime(), *RewindDebugger->GetAnalysisSession())
		.CurrentTime_Lambda([RewindDebugger]{ return RewindDebugger->CurrentTraceTime(); });
}


FMontageTrack::FMontageTrack(uint64 InObjectId, uint64 InAssetId, FMontageTrack::ECurveType InCurveType) :
	 AssetId(InAssetId)
	, CurveType(InCurveType)
	, ObjectId(InObjectId)
{
	SetIsExpanded(false);
}


void FMontageTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for (TSharedPtr<FMontageTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

TSharedPtr<SCurveTimelineView::FTimelineCurveData> FMontageTrack::GetCurveData() const
{
	if (!CurveData.IsValid())
	{
		CurveData = MakeShared<SCurveTimelineView::FTimelineCurveData>();
	}
	
	CurvesUpdateRequested++;
	
	return CurveData;
}

bool FMontageTrack::UpdateInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	// compute curve points
	//
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();

	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
	const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

	int AnimationCount = 0;

	if(CurvesUpdateRequested > 10 && GameplayProvider && AnimationProvider)
	{
		auto& CurvePoints = CurveData->Points;
		CurvePoints.SetNum(0,EAllowShrinking::No);
		
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		
		AnimationProvider->ReadMontageTimeline(ObjectId, [AnalysisSession, StartTime, EndTime, &CurvePoints, this](const FAnimationProvider::AnimMontageTimeline& InTimeline)
		{
			InTimeline.EnumerateEvents(StartTime, EndTime, [&CurvePoints, AnalysisSession, this](double InStartTime, double InEndTime, uint32 InDepth, const FAnimMontageMessage& InMessage)
			{
				if (InMessage.MontageId == AssetId)
				{
					float Weight = 0;
					switch (CurveType)
					{
						case ECurveType::BlendWeight:					Weight = InMessage.Weight; break;
						case ECurveType::DesiredWeight:					Weight = InMessage.DesiredWeight; break; 
						case ECurveType::Position:						Weight = InMessage.Position; break; 
					}
					
					CurvePoints.Add({ InMessage.RecordingTime,	Weight });
				}
								
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		CurvesUpdateRequested = 0;
	}

	// update Icon:
	
	bool bChanged = false;

	if (CurveType == ECurveType::BlendWeight)
	{
		// Blend Weight track gets name/icon of animation
		if (CurveName.IsEmpty() && GameplayProvider)
		{
			if (const FObjectInfo* ObjectInfo = GameplayProvider->FindObjectInfo(AssetId))
			{
				CurveName = FText::FromString(ObjectInfo->Name);
				bChanged = true;

				if (const UClass* FoundClass = GameplayProvider->FindClass(ObjectInfo->ClassId))
				{
					Icon = FSlateIconFinder::FindIconForClass(FoundClass);
				}

				if (Children.Num() == 0)
				{
					for (int i = static_cast<int>(ECurveType::BlendWeight) + 1; i <= static_cast<int>(ECurveType::Position); i++)
					{
						Children.Add(MakeShared<FMontageTrack>(ObjectId, AssetId, static_cast<ECurveType>(i)));
					}
				}
			}
		}
	}
	else
	{
		// other tracks get the curve name

		if (CurveName.IsEmpty())
		{
			switch (CurveType)
			{
				case ECurveType::BlendWeight:					break;
				case ECurveType::DesiredWeight:					CurveName = LOCTEXT("Desired Weight", "Desired Weight"); break;
				case ECurveType::Position:						CurveName = LOCTEXT("Position", "Position"); break;
			}

			Icon = FSlateIcon("EditorStyle", "AnimGraph.Attribute.Curves.Icon", "AnimGraph.Attribute.Curves.Icon");

			bChanged = true;
		}
	}

	for (auto& Child : Children)
	{
		bChanged |= Child->Update();
	}

	return bChanged;
}

static FLinearColor MakeMontageCurveColor(uint32 InSeed, bool bInLine = false)
{
	FRandomStream Stream(InSeed);
	const uint8 Hue = (uint8)(Stream.FRand() * 255.0f);
	const uint8 SatVal = bInLine ? 196 : 128;
	return FLinearColor::MakeFromHSV8(Hue, SatVal, SatVal);
}

TSharedPtr<SWidget> FMontageTrack::GetTimelineViewInternal()
{
	FLinearColor Color;
	switch(CurveType)
	{
	case ECurveType::BlendWeight:
		Color = MakeMontageCurveColor(CityHash32(reinterpret_cast<char*>(&AssetId), 8));
		Color.A = 0.5f;
		break;
	case ECurveType::DesiredWeight:
		Color = FLinearColor::MakeFromHSV8(0, 50, 50);
		break;
	case ECurveType::Position:
		Color = FLinearColor::MakeFromHSV8(0, 50, 50);
		break;
	}

	FLinearColor CurveColor = Color;
	CurveColor.R *= 0.5;
	CurveColor.G *= 0.5;
	CurveColor.B *= 0.5;

	TSharedPtr<SCurveTimelineView> CurveTimelineView = SNew(SCurveTimelineView)
		.FillColor(Color)
		.CurveColor(CurveColor)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.RenderFill(CurveType == ECurveType::BlendWeight)
		.CurveData_Raw(this, &FMontageTrack::GetCurveData);

	if (CurveType == ECurveType::BlendWeight || CurveType == ECurveType::DesiredWeight)
	{
		CurveTimelineView->SetFixedRange(0, 1);
	}

	return CurveTimelineView;
}

TSharedPtr<SWidget> FMontageTrack::GetDetailsViewInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	TSharedPtr<SMontageView> MontageView = SNew(SMontageView, ObjectId, RewindDebugger->CurrentTraceTime(), *RewindDebugger->GetAnalysisSession())
			.CurrentTime_Lambda([RewindDebugger]{ return RewindDebugger->CurrentTraceTime(); });
	MontageView->SetAssetFilter(AssetId);
	return MontageView;
}
	
bool FMontageTrack::HandleDoubleClickInternal()
{
#if WITH_EDITOR
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	if (const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);

		const FObjectInfo& AssetInfo = GameplayProvider->GetObjectInfo(GetAssetId());
	
		UObject* Asset = nullptr;
		bool bMessageFound = false;
		float PlaybackTime = 0;

		float CurrentTraceTime = IRewindDebugger::Instance()->CurrentTraceTime();
		
		const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);
		const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(*AnalysisSession);
		TraceServices::FFrame Frame;
		if (FrameProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, CurrentTraceTime, Frame))
		{
			AnimationProvider->ReadMontageTimeline(ObjectId, [this, &bMessageFound, &PlaybackTime, &GameplayProvider, &Frame](const FAnimationProvider::AnimMontageTimeline& InTimeline)
			{
				InTimeline.EnumerateEvents(Frame.StartTime, Frame.EndTime, [this, &bMessageFound, &PlaybackTime, &GameplayProvider, &Frame](double InStartTime, double InEndTime, uint32 InDepth, const FAnimMontageMessage& InMessage)
				{
					if(InStartTime >= Frame.StartTime && InEndTime <= Frame.EndTime)
					{
						if (InMessage.MontageId == AssetId)
						{
							bMessageFound = true;
							PlaybackTime = InMessage.Position;
							return TraceServices::EEventEnumerate::Stop;
						}
					}
					return TraceServices::EEventEnumerate::Continue;
				});
			});
		}
		
		FString PackagePathString = FPackageName::ObjectPathToPackageName(FString(AssetInfo.PathName));

		UPackage* Package = LoadPackage(NULL, ToCStr(PackagePathString), LOAD_NoRedirects);
		if (Package)
		{
			Package->FullyLoad();
                
			FString AssetName = FPaths::GetBaseFilename(AssetInfo.PathName);
			Asset = FindObject<UObject>(Package, *AssetName);
		}
		else
		{
			// fallback for unsaved assets
			Asset = FindObject<UObject>(nullptr, AssetInfo.PathName);
		}
                    	
		if (Asset != nullptr)
		{
			if (UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				AssetEditorSS->OpenEditorForAsset(Asset);

				if (bMessageFound)
				{
					// if the asset is playing on the current frame, scrub to the appropriate time
					if (IAssetEditorInstance* Editor = AssetEditorSS->FindEditorForAsset(Asset, true))
					{
						if (Editor->GetEditorName()=="AnimationEditor")
						{
							IAnimationEditor* AnimationEditor = static_cast<IAnimationEditor*>(Editor);
							UDebugSkelMeshComponent* PreviewComponent = AnimationEditor->GetPersonaToolkit()->GetPreviewMeshComponent();
							PreviewComponent->PreviewInstance->SetPosition(PlaybackTime);
							PreviewComponent->PreviewInstance->SetPlaying(false);
						}
					}
				}
			}
		}

		

		return true;
	}
#endif
	return false;
}

FName FMontagesTrackCreator::GetTargetTypeNameInternal() const
{
	static const FName TargetTypeName = "AnimInstance";
	return TargetTypeName;
}

static const FName MontagesName("Montages");
FName FMontagesTrackCreator::GetNameInternal() const
{
	return MontagesName;
}
	
void FMontagesTrackCreator::GetTrackTypesInternal(TArray<FRewindDebuggerTrackType>& Types) const 
{
	Types.Add({MontagesName, LOCTEXT("Montages", "Montages")});
}
	
FName FMontagesTrack::GetNameInternal() const
{
	return MontagesName;
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FMontagesTrackCreator::CreateTrackInternal(uint64 ObjectId) const
{
 	return MakeShared<RewindDebugger::FMontagesTrack>(ObjectId);
}

bool FMontagesTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMontagesTrack::HasDebugInfoInternal);
	const TraceServices::IAnalysisSession* AnalysisSession = IRewindDebugger::Instance()->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	bool bHasData = false;
	if (const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName))
	{
		AnimationProvider->ReadMontageTimeline(ObjectId, [&bHasData](const FAnimationProvider::AnimMontageTimeline& InTimeline)
		{
			bHasData = true;
		});
	}
	
	return bHasData;
}

}
#undef LOCTEXT_NAMESPACE
