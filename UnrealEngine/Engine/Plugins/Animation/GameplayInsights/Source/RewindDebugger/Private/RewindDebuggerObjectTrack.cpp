// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerObjectTrack.h"
#include "RewindDebuggerFallbackTrack.h"
#include "IRewindDebugger.h"
#include "IGameplayProvider.h"
#include "IAnimationProvider.h"
#include "RewindDebuggerViewCreators.h"
#include "RewindDebuggerTrackCreators.h"
#include "SEventTimelineView.h"
#include "Styling/SlateIconFinder.h"
#include "ObjectTrace.h"
#include "IRewindDebuggerDoubleClickHandler.h"


#define LOCTEXT_NAMESPACE "RewindDebuggerObjectTrack"

namespace RewindDebugger
{
	
// check if an object is or is a subclass of a type by name, based on Insights traced type info
static bool IsTargetType(uint64 ObjectId, FName TargetTypeName, const TraceServices::IAnalysisSession& Session)
{
	TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);

	const IGameplayProvider* GameplayProvider = Session.ReadProvider<IGameplayProvider>("GameplayProvider");
	const FObjectInfo& ObjectInfo = GameplayProvider->GetObjectInfo(ObjectId);
	uint64 ClassId = ObjectInfo.ClassId;

	while (ClassId != 0)
	{
		const FClassInfo& ClassInfo = GameplayProvider->GetClassInfo(ClassId);
		if (ClassInfo.Name == TargetTypeName)
		{
			return true;
		}
		ClassId = ClassInfo.SuperId;
	}

	return false;
}
	
FRewindDebuggerObjectTrack::FRewindDebuggerObjectTrack(uint64 InObjectId, const FString& InObjectName, bool bInAddController)
	: ObjectName(InObjectName)
	, ObjectId(InObjectId)
	, bAddController(bInAddController)
	, bDisplayNameValid(false)
{
	ExistenceRange = MakeShared<SEventTimelineView::FTimelineEventData>();
	
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);
	FRewindDebuggerTrackCreators::EnumerateCreators([this, &Session, RewindDebugger](const IRewindDebuggerTrackCreator* Creator)
	{
		if (IsTargetType(ObjectId, Creator->GetTargetTypeName(), *Session))
		{
			TrackChildren.Push({Creator, nullptr});
		}
	});

	// sort by creators by priority + name
	TrackChildren.Sort([](const FTrackCreatorAndTrack& A, const FTrackCreatorAndTrack& B)
		{
			const int32 SortOrderPriorityA = A.Creator->GetSortOrderPriority();
			const int32 SortOrderPriorityB = B.Creator->GetSortOrderPriority();
			
			if (SortOrderPriorityA != SortOrderPriorityB)
			{
				return SortOrderPriorityA > SortOrderPriorityB;
			}
			
			return A.Creator->GetName().ToString() < B.Creator->GetName().ToString();
		});
}
	
TSharedPtr<SWidget> FRewindDebuggerObjectTrack::GetTimelineViewInternal()
{
	return SNew(SEventTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.EventData_Raw(this, &FRewindDebuggerObjectTrack::GetExistenceRange);
}

bool FRewindDebuggerObjectTrack::HandleDoubleClickInternal()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	static const FName HandlerFeatureName = IRewindDebuggerDoubleClickHandler::ModularFeatureName;

	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
		
	if (const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);
	
		const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider");
		const FObjectInfo& ObjectInfo = GameplayProvider->GetObjectInfo(ObjectId);
		uint64 ClassId = ObjectInfo.ClassId;
		bool bHandled = false;
		
		const int32 NumExtensions = ModularFeatures.GetModularFeatureImplementationCount(HandlerFeatureName);

		// iterate up the class hierarchy, looking for a registered double click handler, until we find the one that succeeeds that is most specific to the type of this object
		while (ClassId != 0 && !bHandled)
		{
			const FClassInfo& ClassInfo = GameplayProvider->GetClassInfo(ClassId);
		
			for (int32 ExtensionIndex = 0; ExtensionIndex < NumExtensions; ++ExtensionIndex)
			{
				IRewindDebuggerDoubleClickHandler* Handler = static_cast<IRewindDebuggerDoubleClickHandler*>(ModularFeatures.GetModularFeatureImplementation(HandlerFeatureName, ExtensionIndex));
				if (Handler->GetTargetTypeName() == ClassInfo.Name)
				{
					if (Handler->HandleDoubleClick(RewindDebugger))
					{
						bHandled = true;
						break;
					}
				}
			}
		
			ClassId = ClassInfo.SuperId;
		}
	}
	
	return true;
}


void FRewindDebuggerObjectTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for (FTrackCreatorAndTrack& TrackChild : TrackChildren)
	{
		if (TrackChild.Track.IsValid())
		{
			IteratorFunction(TrackChild.Track);
		}
	}
	
	for (TSharedPtr<FRewindDebuggerTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};
	
FText FRewindDebuggerObjectTrack::GetDisplayNameInternal() const
{
	if (!bDisplayNameValid)
	{
		if (ObjectId == 0)
		{
			DisplayName = FText::FromString(ObjectName);
		}
		else
		{
			IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
			const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession();
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

			const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider");

			const FObjectInfo& ObjectInfo = GameplayProvider->GetObjectInfo(ObjectId);

			DisplayName = FText::FromString(ObjectInfo.Name);

			if (const FWorldInfo* WorldInfo = GameplayProvider->FindWorldInfoFromObject(ObjectId))
			{
				bool bIsServer = WorldInfo->NetMode == FWorldInfo::ENetMode::DedicatedServer;

				if (bIsServer)
				{
					DisplayName = FText::Format(NSLOCTEXT("RewindDebuggerTrack", " (Server)", "{0} (Server)"), FText::FromString(ObjectInfo.Name));
				}
			}
		}

		bDisplayNameValid = true;
	}

	return DisplayName;
}

