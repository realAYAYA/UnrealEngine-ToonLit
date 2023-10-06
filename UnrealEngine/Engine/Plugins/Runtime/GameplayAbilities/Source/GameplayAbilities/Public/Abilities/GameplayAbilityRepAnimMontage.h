// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayPrediction.h"
#include "GameplayAbilityRepAnimMontage.generated.h"

class UAnimMontage;

/** Enum used by the Ability Rep Anim Montage struct to rep the quantized position or the current section id */
UENUM()
enum class ERepAnimPositionMethod
{
	Position = 0,			// reps the position in the montage to keep the client in sync (heavier, quantized, more precise)
	CurrentSectionId = 1,	// reps the current section id we want to play on the client (compact, less precise)
};

/** Data about montages that is replicated to simulated clients */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayAbilityRepAnimMontage
{
	GENERATED_USTRUCT_BODY()

	/** AnimMontage ref */
	UPROPERTY()
	TObjectPtr<UAnimMontage> AnimMontage;

	/** Play Rate */
	UPROPERTY()
	float PlayRate;

	/** Montage position */
	UPROPERTY(NotReplicated)
	float Position;

	/** Montage current blend time */
	UPROPERTY()
	float BlendTime;

	/** NextSectionID */
	UPROPERTY()
	uint8 NextSectionID;

	/** ID incremented every time a montage is played, used to trigger replication when the same montage is played multiple times. This ID wraps around when it reaches its max value. */
	UPROPERTY()
	uint8 PlayInstanceId;

	/** flag indicating we should serialize the position or the current section id */
	UPROPERTY()
	uint8 bRepPosition : 1;

	/** Bit set when montage has been stopped. */
	UPROPERTY()
	uint8 IsStopped : 1;
	
	/** Stops montage position from replicating at all to save bandwidth */
	UPROPERTY()
	uint8 SkipPositionCorrection : 1;

	/** Stops PlayRate from replicating to save bandwidth. PlayRate will be assumed to be 1.f. */
	UPROPERTY()
	uint8 bSkipPlayRate : 1;

	UPROPERTY()
	FPredictionKey PredictionKey;

	/** The current section Id used by the montage. Will only be valid if bRepPosition is false */
	UPROPERTY(NotReplicated)
	uint8 SectionIdToPlay;

	FGameplayAbilityRepAnimMontage()
	: AnimMontage(nullptr),
	PlayRate(0.f),
	Position(0.f),
	BlendTime(0.f),
	NextSectionID(0),
	PlayInstanceId(0),
	bRepPosition(true),
	IsStopped(true),
	SkipPositionCorrection(false),
	bSkipPlayRate(false),
	SectionIdToPlay(0)
	{
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	void SetRepAnimPositionMethod(ERepAnimPositionMethod InMethod);
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityRepAnimMontage> : public TStructOpsTypeTraitsBase2<FGameplayAbilityRepAnimMontage>
{
	enum
	{
		WithNetSerializer = true,
	};
};
