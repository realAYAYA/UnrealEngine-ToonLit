// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "LevelInstance/LevelInstanceTypes.h"

class ILevelInstanceEditorPivotInterface;
class ILevelInstanceInterface;
class ULevelStreaming;
class AActor;

class ENGINE_API FLevelInstanceEditorPivotHelper
{
public:
	static ILevelInstanceEditorPivotInterface* Create(ILevelInstanceInterface* LevelInstance, ULevelStreaming* LevelStreaming);
	static void SetPivot(ILevelInstanceEditorPivotInterface* PivotInterface, ELevelInstancePivotType PivotType, AActor* PivotToActor);
	static FVector GetPivot(ILevelInstanceEditorPivotInterface* PivotInterface, ELevelInstancePivotType PivotType, AActor* PivotToActor);
	static void ShowPivotLocation(const FVector& PivotLocation);
};

#endif