bool FRewindDebuggerObjectTrack::UpdateInternal()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebuggerObjectTrack::UpdateInternal);
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

	const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider");

	bool bChanged = false;

	TRange<double> Existence = GameplayProvider->GetObjectRecordingLifetime(ObjectId);

	ExistenceRange->Windows.SetNum(0,EAllowShrinking::No);
	if (Existence.HasLowerBound() && Existence.HasUpperBound())
	{
		ExistenceRange->Windows.Add({Existence.GetLowerBoundValue(), Existence.GetUpperBoundValue(), LOCTEXT("Object Existence","Object Existence"), LOCTEXT("Object Existence","Object Existence"), FLinearColor(0.1f,0.11f,0.1f)});
	}

	if (!Icon.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebuggerObjectTrack::FindIcon);
		if (const FObjectInfo* ObjectInfo = GameplayProvider->FindObjectInfo(ObjectId))
		{
			if (const UClass* FoundClass = GameplayProvider->FindClass(ObjectInfo->ClassId))
			{
				Icon = FSlateIconFinder::FindIconForClass(FoundClass);
				bChanged = true;
			}
		}
	}

	TArray<uint64, TInlineAllocator<32>> FoundObjects;

	FoundObjects.Add(ObjectId); // prevent debug views from being removed

	// add debug views as children

	for(FTrackCreatorAndTrack& TrackChild : TrackChildren)
	{
		const bool bHasDebugInfo = TrackChild.Creator->HasDebugInfo(ObjectId);

		if (TrackChild.Track.IsValid())
		{
			if (!bHasDebugInfo)
			{
				bChanged = true;
				TrackChild.Track.Reset();
			}
		}
		else
		{
			if (bHasDebugInfo)
			{
				bChanged = true;
				TrackChild.Track = TrackChild.Creator->CreateTrack(ObjectId);
			}
		}
	}

	// Fallback codepath to add views with no track implementation
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebuggerObjectTrack::UpdateInternal_AddViews);
		FRewindDebuggerViewCreators::EnumerateCreators([this, &Session, &bChanged, RewindDebugger](const IRewindDebuggerViewCreator* Creator)
			{
				int32 FoundIndex = Children.FindLastByPredicate([&bChanged, this, Creator, RewindDebugger](const TSharedPtr<FRewindDebuggerTrack>& Track) { return Track->GetName() == Creator->GetName(); });

				bool bHasDebugInfo = Creator->HasDebugInfo(ObjectId)
					&& IsTargetType(ObjectId, Creator->GetTargetTypeName(), *Session);

				if (FoundIndex >= 0)
				{
					if (!bHasDebugInfo)
					{
						bChanged = true;
						Children.RemoveAt(FoundIndex);
					}
				}
				else
				{
					if (bHasDebugInfo)
					{
						bChanged = true;
						TSharedPtr<FRewindDebuggerTrack> Track = MakeShared<FRewindDebuggerFallbackTrack>(ObjectId, Creator);
						Children.Add(Track);
					}
				}
			}
		);
	}

	// add child objects/components
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebuggerObjectTrack::UpdateInternal_AddChildComponents);

		TRange<double> TraceRange = RewindDebugger->GetCurrentTraceRange();
		TRange<double> ViewRange = RewindDebugger->GetCurrentViewRange();

		GameplayProvider->EnumerateSubobjects(ObjectId, [this, &FoundObjects, &bChanged,&TraceRange, &ViewRange,GameplayProvider](uint64 SubobjectId)
			{
				TRange<double> Lifetime = GameplayProvider->GetObjectRecordingLifetime(SubobjectId);
				TRange<double> Overlap = TRange<double>::Intersection(Lifetime, ViewRange);
				// only display the track if the lifetime of the object and the view range overlap
				if (!Overlap.IsEmpty())
				{
					int32 FoundIndex = Children.FindLastByPredicate([SubobjectId](const TSharedPtr<FRewindDebuggerTrack>& Info) { return Info->GetObjectId() == SubobjectId; });
					bool bExisted = FoundIndex >= 0;

					if (!bExisted)
					{
						FoundIndex = Children.Num();

						Children.Add(MakeShared<FRewindDebuggerObjectTrack>(SubobjectId, ""));
					}

					bChanged = bChanged || !bExisted;
					FoundObjects.Add(SubobjectId);
				}
			});
	}

	// add controller and it's component hierarchy if one is attached
	if (bAddController)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebuggerObjectTrack::FindController);
		// Should probably update this to use a time range and return all posessing controllers from the visible time range.  For now just returns the one at the current time.
		if (uint64 ControllerId = GameplayProvider->FindPossessingController(ObjectId, RewindDebugger->CurrentTraceTime()))
		{
			const FObjectInfo& ObjectInfo = GameplayProvider->GetObjectInfo(ControllerId);
			const int32 FoundIndex = Children.FindLastByPredicate([ObjectInfo](const TSharedPtr<FRewindDebuggerTrack>& Info) { return Info->GetObjectId() == ObjectInfo.Id; });

			if (FoundIndex < 0)
			{
				bChanged = true;
				Children.Add(MakeShared<FRewindDebuggerObjectTrack>(ObjectInfo.Id,ObjectInfo.Name));
			}
			
			FoundObjects.Add(ControllerId);
		}
	}

	// remove any components previously in the list that were not found in this time range.
	for (int Index = Children.Num() - 1; Index >= 0; Index--)
	{
		if (!FoundObjects.Contains(Children[Index]->GetObjectId()))
		{
			bChanged = true;
			Children.RemoveAt(Index);
		}
	}

	if (bChanged)
	{
		// sort child object tracks by name
		Children.Sort([](const TSharedPtr<FRewindDebuggerTrack>& A, const TSharedPtr<FRewindDebuggerTrack>& B)
		{
			return A->GetDisplayName().ToString() < B->GetDisplayName().ToString();
		});
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebuggerObjectTrack::UpdateChilden);
		for (auto& Child : Children)
		{
			if (Child->Update())
			{
				bChanged = true;
			}
		}
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebuggerObjectTrack::UpdateTrackChilden);
		for (auto& TrackChild : TrackChildren)
		{
			if (TrackChild.Track.IsValid())
			{
				if (TrackChild.Track->Update())
				{
					bChanged = true;
				}
			}
		}
	}

	return bChanged;
}

}

#undef LOCTEXT_NAMESPACE