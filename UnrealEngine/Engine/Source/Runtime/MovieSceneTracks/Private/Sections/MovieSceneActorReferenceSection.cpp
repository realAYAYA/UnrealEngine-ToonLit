// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneActorReferenceSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSequenceID.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "IMovieScenePlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneActorReferenceSection)

bool FMovieSceneActorReferenceData::Evaluate(FFrameTime InTime, FMovieSceneActorReferenceKey& OutValue) const
{
	if (KeyTimes.Num())
	{
		const int32 Index = FMath::Max(0, Algo::UpperBound(KeyTimes, InTime.FrameNumber)-1);
		OutValue = KeyValues[Index];
		return true;
	}

	OutValue = DefaultValue;
	return true;
}

void FMovieSceneActorReferenceData::GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles)
{
	GetData().GetKeys(WithinRange, OutKeyTimes, OutKeyHandles);
}

void FMovieSceneActorReferenceData::GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes)
{
	GetData().GetKeyTimes(InHandles, OutKeyTimes);
}

void FMovieSceneActorReferenceData::SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes)
{
	GetData().SetKeyTimes(InHandles, InKeyTimes);
}

void FMovieSceneActorReferenceData::DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles)
{
	GetData().DuplicateKeys(InHandles, OutNewHandles);
}

void FMovieSceneActorReferenceData::DeleteKeys(TArrayView<const FKeyHandle> InHandles)
{
	GetData().DeleteKeys(InHandles);
}

void FMovieSceneActorReferenceData::DeleteKeysFrom(FFrameNumber InTime, bool bDeleteKeysBefore)
{
	// Insert a key at the current time to maintain evaluation
	if (GetData().GetTimes().Num() > 0)
	{
		FMovieSceneActorReferenceKey Value;
		Evaluate(InTime, Value);
		GetData().UpdateOrAddKey(InTime, Value);
	}

	GetData().DeleteKeysFrom(InTime, bDeleteKeysBefore);
}

FKeyHandle FMovieSceneActorReferenceData::GetHandle(int32 Index)
{
	return GetData().GetHandle(Index);
}

int32 FMovieSceneActorReferenceData::GetIndex(FKeyHandle Handle)
{
	return GetData().GetIndex(Handle);
}

void FMovieSceneActorReferenceData::ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	GetData().ChangeFrameResolution(SourceRate, DestinationRate);
}

TRange<FFrameNumber> FMovieSceneActorReferenceData::ComputeEffectiveRange() const
{
	return GetData().GetTotalRange();
}

int32 FMovieSceneActorReferenceData::GetNumKeys() const
{
	return KeyTimes.Num();
}

void FMovieSceneActorReferenceData::Reset()
{
	KeyTimes.Reset();
	KeyValues.Reset();
	KeyHandles.Reset();
	DefaultValue = FMovieSceneActorReferenceKey();
}

void FMovieSceneActorReferenceData::Offset(FFrameNumber DeltaPosition)
{
	GetData().Offset(DeltaPosition);
}

void FMovieSceneActorReferenceData::ClearDefault()
{
	DefaultValue = FMovieSceneActorReferenceKey();
}

UMovieSceneActorReferenceSection::UMovieSceneActorReferenceSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
#if WITH_EDITOR

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(ActorReferenceData, FMovieSceneChannelMetaData());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(ActorReferenceData);

#endif
}

void UMovieSceneActorReferenceSection::PostLoad()
{
	Super::PostLoad();

	if (ActorGuidStrings_DEPRECATED.Num())
	{
		TArray<FGuid> Guids;

		for (const FString& ActorGuidString : ActorGuidStrings_DEPRECATED)
		{
			FGuid& ActorGuid = Guids[Guids.Emplace()];
			FGuid::Parse( ActorGuidString,  ActorGuid);
		}

		if (Guids.IsValidIndex(ActorGuidIndexCurve_DEPRECATED.GetDefaultValue()))
		{
			FMovieSceneObjectBindingID DefaultValue = UE::MovieScene::FRelativeObjectBindingID(Guids[ActorGuidIndexCurve_DEPRECATED.GetDefaultValue()]);
			ActorReferenceData.SetDefault(DefaultValue);
		}

		for (auto It = ActorGuidIndexCurve_DEPRECATED.GetKeyIterator(); It; ++It)
		{
			if (ensure(Guids.IsValidIndex(It->Value)))
			{
				FMovieSceneObjectBindingID BindingID = UE::MovieScene::FRelativeObjectBindingID(Guids[It->Value]);
				ActorReferenceData.UpgradeLegacyTime(this, It->Time, BindingID);
			}
		}
	}
}

void UMovieSceneActorReferenceSection::OnBindingIDsUpdated(const TMap<UE::MovieScene::FFixedObjectBindingID, UE::MovieScene::FFixedObjectBindingID>& OldFixedToNewFixedMap, FMovieSceneSequenceID LocalSequenceID, const FMovieSceneSequenceHierarchy* Hierarchy, IMovieScenePlayer& Player)
{
	UE::MovieScene::FFixedObjectBindingID DefaultFixedBindingID = ActorReferenceData.GetDefault().Object.ResolveToFixed(LocalSequenceID, Player);

	if (OldFixedToNewFixedMap.Contains(DefaultFixedBindingID))
	{
		Modify();

		FMovieSceneActorReferenceKey NewDefaultValue = ActorReferenceData.GetDefault();
		NewDefaultValue.Object = OldFixedToNewFixedMap[DefaultFixedBindingID].ConvertToRelative(LocalSequenceID, Hierarchy);

		ActorReferenceData.SetDefault(NewDefaultValue);
	}

	for (FMovieSceneActorReferenceKey& Key : ActorReferenceData.GetData().GetValues())
	{
		UE::MovieScene::FFixedObjectBindingID KeyFixedBindingID = Key.Object.ResolveToFixed(LocalSequenceID, Player);

		if (OldFixedToNewFixedMap.Contains(KeyFixedBindingID))
		{
			Modify();

			Key.Object = OldFixedToNewFixedMap[KeyFixedBindingID].ConvertToRelative(LocalSequenceID, Hierarchy);
		}
	}
}


