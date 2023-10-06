// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BakeToControlRigSettings.generated.h"

UCLASS(BlueprintType, config = EditorSettings)
class  UBakeToControlRigSettings : public UObject
{
public:
	UBakeToControlRigSettings(const FObjectInitializer& Initializer);

	GENERATED_BODY()
	/** Reduce Keys */
	UPROPERTY(EditAnywhere, Category = "Reduce Keys")
	bool bReduceKeys = false;

	/** Reduce Keys Tolerance*/
	UPROPERTY(EditAnywhere, Category = "Reduce Keys")
	float Tolerance = 0.001f;

	/** Resets the default properties. */
	void Reset();
};


