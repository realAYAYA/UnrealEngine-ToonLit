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
UCLASS(Experimental)
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

	/**
	 * Set the database to search on the motion matching node. This overrides the Database property on the motion matching node.
	 * @param MotionMatchingNode - The motion matching node to operate on.
	 * @param Database - The database for the motion matching node to search.
	 * @param bForceInterruptIfNew - If true, ignore the continuing pose (the current clip that's playing) and force a search of the new database. 
		If false, the continuing pose will continue to play until a better match is found in the new database. This setting is ignored if the 
		motion matching node is already searching this database.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe))
	static void SetDatabaseToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, UPoseSearchDatabase* Database, bool bForceInterruptIfNew);

	/**
	 * Set the database to search on the motion matching node. This overrides the Database property on the motion matching node.
	 * @param MotionMatchingNode - The motion matching node to operate on.
	 * @param Databases - Array of databases for the motion matching node to search.
	 * @param bForceInterruptIfNew - If true, ignore the continuing pose (the current clip that's playing) and force a search of the new databases. 
		If false, the continuing pose will continue to play until a better match is found in one of the new databases. This setting is ignored if the 
		motion matching node is already searching this array of databases (note: a subset of databases or the same databases in a different order is
		considered to be a new array of databases).
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe))
	static void SetDatabasesToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, const TArray<UPoseSearchDatabase*>& Databases, bool bForceInterruptIfNew);

	/**
	 * Clear the effects of SetDatabaseToSearch/SetDatabasesToSearch and resume searching the Database property on the motion matching node.
	 * @param MotionMatchingNode - The motion matching node to operate on.
	 * @param bForceInterrupt - Force a search after the reset. If false, the continuing pose (the current clip that's playing) will continue
		to play until a better match is found from the Database property.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe))
	static void ResetDatabasesToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, bool bForceInterrupt);

	/**
	 * Ignore the continuing pose (the current clip that's playing) and force a new search.
	 * @param MotionMatchingNode - The motion matching node to operate on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|MotionMatching", meta = (BlueprintThreadSafe))
	static void ForceInterruptNextUpdate(const FMotionMatchingAnimNodeReference& MotionMatchingNode);
};