// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/SoftObjectPtr.h"
#include "AvaOutlinerClipboardData.generated.h"

class AActor;

UCLASS()
class UAvaOutlinerClipboardData : public UObject
{
	GENERATED_BODY()

public:
	/** Copied Actor names in the order that they should be processed by Outliner */
	UPROPERTY()
	TArray<FName> ActorNames;
};
