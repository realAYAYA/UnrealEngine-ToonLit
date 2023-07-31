// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraTeamInfoBase.h"
#include "UObject/UObjectGlobals.h"

#include "LyraTeamPrivateInfo.generated.h"

class UObject;

UCLASS()
class ALyraTeamPrivateInfo : public ALyraTeamInfoBase
{
	GENERATED_BODY()

public:
	ALyraTeamPrivateInfo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
