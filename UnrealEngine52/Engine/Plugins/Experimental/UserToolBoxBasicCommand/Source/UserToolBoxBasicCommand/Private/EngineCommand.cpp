// Copyright Epic Games, Inc. All Rights Reserved.


#include "EngineCommand.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
  void UEngineCommand::Execute()
 {
 	GEngine->Exec(GWorld, *Command);
 }