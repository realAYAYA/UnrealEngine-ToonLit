// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PoseSearchNormalizationSet.generated.h"

class UPoseSearchDatabase;

UCLASS(BlueprintType, Category = "Animation|Pose Search", meta = (DisplayName = "Pose Search Normalization Set"))
class POSESEARCH_API UPoseSearchNormalizationSet : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NormalizationSet")
	TArray<TObjectPtr<const UPoseSearchDatabase>> Databases;

	void AddUniqueDatabases(TArray<const UPoseSearchDatabase*>& UniqueDatabases) const;
};