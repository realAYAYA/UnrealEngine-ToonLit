// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SkeletalMeshToolMenuContext.generated.h"

class ISkeletalMeshEditor;

UCLASS(MinimalAPI)
class USkeletalMeshToolMenuContext : public UObject
{
	GENERATED_BODY()
public:

	TWeakPtr<ISkeletalMeshEditor> SkeletalMeshEditor;
};
