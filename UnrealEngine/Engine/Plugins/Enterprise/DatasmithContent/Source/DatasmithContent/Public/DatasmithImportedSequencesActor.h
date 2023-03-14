// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "DatasmithImportedSequencesActor.generated.h"

class ULevelSequence;

UCLASS()
class DATASMITHCONTENT_API ADatasmithImportedSequencesActor : public AActor
{
public:

	GENERATED_BODY()

	ADatasmithImportedSequencesActor(const FObjectInitializer& Init);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImportedSequences")
	TArray<TObjectPtr<ULevelSequence>> ImportedSequences;

    UFUNCTION(BlueprintCallable, Category="ImportedSequences")
	void PlayLevelSequence(ULevelSequence* SequenceToPlay);
};