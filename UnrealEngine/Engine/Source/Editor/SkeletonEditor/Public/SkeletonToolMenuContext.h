// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolMenuContext.h"
#include "SkeletonToolMenuContext.generated.h"

class ISkeletonEditor;

UCLASS()
class SKELETONEDITOR_API USkeletonToolMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<ISkeletonEditor> SkeletonEditor;
};