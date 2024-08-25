// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseSearchDebuggerReflection.generated.h"

/**
 * Reflection UObject being observed in the details view panel of the debugger
 */
UCLASS()
class POSESEARCHEDITOR_API UPoseSearchDebuggerReflection : public UObject
{
	GENERATED_BODY()

public:
	/** Time since last PoseSearch */
	UPROPERTY(VisibleAnywhere, Category="Motion Matching State", meta = (ForceUnits = "s"))
	float ElapsedPoseSearchTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State", meta = (ForceUnits = "s"))
	float AssetPlayerTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State", meta = (ForceUnits = "s"))
	float LastDeltaTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State", meta = (ForceUnits = "cm/s"))
	float SimLinearVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State", meta = (ForceUnits = "deg/s"))
	float SimAngularVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State", meta = (ForceUnits = "cm/s"))
	float AnimLinearVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State", meta = (ForceUnits = "deg/s"))
	float AnimAngularVelocity;
};
