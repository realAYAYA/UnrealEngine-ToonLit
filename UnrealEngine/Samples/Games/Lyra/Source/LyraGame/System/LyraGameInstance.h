// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonGameInstance.h"
#include "UObject/UObjectGlobals.h"

#include "LyraGameInstance.generated.h"

class ALyraPlayerController;
class UObject;

UCLASS(Config = Game)
class LYRAGAME_API ULyraGameInstance : public UCommonGameInstance
{
	GENERATED_BODY()

public:

	ULyraGameInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	ALyraPlayerController* GetPrimaryPlayerController() const;
	
	virtual bool CanJoinRequestedSession() const override;

protected:

	virtual void Init() override;
	virtual void Shutdown() override;
};
