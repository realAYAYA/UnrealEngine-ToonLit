// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameEngine.h"

#include "LyraGameEngine.generated.h"

class IEngineLoop;
class UObject;


UCLASS()
class ULyraGameEngine : public UGameEngine
{
	GENERATED_BODY()

public:

	ULyraGameEngine(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	virtual void Init(IEngineLoop* InEngineLoop) override;
};
