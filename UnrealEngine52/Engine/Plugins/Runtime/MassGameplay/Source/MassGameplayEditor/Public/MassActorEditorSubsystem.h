// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "MassActorEditorSubsystem.generated.h"


struct FMassActorManager;

UCLASS()
class MASSGAMEPLAYEDITOR_API UMassActorEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	FMassActorManager& GetMutableActorManager() { return *ActorManager.Get(); }

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	TSharedPtr<FMassActorManager> ActorManager;
};
