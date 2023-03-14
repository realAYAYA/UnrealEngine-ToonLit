// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSection.h"
#include "Curves/IntegralCurve.h"
#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelData.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "MovieSceneClipboard.h"
#include "Rigs/RigHierarchyDefines.h"
#include "MovieSceneControlRigSpaceChannel.generated.h"

class UControlRig;
struct FMovieSceneControlRigSpaceChannel;

DECLARE_MULTICAST_DELEGATE_TwoParams(FMovieSceneControlRigSpaceChannelSpaceNoLongerUsedEvent, FMovieSceneControlRigSpaceChannel*, const TArray<FRigElementKey>&);

UENUM(Blueprintable)
enum class EMovieSceneControlRigSpaceType : uint8
{
	Parent = 0,
	World,
	ControlRig
};

USTRUCT()
struct CONTROLRIG_API FMovieSceneControlRigSpaceBaseKey
{
	GENERATED_BODY()
	
	friend bool operator==(const FMovieSceneControlRigSpaceBaseKey& A, const FMovieSceneControlRigSpaceBaseKey& B)
	{
		return A.SpaceType == B.SpaceType && (A.SpaceType != EMovieSceneControlRigSpaceType::ControlRig || A.ControlRigElement == B.ControlRigElement);
	}

	friend bool operator!=(const FMovieSceneControlRigSpaceBaseKey& A, const FMovieSceneControlRigSpaceBaseKey& B)
	{
		return A.SpaceType != B.SpaceType || (A.SpaceType == EMovieSceneControlRigSpaceType::ControlRig && A.ControlRigElement != B.ControlRigElement);
	}

	FName GetName() const;

	UPROPERTY(EditAnywhere, Category = "Key")
	EMovieSceneControlRigSpaceType SpaceType = EMovieSceneControlRigSpaceType::Parent;
	UPROPERTY(EditAnywhere, Category = "Key")
	FRigElementKey ControlRigElement;

};

struct FSpaceRange
{
	TRange<FFrameNumber> Range;
	FMovieSceneControlRigSpaceBaseKey Key;
};

/** A curve of spaces */
USTRUCT()
struct CONTROLRIG_API FMovieSceneControlRigSpaceChannel : public FMovieSceneChannel
{
	GENERATED_BODY()

	FMovieSceneControlRigSpaceChannel()
	{}

	/**
	* Access a mutable interface for this channel's data
	*
	* @return An object that is able to manipulate this channel's data
	*/
	FORCEINLINE TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey> GetData()
	{
		return TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey>(&KeyTimes, &KeyValues, &KeyHandles, this);
	}

	/**
	* Access a constant interface for this channel's data
	*
	* @return An object that is able to interrogate this channel's data
	*/
	FORCEINLINE TMovieSceneChannelData<const FMovieSceneControlRigSpaceBaseKey> GetData() const
	{
		return TMovieSceneChannelData<const FMovieSceneControlRigSpaceBaseKey>(&KeyTimes, &KeyValues);
	}

	/**
	* Evaluate this channel
	*
	* @param InTime     The time to evaluate at
	* @param OutValue   A value to receive the result
	* @return true if the channel was evaluated successfully, false otherwise
	*/
	bool Evaluate(FFrameTime InTime, FMovieSceneControlRigSpaceBaseKey& OutValue) const;

public:

	// ~ FMovieSceneChannel Interface
	virtual void GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles) override;
	virtual void GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes) override;
	virtual void SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes) override;
	virtual void DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles) override;
	virtual void DeleteKeys(TArrayView<const FKeyHandle> InHandles) override;
	virtual void DeleteKeysFrom(FFrameNumber InTime, bool bDeleteKeysBefore) override;
	virtual void ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate) override;
	virtual TRange<FFrameNumber> ComputeEffectiveRange() const override;
	virtual int32 GetNumKeys() const override;
	virtual void Reset() override;
	virtual void Offset(FFrameNumber DeltaPosition) override;

	void GetUniqueSpaceList(TArray<FRigElementKey>* OutList);
	FMovieSceneControlRigSpaceChannelSpaceNoLongerUsedEvent& OnSpaceNoLongerUsed() { return SpaceNoLongerUsedEvent; }

	TArray <FSpaceRange> FindSpaceIntervals();

private:

	void BroadcastSpaceNoLongerUsed(const TArray<FRigElementKey>& BeforeKeys, const TArray<FRigElementKey>& AfterKeys);

	/** Sorted array of key times */
	UPROPERTY(meta = (KeyTimes))
	TArray<FFrameNumber> KeyTimes;

	/** Array of values that correspond to each key time */
	UPROPERTY(meta = (KeyValues))
	TArray<FMovieSceneControlRigSpaceBaseKey> KeyValues;

	FMovieSceneKeyHandleMap KeyHandles;
	
	FMovieSceneControlRigSpaceChannelSpaceNoLongerUsedEvent SpaceNoLongerUsedEvent;

	friend struct FControlRigSpaceChannelHelpers;
};


template<>
struct TMovieSceneChannelTraits<FMovieSceneControlRigSpaceChannel> : TMovieSceneChannelTraitsBase<FMovieSceneControlRigSpaceChannel>
{
	enum { SupportsDefaults = false };

};

inline bool EvaluateChannel(const FMovieSceneControlRigSpaceChannel* InChannel, FFrameTime InTime, FMovieSceneControlRigSpaceBaseKey& OutValue)
{
	return InChannel->Evaluate(InTime, OutValue);
}

#if WITH_EDITOR
namespace MovieSceneClipboard
{
	template<> inline FName GetKeyTypeName<FMovieSceneControlRigSpaceBaseKey>()
	{
		return "FMovieSceneControlRigSpaceBaseKey";
	}
}
#endif

//mz todoo TSharedPtr<FStructOnScope> GetKeyStruct(TMovieSceneChannelHandle<FMovieSceneControlRigSpaceChannel> Channel, FKeyHandle InHandle);