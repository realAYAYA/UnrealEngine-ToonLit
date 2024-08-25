// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PoseSearchHistoryCollectorAnimNodeLibrary.generated.h"

struct FAnimNode_PoseSearchHistoryCollector;

USTRUCT(BlueprintType)
struct FPoseSearchHistoryCollectorAnimNodeReference : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef FAnimNode_PoseSearchHistoryCollector FInternalNodeType;
};

// Exposes operations that can be run on a Pose History node via Anim Node Functions such as "On Become Relevant" and "On Update".
UCLASS()
class POSESEARCH_API UPoseSearchHistoryCollectorAnimNodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Get a Pose History node context from an anim node context */
	UFUNCTION(BlueprintCallable, Category = "Animation|PoseHistory", meta = (BlueprintThreadSafe, ExpandEnumAsExecs = "Result"))
	static FPoseSearchHistoryCollectorAnimNodeReference ConvertToPoseHistoryNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result);

	/** Get a Pose History node context from an anim node context (pure) */
	UFUNCTION(BlueprintPure, Category = "Animation|PoseHistory", meta = (BlueprintThreadSafe, DisplayName = "Convert to Pose History Node"))
	static void ConvertToPoseHistoryNodePure(const FAnimNodeReference& Node, FPoseSearchHistoryCollectorAnimNodeReference& PoseSearchHistoryCollectorNode, bool& Result)
	{
		EAnimNodeReferenceConversionResult ConversionResult;
		PoseSearchHistoryCollectorNode = ConvertToPoseHistoryNode(Node, ConversionResult);
		Result = (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded);
	}

	UFUNCTION(BlueprintPure, Category = "Animation|PoseHistory", meta = (BlueprintThreadSafe, DisplayName = "Get Pose History Node Trajectory"))
	static void GetPoseHistoryNodeTrajectory(const FPoseSearchHistoryCollectorAnimNodeReference& PoseSearchHistoryCollectorNode, FPoseSearchQueryTrajectory& Trajectory);

	UFUNCTION(BlueprintCallable, Category = "Animation|PoseHistory", meta = (BlueprintThreadSafe, DisplayName = "Set Pose History Node Trajectory"))
	static void SetPoseHistoryNodeTrajectory(const FPoseSearchHistoryCollectorAnimNodeReference& PoseSearchHistoryCollectorNode, const FPoseSearchQueryTrajectory& Trajectory);
};