// Copyright Epic Games, Inc. All Rights Reserved.

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlueprintFunctionLibrary)

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintFuncLibrary, Log, All);

UBlueprintFunctionLibrary::UBlueprintFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UBlueprintFunctionLibrary::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	return GEngine->GetGlobalFunctionCallspace(Function, this, Stack);
}


