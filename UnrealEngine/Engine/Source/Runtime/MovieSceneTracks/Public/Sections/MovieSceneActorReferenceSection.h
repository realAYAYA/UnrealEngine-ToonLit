// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelData.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Curves/IntegralCurve.h"
#include "Curves/KeyHandle.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Math/Range.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"
#include "Misc/Guid.h"
#include "MovieSceneFwd.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSection.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneActorReferenceSection.generated.h"

class IMovieScenePlayer;
class UObject;
struct FKeyHandle;
struct FMovieSceneSequenceHierarchy;
struct FMovieSceneSequenceID;

USTRUCT()
struct FMovieSceneActorReferenceKey
{
	GENERATED_BODY()

	FMovieSceneActorReferenceKey()
	{}

	FMovieSceneActorReferenceKey(const FMovieSceneObjectBindingID& InBindingID)
		: Object(InBindingID)
	{}

	friend bool operator==(const FMovieSceneActorReferenceKey& A, const FMovieSceneActorReferenceKey& B)
	{
		return A.Object == B.Object && A.ComponentName == B.ComponentName && A.SocketName == B.SocketName;
	}

	friend bool operator!=(const FMovieSceneActorReferenceKey& A, const FMovieSceneActorReferenceKey& B)
	{
		return A.Object != B.Object || A.ComponentName != B.ComponentName || A.SocketName != B.SocketName;
	}

	UPROPERTY(EditAnywhere, Category="Key")
	FMovieSceneObjectBindingID Object;

	UPROPERTY(EditAnywhere, Category="Key")
	FName ComponentName;

	UPROPERTY(EditAnywhere, Category="Key")
	FName SocketName;
};

/** A curve of events */
USTRUCT()
struct FMovieSceneActorReferenceData : public FMovieSceneChannel
{
	GENERATED_BODY()

	FMovieSceneActorReferenceData()
		: DefaultValue()
	{}

	/**
	 * Access a mutable interface for this channel's data
	 *
	 * @return An object that is able to manipulate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<FMovieSceneActorReferenceKey> GetData()
	{
		return TMovieSceneChannelData<FMovieSceneActorReferenceKey>(&KeyTimes, &KeyValues, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel's data
	 *
	 * @return An object that is able to interrogate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<const FMovieSceneActorReferenceKey> GetData() const
	{
		return TMovieSceneChannelData<const FMovieSceneActorReferenceKey>(&KeyTimes, &KeyValues);
	}

	/**
	 * Evaluate this channel
	 *
	 * @param InTime     The time to evaluate at
	 * @param OutValue   A value to receive the result
	 * @return true if the channel was evaluated successfully, false otherwise
	 */
	MOVIESCENETRACKS_API bool Evaluate(FFrameTime InTime, FMovieSceneActorReferenceKey& OutValue) const;

public:

	// ~ FMovieSceneChannel Interface
	MOVIESCENETRACKS_API virtual void GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles) override;
	MOVIESCENETRACKS_API virtual void GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes) override;
	MOVIESCENETRACKS_API virtual void SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes) override;
	MOVIESCENETRACKS_API virtual void DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles) override;
	MOVIESCENETRACKS_API virtual void DeleteKeys(TArrayView<const FKeyHandle> InHandles) override;
	MOVIESCENETRACKS_API virtual void DeleteKeysFrom(FFrameNumber InTime, bool bDeleteKeysBefore) override;
	MOVIESCENETRACKS_API virtual void ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate) override;
	MOVIESCENETRACKS_API virtual TRange<FFrameNumber> ComputeEffectiveRange() const override;
	MOVIESCENETRACKS_API virtual int32 GetNumKeys() const override;
	MOVIESCENETRACKS_API virtual void Reset() override;
	MOVIESCENETRACKS_API virtual void Offset(FFrameNumber DeltaPosition) override;
	MOVIESCENETRACKS_API virtual void ClearDefault() override;
	MOVIESCENETRACKS_API virtual FKeyHandle GetHandle(int32 Index) override;
	MOVIESCENETRACKS_API virtual int32 GetIndex(FKeyHandle Handle) override;

public:

	/**
	 * Set this channel's default value that should be used when no keys are present
	 *
	 * @param InDefaultValue The desired default value
	 */
	FORCEINLINE void SetDefault(FMovieSceneActorReferenceKey InDefaultValue)
	{
		DefaultValue = InDefaultValue;
	}

	/**
	 * Get this channel's default value that will be used when no keys are present
	 *
	 * @return (Optional) The channel's default value
	 */
	FORCEINLINE const FMovieSceneActorReferenceKey& GetDefault() const
	{
		return DefaultValue;
	}

	/**
	 * Upgrade legacy data by appending to the end of the array. Assumes sorted data
	 */
	void UpgradeLegacyTime(UObject* Context, double Time, FMovieSceneActorReferenceKey Value)
	{
		FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();
		FFrameNumber KeyTime = UpgradeLegacyMovieSceneTime(nullptr, LegacyFrameRate, Time);

		check(KeyTimes.Num() == 0 || KeyTime >= KeyTimes.Last());

		KeyTimes.Add(KeyTime);
		KeyValues.Add(Value);
	}

private:

	/** Sorted array of key times */
	UPROPERTY(meta=(KeyTimes))
	TArray<FFrameNumber> KeyTimes;

	/** Default value used when there are no keys */
	UPROPERTY()
	FMovieSceneActorReferenceKey DefaultValue;

	/** Array of values that correspond to each key time */
	UPROPERTY(meta=(KeyValues))
	TArray<FMovieSceneActorReferenceKey> KeyValues;

	/** This needs to be a UPROPERTY so it gets saved into editor transactions but transient so it doesn't get saved into assets. */
	UPROPERTY(Transient)
	FMovieSceneKeyHandleMap KeyHandles;
};

/**
 * A single actor reference point section
 */
UCLASS(MinimalAPI)
class UMovieSceneActorReferenceSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	//~ UObject interface
	virtual void PostLoad() override;

	//~ UMovieSceneSection interface
	virtual void OnBindingIDsUpdated(const TMap<UE::MovieScene::FFixedObjectBindingID, UE::MovieScene::FFixedObjectBindingID>& OldFixedToNewFixedMap, FMovieSceneSequenceID LocalSequenceID, const FMovieSceneSequenceHierarchy* Hierarchy, IMovieScenePlayer& Player) override;

	const FMovieSceneActorReferenceData& GetActorReferenceData() const { return ActorReferenceData; }

private:

	UPROPERTY()
	FMovieSceneActorReferenceData ActorReferenceData;

private:

	/** Curve data */
	UPROPERTY()
	FIntegralCurve ActorGuidIndexCurve_DEPRECATED;

	UPROPERTY()
	TArray<FString> ActorGuidStrings_DEPRECATED;
};

inline bool EvaluateChannel(const FMovieSceneActorReferenceData* InChannel, FFrameTime InTime, FMovieSceneActorReferenceKey& OutValue)
{
	return InChannel->Evaluate(InTime, OutValue);
}
