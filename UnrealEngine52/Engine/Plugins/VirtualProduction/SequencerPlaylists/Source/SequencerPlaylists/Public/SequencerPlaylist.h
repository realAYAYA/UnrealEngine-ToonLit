// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SequencerPlaylist.generated.h"


class USequencerPlaylistItem;


UCLASS(BlueprintType)
class USequencerPlaylist : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, BlueprintReadWrite, Category="SequencerPlaylists")
	FText Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="SequencerPlaylists")
	TArray<TObjectPtr<USequencerPlaylistItem>> Items;
};
