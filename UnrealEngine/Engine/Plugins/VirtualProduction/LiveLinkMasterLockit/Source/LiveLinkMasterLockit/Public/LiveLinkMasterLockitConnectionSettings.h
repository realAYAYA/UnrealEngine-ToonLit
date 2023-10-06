// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkMasterLockitConnectionSettings.generated.h"

USTRUCT()
struct FLiveLinkMasterLockitConnectionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FString IPAddress = TEXT("0.0.0.0");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName SubjectName = TEXT("MasterLockitDevice");
};
