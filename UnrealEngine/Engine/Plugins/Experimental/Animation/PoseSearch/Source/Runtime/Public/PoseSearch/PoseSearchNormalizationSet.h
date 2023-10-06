// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PoseSearchNormalizationSet.generated.h"

class UPoseSearchDatabase;

UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental, meta = (DisplayName = "Normalization Set"))
class POSESEARCH_API UPoseSearchNormalizationSet : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NormalizationSet")
	TArray<TObjectPtr<const UPoseSearchDatabase>> Databases;

	void AddUniqueDatabases(TArray<TObjectPtr<const UPoseSearchDatabase>>& UniqueDatabases) const;
};