// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerTrackFilterBase.h"
#include "Misc/IFilter.h"
#include "CollectionManagerTypes.h"
#include "Styling/SlateIconFinder.h"
#include "Styling/AppStyle.h"
#include "MovieSceneSequence.h"

#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneLevelVisibilityTrack.h"
#include "Tracks/MovieSceneParticleTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

#include "Camera/CameraComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystem.h"

#include "Channels/MovieSceneChannel.h"

class UWorld;

#define LOCTEXT_NAMESPACE "Sequencer"

class FSequencerTrackFilterCollection : public TSharedFromThis<FSequencerTrackFilterCollection>
{
public:
	/**
	 *	Returns whether the specified Item passes any of the filters in the collection
	 *
	 *	@param	InItem	The Item to check against all child filter restrictions
	 *  @param  InText  The Items Displayed Name
	 *	@return			Whether the Item passed any child filter restrictions
	 */
	 // @todo Maybe this should get moved in to TFilterCollection
	bool PassesAnyFilters(/*ItemType*/ FTrackFilterType InItem, const FText& InText) const
	{
		for (int32 Index = 0; Index < ChildFilters.Num(); Index++)
		{
			if (ChildFilters[Index]->PassesFilterWithDisplayName(InItem, InText))
			{
				return true;
			}
		}

		return false;
	}
	/**
 *	Returns whether the specified MovieSceneChannel  passes any of the filters in the collection
 *
 *	@param	InChannel	The Item to check against all child filter restrictions
 *	@return			Whether the Item passed any child filter restrictions
 */
	bool PassesAnyFilters(FMovieSceneChannel* InChannel) const
	{
		for (int32 Index = 0; Index < ChildFilters.Num(); Index++)
		{
			if (ChildFilters[Index]->PassesFilterChannel(InChannel))
			{
				return true;
			}
		}
		return false;
	}
	// @todo Maybe this should get moved in to TFilterCollection
	bool Contains(const TSharedPtr< FSequencerTrackFilter >& InItem) const
	{
		for (const TSharedPtr<FSequencerTrackFilter>& Filter : ChildFilters)
		{
			if (InItem == Filter)
			{
				return true;
			}
		}
		return false;
	}

	// @todo Maybe this should get moved in to TFilterCollection
	void RemoveAll()
	{
		for (auto Iterator = ChildFilters.CreateIterator(); Iterator; ++Iterator)
		{
			const TSharedPtr< FSequencerTrackFilter >& Filter = *Iterator;

			if (Filter.IsValid())
			{
				Filter->OnChanged().RemoveAll(this);
			}
		}

		ChildFilters.Empty();

		ChangedEvent.Broadcast();
	}

	/**
	 * TFilterCollection destructor. Unregisters from all child filter changed delegates.
	 */
	~FSequencerTrackFilterCollection()
	{
		for (auto Iterator = ChildFilters.CreateIterator(); Iterator; ++Iterator)
		{
			const TSharedPtr< FSequencerTrackFilter>& Filter = *Iterator;

			if (Filter.IsValid())
			{
				Filter->OnChanged().RemoveAll(this);
			}
		}
	}

	/**
	 *	Adds the specified Filter to the collection
	 *
	 *	@param	Filter	The filter object to add to the collection
	 *	@return			The index in the collection at which the filter was added
	 */
	int32 Add(const TSharedPtr<FSequencerTrackFilter >& Filter)
	{
		int32 ExistingIdx;
		if (ChildFilters.Find(Filter, ExistingIdx))
		{
			// The filter already exists, don't add a new one but return the index where it was found.
			return ExistingIdx;
		}

		if (Filter.IsValid())
		{
			Filter->OnChanged().AddSP(this, &FSequencerTrackFilterCollection::OnChildFilterChanged);
		}

		int32 Result = ChildFilters.Add(Filter);
		ChangedEvent.Broadcast();

		return Result;
	}

	/**
	 *	Removes as many instances of the specified Filter as there are in the collection
	 *
	 *	@param	Filter	The filter object to remove from the collection
	 *	@return			The number of Filters removed from the collection
	 */
	int32 Remove(const TSharedPtr<FSequencerTrackFilter >& Filter)
	{
		if (Filter.IsValid())
		{
			Filter->OnChanged().RemoveAll(this);
		}

		int32 Result = ChildFilters.Remove(Filter);

		// Don't broadcast if the collection didn't change
		if (Result > 0)
		{
			ChangedEvent.Broadcast();
		}

		return Result;
	}

	/**
	 *	Gets the filter at the specified index
	 *
	 *	@param	Index	The index of the requested filter in the ChildFilters array.
	 *	@return			Filter at the specified index
	 */
	TSharedPtr< FSequencerTrackFilter> GetFilterAtIndex(int32 Index)
	{
		return ChildFilters[Index];
	}

	/** Returns the number of Filters in the collection */
	FORCEINLINE int32 Num() const
	{
		return ChildFilters.Num();
	}


