// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MotionMatchingAnimNodeLibrary.generated.h"

struct FAnimNode_MotionMatching;
class UPoseSearchDatabase;

USTRUCT(BlueprintType)
struct FMotionMatchingAnimNodeReference : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef FAnimNode_MotionMatching FInternalNodeType;
};

// Exposes operations that can be run on a Motion Matching node via Anim Node Functions such as "On Become Relevant" and "On Update".
UCLASS()
class POSESEARCH_API UMotionMatchingAnimNodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Get a motion matching node context from an anim node context */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe, ExpandEnumAsExecs = "Result"))
	static FMotionMatchingAnimNodeReference ConvertToMotionMatchingNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result);

	/** Get a motion matching node context from an anim node context (pure) */
	UFUNCTION(BlueprintPure, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe, DisplayName = "Convert to Motion Matching Node"))
	static void ConvertToMotionMatchingNodePure(const FAnimNodeReference& Node, FMotionMatchingAnimNodeReference& MotionMatchingNode, bool& Result)
	{
		EAnimNodeReferenceConversionResult ConversionResult;
		MotionMatchingNode = ConvertToMotionMatchingNode(Node, ConversionResult);
		Result = (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded);
	}

	UFUNCTION(BlueprintPure, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe, DisplayName = "Get Motion Matching Search Result"))
	static void GetMotionMatchingSearchResult(const FMotionMatchingAnimNodeReference& MotionMatchingNode, FPoseSearchBlueprintResult& Result, bool& bIsResultValid);

	/**
	 * Set the database to search on the motion matching node. This overrides the Database property on the motion matching node.
	 * @param MotionMatchingNode - The motion matching node to operate on.
	 * @param Database - The database for the motion matching node to search.
	 * @param InterruptMode - mode to control the continuing pose search (the current animation that's playing)
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe))
	static void SetDatabaseToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, UPoseSearchDatabase* Database, EPoseSearchInterruptMode InterruptMode);

	/**
	 * Set the database to search on the motion matching node. This overrides the Database property on the motion matching node.
	 * @param MotionMatchingNode - The motion matching node to operate on.
	 * @param Databases - Array of databases for the motion matching node to search.
	 * @param InterruptMode - mode to control the continuing pose search (the current animation that's playing)
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe))
	static void SetDatabasesToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, const TArray<UPoseSearchDatabase*>& Databases, EPoseSearchInterruptMode InterruptMode);

	/**
	 * Clear the effects of SetDatabaseToSearch/SetDatabasesToSearch and resume searching the Database property on the motion matching node.
	 * @param MotionMatchingNode - The motion matching node to operate on.
	 * @param InterruptMode - mode to control the continuing pose search (the current animation that's playing)
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe))
	static void ResetDatabasesToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, EPoseSearchInterruptMode InterruptMode);

	/**
	 * Ignore the continuing pose (the current clip that's playing) and force a new search.
	 * @param MotionMatchingNode - The motion matching node to operate on.
	 * @param InterruptMode - mode to control the continuing pose search (the current animation that's playing)
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe))
	static void SetInterruptMode(const FMotionMatchingAnimNodeReference& MotionMatchingNode, EPoseSearchInterruptMode InterruptMode);
};