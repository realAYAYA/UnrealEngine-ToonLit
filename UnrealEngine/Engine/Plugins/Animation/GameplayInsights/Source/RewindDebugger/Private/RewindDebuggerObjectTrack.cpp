// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerObjectTrack.h"
#include "RewindDebuggerFallbackTrack.h"
#include "IRewindDebugger.h"
#include "IGameplayProvider.h"
#include "IAnimationProvider.h"
#include "RewindDebuggerViewCreators.h"
#include "RewindDebuggerTrackCreators.h"
#include "SSegmentedTimelineView.h"
#include "Styling/SlateIconFinder.h"
#include "ObjectTrace.h"
#include "IRewindDebuggerDoubleClickHandler.h"

namespace RewindDebugger
{

TSharedPtr<SWidget> FRewindDebuggerObjectTrack::GetTimelineViewInternal()
{
	return SNew(SSegmentedTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.FillColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.5f))
		.SegmentData_Raw(this, &FRewindDebuggerObjectTrack::GetExistenceRange);

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

void FRewindDebuggerObjectTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
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

	check(ExistenceRange->Segments.Num() == 1);
	ExistenceRange->Segments[0] = GameplayProvider->GetObjectRecordingLifetime(ObjectId);

	if (!Icon.IsSet())
	{
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
	FRewindDebuggerTrackCreators::EnumerateCreators([this, &Session, &bChanged, RewindDebugger](const IRewindDebuggerTrackCreator* Creator)
		{
			int32 FoundIndex = Children.FindLastByPredicate([&bChanged, this, Creator, RewindDebugger](const TSharedPtr<FRewindDebuggerTrack>& Track) { return Track->GetName() == Creator->GetName(); });

			const bool bHasDebugInfo = Creator->HasDebugInfo(ObjectId) && IsTargetType(ObjectId, Creator->GetTargetTypeName(), *Session);

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
					if (TSharedPtr<FRewindDebuggerTrack> Track = Creator->CreateTrack(ObjectId))
					{
						Children.Add(Track);
					}
				}
			}
		});

	// Fallback codepath to add views with no track implementation
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

	for (auto& Child : Children)
	{
		bChanged = bChanged || Child->Update();
	}

	return bChanged;
}

}