	/** Broadcasts anytime the restrictions of any of the child Filters change */
	DECLARE_EVENT(FSequencerTrackFilterCollection, FChangedEvent);
	FChangedEvent& OnChanged() { return ChangedEvent; }

protected:

	/**
	 *	Called when a child Filter restrictions change and broadcasts the FilterChanged delegate
	 *	for the collection
	 */
	void OnChildFilterChanged()
	{
		ChangedEvent.Broadcast();
	}

	/** The array of child filters */
	TArray< TSharedPtr< FSequencerTrackFilter > > ChildFilters;

	/**	Fires whenever any filter in the collection changes */
	FChangedEvent ChangedEvent;

public:

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE TArray< TSharedPtr< FSequencerTrackFilter > >::RangedForIteratorType      begin()       { return ChildFilters.begin(); }
	FORCEINLINE TArray< TSharedPtr< FSequencerTrackFilter > >::RangedForConstIteratorType begin() const { return ChildFilters.begin(); }
	FORCEINLINE TArray< TSharedPtr< FSequencerTrackFilter > >::RangedForIteratorType      end()	        { return ChildFilters.end(); }
	FORCEINLINE TArray< TSharedPtr< FSequencerTrackFilter > >::RangedForConstIteratorType end()   const { return ChildFilters.end(); }
};

class FSequencerTrackFilter_AudioTracks : public FSequencerTrackFilter_ClassType< UMovieSceneAudioTrack >
{
	virtual FString GetName() const override { return TEXT("SequencerAudioTracksFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_AudioTracks", "Audio"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_AudioTracksToolTip", "Show only Audio tracks."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Audio"); }

	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override
	{
		if (InSequence && InSequence->IsTrackSupported(UMovieSceneAudioTrack::StaticClass()) == ETrackSupport::NotSupported)
		{
			return false;
		}

		static UClass* LevelSequenceClass = FindObject<UClass>(nullptr, TEXT("/Script/LevelSequence.LevelSequence"), true);
		static UClass* WidgetAnimationClass = FindObject<UClass>(nullptr, TEXT("/Script/UMG.WidgetAnimation"), true);
		return InSequence != nullptr &&
			((LevelSequenceClass != nullptr && InSequence->GetClass()->IsChildOf(LevelSequenceClass)) ||
			(WidgetAnimationClass != nullptr && InSequence->GetClass()->IsChildOf(WidgetAnimationClass)));
	}
};

class FSequencerTrackFilter_EventTracks : public FSequencerTrackFilter_ClassType< UMovieSceneEventTrack >
{
	virtual FString GetName() const override { return TEXT("SequencerEventTracksFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_EventTracks", "Event"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_EventTracksToolTip", "Show only Event tracks."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Event"); }

	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override
	{
		if (InSequence && InSequence->IsTrackSupported(UMovieSceneEventTrack::StaticClass()) == ETrackSupport::NotSupported)
		{
			return false;
		}

		static UClass* LevelSequenceClass = FindObject<UClass>(nullptr, TEXT("/Script/LevelSequence.LevelSequence"), true);
		static UClass* WidgetAnimationClass = FindObject<UClass>(nullptr, TEXT("/Script/UMG.WidgetAnimation"), true);
		return InSequence != nullptr &&
			((LevelSequenceClass != nullptr && InSequence->GetClass()->IsChildOf(LevelSequenceClass)) ||
			(WidgetAnimationClass != nullptr && InSequence->GetClass()->IsChildOf(WidgetAnimationClass)));
	}
};

class FSequencerTrackFilter_LevelVisibilityTracks : public FSequencerTrackFilter_ClassType< UMovieSceneLevelVisibilityTrack >
{
	virtual FString GetName() const override { return TEXT("SequencerLevelVisibilityTracksFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_LevelVisibilityTracks", "Level Visibility"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_LevelVisibilityTracksToolTip", "Show only Level Visibility tracks."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.LevelVisibility"); }
};

