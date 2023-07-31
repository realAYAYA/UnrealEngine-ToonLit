// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "UObject/Object.h"
#include "Engine/AssetUserData.h"
#include "AnimSequenceLevelSequenceLink.generated.h"

class ULevelSequence;

/** Link To Level Sequence That may be driving the anim sequence*/
UCLASS(BlueprintType)
class LEVELSEQUENCE_API UAnimSequenceLevelSequenceLink : public UAssetUserData
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = Property)
	FGuid SkelTrackGuid;

	UPROPERTY(BlueprintReadWrite, Category = Property)
	FSoftObjectPath PathToLevelSequence;

	void SetLevelSequence(ULevelSequence* InLevelSequence);
	ULevelSequence* ResolveLevelSequence();
};