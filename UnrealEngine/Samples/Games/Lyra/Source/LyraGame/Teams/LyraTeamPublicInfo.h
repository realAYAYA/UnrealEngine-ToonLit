// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraTeamInfoBase.h"

#include "LyraTeamPublicInfo.generated.h"

class ULyraTeamCreationComponent;
class ULyraTeamDisplayAsset;
class UObject;
struct FFrame;

UCLASS()
class ALyraTeamPublicInfo : public ALyraTeamInfoBase
{
	GENERATED_BODY()

	friend ULyraTeamCreationComponent;

public:
	ALyraTeamPublicInfo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	ULyraTeamDisplayAsset* GetTeamDisplayAsset() const { return TeamDisplayAsset; }

private:
	UFUNCTION()
	void OnRep_TeamDisplayAsset();

	void SetTeamDisplayAsset(TObjectPtr<ULyraTeamDisplayAsset> NewDisplayAsset);

private:
	UPROPERTY(ReplicatedUsing=OnRep_TeamDisplayAsset)
	TObjectPtr<ULyraTeamDisplayAsset> TeamDisplayAsset;
};