class FSequencerTrackFilter_ParticleTracks : public FSequencerTrackFilter_ClassType< UMovieSceneParticleTrack >
{
	virtual FString GetName() const override { return TEXT("SequencerParticleTracksFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_ParticleTracks", "Particle Systems"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_ParticleTracksToolTip", "Show only Particle System tracks."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIconFinder::FindIconForClass(UParticleSystem::StaticClass()); }
};

class FSequencerTrackFilter_CinematicShotTracks : public FSequencerTrackFilter_ClassType< UMovieSceneCinematicShotTrack >
{
	virtual FString GetName() const override { return TEXT("SequencerCinematicShotTracksFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_CinematicShotTracks", "Shots"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_CinematicShotTracksToolTip", "Show only Shot tracks."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.CinematicShot"); }
};

class FSequencerTrackFilter_SubTracks : public FSequencerTrackFilter_ClassType< UMovieSceneSubTrack >
{
	virtual FString GetName() const override { return TEXT("SequencerSubTracksFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_SubTracks", "Subsequences"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_SubTracksToolTip", "Show only Subsequence tracks."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Sub"); }

	// IFilter implementation
	virtual bool PassesFilter(FTrackFilterType InItem) const override
	{
		return InItem->IsA(UMovieSceneSubTrack::StaticClass()) && !InItem->IsA(UMovieSceneCinematicShotTrack::StaticClass());
	}
};

class FSequencerTrackFilter_SkeletalMeshObjects : public FSequencerTrackFilter_ComponentType< USkeletalMeshComponent >
{
	virtual FString GetName() const override { return TEXT("SequencerSkeletalMeshObjectsFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_SkeletalMeshObjects", "Skeletal Mesh"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_SkeletalMeshObjectsToolTip", "Show only Skeletal Mesh objects."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIconFinder::FindIconForClass(USkeletalMeshComponent::StaticClass()); }
};

class FSequencerTrackFilter_CameraObjects : public FSequencerTrackFilter_ComponentType< UCameraComponent >
{
	virtual FString GetName() const override { return TEXT("SequencerCameraObjectsFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_CameraObjects", "Cameras"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_CameraObjectsToolTip", "Show only Camera objects and Camera Cut tracks."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIconFinder::FindIconForClass(UCameraComponent::StaticClass()); }

	// IFilter implementation
	virtual bool PassesFilter(FTrackFilterType InItem) const override
	{
		if (InItem->IsA(UMovieSceneCameraCutTrack::StaticClass()))
		{
			return true;
		}
		return FSequencerTrackFilter_ComponentType< UCameraComponent >::PassesFilter(InItem);
	}
};

class FSequencerTrackFilter_LightObjects : public FSequencerTrackFilter_ComponentType< ULightComponentBase >
{
	virtual FString GetName() const override { return TEXT("SequencerLightObjectsFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequencerTrackFilter_LightObjects", "Lights"); }
	virtual FText GetToolTipText() const override { return LOCTEXT("SequencerTrackFilter_LightObjectsToolTip", "Show only Light objects."); }
	virtual FSlateIcon GetIcon() const override { return FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Light"); }
};

class FSequencerTrackFilter_LevelFilter : public FSequencerTrackFilter
{
public:
	~FSequencerTrackFilter_LevelFilter();

	virtual bool PassesFilter(FTrackFilterType InItem) const override;

	virtual FString GetName() const override { return TEXT("SequencerSubLevelFilter"); }
	virtual FText GetDisplayName() const override { return FText::GetEmpty(); }
	virtual FText GetToolTipText() const override { return FText::GetEmpty(); }

	void UpdateWorld(UWorld* World);
	void ResetFilter();

	bool IsActive() const { return HiddenLevels.Num() > 0; }

	bool IsLevelHidden(const FString& LevelName) const;
	void HideLevel(const FString& LevelName);
	void UnhideLevel(const FString& LevelName);

private:
	void HandleLevelsChanged();

	// List of sublevels which should not pass filter
	TArray<FString> HiddenLevels;

	TWeakObjectPtr<UWorld> CachedWorld;
};


class FSequencerTrackFilter_Animated : public FSequencerTrackFilter
{
public:
	FSequencerTrackFilter_Animated();
	~FSequencerTrackFilter_Animated();

	virtual FString GetName() const override { return TEXT("AnimatedFilter"); }
	virtual FText GetDisplayName() const override { return LOCTEXT("SequenceTrackFilter_Animated", "Animated Tracks"); }
	virtual FText GetToolTipText() const override;
	virtual FSlateIcon GetIcon() const { return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.IconKeyUser"); }

	virtual bool ShouldUpdateOnTrackValueChanged() const override
	{
		return true;
	}

	virtual bool PassesFilter(FTrackFilterType InItem) const override
	{
		return false;
	}

	virtual bool PassesFilterChannel(FMovieSceneChannel* InMovieSceneChannel) const override
	{
		return (InMovieSceneChannel && InMovieSceneChannel->GetNumKeys() > 0);
	}

	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override
	{
		static UClass* LevelSequenceClass = FindObject<UClass>(nullptr, TEXT("/Script/LevelSequence.LevelSequence"), true);
		static UClass* WidgetAnimationClass = FindObject<UClass>(nullptr, TEXT("/Script/UMG.WidgetAnimation"), true);
		return InSequence != nullptr &&
			((LevelSequenceClass != nullptr && InSequence->GetClass()->IsChildOf(LevelSequenceClass)) ||
			(WidgetAnimationClass != nullptr && InSequence->GetClass()->IsChildOf(WidgetAnimationClass)));
	}
	
	virtual void BindCommands(TSharedRef<FUICommandList> SequencerBindings, TSharedRef<FUICommandList> CurveEditorBindings, TWeakPtr<ISequencer> Sequencer) override;

private:
	mutable uint32 BindingCount;
};
#undef LOCTEXT_NAMESPACE