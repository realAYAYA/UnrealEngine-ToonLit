// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DocumentationObjects.generated.h"

/**
 * A class to test the documentation policy 
 * against. For now we'll focus on the main class
 * tooltip only (this comment).
 */
UCLASS(meta=(DocumentationPolicy = "Strict"))
class UClassToDocument : public UObject
{
	GENERATED_BODY()

	UClassToDocument()
		: MyProperty(0.f)
	{
	}

	// A property for testing the policy
	UPROPERTY(meta = (UIMin = "0.0", UIMax = "1.0"))
	float MyProperty;

	// A function to test the policy
	UFUNCTION()
	void MyFunction() {}
};
