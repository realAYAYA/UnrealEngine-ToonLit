// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "CinePrestreamingData.generated.h"

USTRUCT()
struct FCinePrestreamingVTData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Cinematic Prestreaming")
	TArray<uint64> PageIds;
};

USTRUCT()
struct FCinePrestreamingNaniteData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Cinematic Prestreaming")
	TArray<uint32> RequestData;
};

UCLASS()
class CINEMATICPRESTREAMING_API UCinePrestreamingData : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Cinematic Prestreaming")
	TArray<FFrameNumber> Times;
	
	UPROPERTY(EditAnywhere, Category = "Cinematic Prestreaming")
	TArray<FCinePrestreamingVTData> VirtualTextureDatas;

	UPROPERTY(EditAnywhere, Category = "Cinematic Prestreaming")
	TArray<FCinePrestreamingNaniteData> NaniteDatas;

	// UObject Interface
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);
	// ~UObject Interface

	/** Time that this asset was generated (in UTC). Used to give better context about how up to date an asset is as they are hard to preview. */
	UPROPERTY(EditAnywhere, Category = "Cinematic Prestreaming")
	FDateTime RecordedTime;

	/** What resolution was this asset generated at? Recordings are resolution dependent as different mips will be chosen for different resolutions. */
	UPROPERTY(EditAnywhere, Category = "Cinematic Prestreaming")
	FIntPoint RecordedResolution;
};
