// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/ObjectMacros.h"
#include "ObjectToolsTests.generated.h"

UCLASS()
class UObjectToolsTestObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UObject> StrongReference;

	UPROPERTY()
	TWeakObjectPtr<UObject> WeakReference;
};
