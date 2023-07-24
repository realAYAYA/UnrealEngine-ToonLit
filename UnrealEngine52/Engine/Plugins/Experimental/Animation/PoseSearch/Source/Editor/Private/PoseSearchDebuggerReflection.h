// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseSearchDebuggerReflection.generated.h"

/**
 * Used by the reflection UObject to encompass draw options for the query and database selections
 */
USTRUCT()
struct FPoseSearchDebuggerFeatureDrawOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Draw Options")
    bool bDisable = false;

	UPROPERTY(EditAnywhere, Category = "Draw Options", Meta = (EditCondition = "!bDisable"))
	bool bDrawBoneNames = false;

	UPROPERTY(EditAnywhere, Category = "Draw Options", Meta = (EditCondition = "!bDisable"))
	bool bDrawSampleLabels = false;
};

/**
 * Reflection UObject being observed in the details view panel of the debugger
 */
UCLASS()
class POSESEARCHEDITOR_API UPoseSearchDebuggerReflection : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category="Motion Matching State", Meta=(DisplayName="Current Database"))
	FString CurrentDatabaseName = "";

	/** Time since last PoseSearch jump */
	UPROPERTY(VisibleAnywhere, Category="Motion Matching State")
	float ElapsedPoseJumpTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	FString AssetPlayerAssetName = "";

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float AssetPlayerTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float LastDeltaTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float SimLinearVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float SimAngularVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float AnimLinearVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float AnimAngularVelocity;

	UPROPERTY(EditAnywhere, Category="Draw Options", Meta=(DisplayName="Query"))
	FPoseSearchDebuggerFeatureDrawOptions QueryDrawOptions;

	UPROPERTY(EditAnywhere, Category="Draw Options", Meta=(DisplayName="Selected Pose"))
	FPoseSearchDebuggerFeatureDrawOptions SelectedPoseDrawOptions;

	UPROPERTY(EditAnywhere, Category = "Draw Options")
	bool bDrawActiveSkeleton = false;
	
	UPROPERTY(EditAnywhere, Category = "Draw Options")
	bool bDrawSelectedSkeleton = false;

    UPROPERTY(VisibleAnywhere, Category="Pose Vectors")
	TArray<float> QueryPoseVector;
    	
    UPROPERTY(VisibleAnywhere, Category="Pose Vectors")
	TArray<float> ActivePoseVector;

	UPROPERTY(VisibleAnywhere, Category="Pose Vectors")
	TArray<float> SelectedPoseVector;

	UPROPERTY(VisibleAnywhere, Category="Pose Vectors")
	TArray<float> CostVector;
};
