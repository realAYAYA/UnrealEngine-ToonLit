// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"
#include "GameFramework/Actor.h"
#include "Misc/IFilter.h"
#include "MovieSceneSequence.h"

class FMenuBuilder;
class FUICommandList;
class ISequencer;

typedef const UObject* FTrackFilterType;

struct FMovieSceneChannel;

class FSequencerTrackFilter : public IFilter<FTrackFilterType>
{
public:
	/** Returns the system name for this filter */
	virtual FString GetName() const = 0;

	/** Returns the human readable name for this filter */
	virtual FText GetDisplayName() const = 0;

	/** Returns the tooltip for this filter, shown in the filters menu */
	virtual FText GetToolTipText() const = 0;

	/** Returns the icon to use in menu entries */
	virtual FSlateIcon GetIcon() const { return FSlateIcon(); }

	/** Returns whether the filter supports the sequence type */
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const
	{
		static UClass* LevelSequenceClass = FindObject<UClass>(nullptr, TEXT("/Script/LevelSequence.LevelSequence"), true);
		return InSequence != nullptr && (LevelSequenceClass != nullptr && InSequence->GetClass()->IsChildOf(LevelSequenceClass));
	}

	/** Returns whether this filter needs reevaluating any time track values have been modified, not just tree changes */
	virtual bool ShouldUpdateOnTrackValueChanged() const { return false; }

	// IFilter implementation
	DECLARE_DERIVED_EVENT(FSequencerTrackFilter, IFilter<FTrackFilterType>::FChangedEvent, FChangedEvent);
	virtual FChangedEvent& OnChanged() override { return ChangedEvent; }

	/** Returns whether the specified Item passes the Filter's restrictions */
	virtual bool PassesFilterWithDisplayName(FTrackFilterType InItem, const FText& InText) const 
	{
		return PassesFilter(InItem);
	}

	virtual bool PassesFilterChannel(FMovieSceneChannel* InMovieSceneChannel) const { return false; }

	virtual void BindCommands(TSharedRef<FUICommandList> SequencerBindings, TSharedRef<FUICommandList> CurveEditorBindings, TWeakPtr<ISequencer> Sequencer) {}

protected:
	void BroadcastChangedEvent() const { ChangedEvent.Broadcast(); }

private:
	FChangedEvent ChangedEvent;
};

/** Helper template for building Sequencer track filters based on a track or object type */
template<typename TrackType>
class FSequencerTrackFilter_ClassType : public FSequencerTrackFilter
{
public:
	// IFilter implementation
	virtual bool PassesFilter(FTrackFilterType InItem) const override
	{
		if (InItem->IsA(TrackType::StaticClass()))
		{
			return true;
		}
		return false;
	}
};

/** Helper template for building Sequencer track filters based on a component */
template<typename ComponentType>
class FSequencerTrackFilter_ComponentType : public FSequencerTrackFilter
{
public:
	// IFilter implementation
	virtual bool PassesFilter(FTrackFilterType InItem) const override
	{
		if (!InItem)
		{
			return false;
		}

		if (InItem->IsA(ComponentType::StaticClass()))
		{
			return true;
		}

		const AActor* Actor = Cast<const AActor>(InItem);
		if (Actor)
		{
			if (Actor->FindComponentByClass(ComponentType::StaticClass()))
			{
				return true;
			}
		}
		return false;
	}
};
