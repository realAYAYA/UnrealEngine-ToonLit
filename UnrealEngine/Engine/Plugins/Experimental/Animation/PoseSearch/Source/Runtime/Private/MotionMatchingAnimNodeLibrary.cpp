// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"

#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/PoseSearchDefines.h"

FMotionMatchingAnimNodeReference UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FMotionMatchingAnimNodeReference>(Node, Result);
}

void UMotionMatchingAnimNodeLibrary::SetDatabaseToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, UPoseSearchDatabase* Database, bool bForceInterruptIfNew)
{
	FAnimNode_MotionMatching* MotionMatchingNodePtr = MotionMatchingNode.GetAnimNodePtr<FAnimNode_MotionMatching>();
	if (MotionMatchingNodePtr == nullptr)
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UMotionMatchingAnimNodeLibrary::SetDatabase called on an invalid context or with an invalid type"));
		return;
	}

	MotionMatchingNodePtr->SetDatabaseToSearch(Database, bForceInterruptIfNew);
}

void UMotionMatchingAnimNodeLibrary::SetDatabasesToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, const TArray<UPoseSearchDatabase*>& Databases, bool bForceInterruptIfNew)
{
	FAnimNode_MotionMatching* MotionMatchingNodePtr = MotionMatchingNode.GetAnimNodePtr<FAnimNode_MotionMatching>();
	if (MotionMatchingNodePtr == nullptr)
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UMotionMatchingAnimNodeLibrary::SetDatabases called on an invalid context or with an invalid type"));
		return;
	}

	MotionMatchingNodePtr->SetDatabasesToSearch(Databases, bForceInterruptIfNew);
}

void UMotionMatchingAnimNodeLibrary::ResetDatabasesToSearch(const FMotionMatchingAnimNodeReference& MotionMatchingNode, bool bForceInterrupt)
{
	FAnimNode_MotionMatching* MotionMatchingNodePtr = MotionMatchingNode.GetAnimNodePtr<FAnimNode_MotionMatching>();
	if (MotionMatchingNodePtr == nullptr)
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UMotionMatchingAnimNodeLibrary::ResetDatabasesToSearch called on an invalid context or with an invalid type"));
		return;
	}

	MotionMatchingNodePtr->ResetDatabasesToSearch(bForceInterrupt);

}

void UMotionMatchingAnimNodeLibrary::ForceInterruptNextUpdate(const FMotionMatchingAnimNodeReference& MotionMatchingNode)
{
	FAnimNode_MotionMatching* MotionMatchingNodePtr = MotionMatchingNode.GetAnimNodePtr<FAnimNode_MotionMatching>();
	if (MotionMatchingNodePtr == nullptr)
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UMotionMatchingAnimNodeLibrary::ForceInterruptOneFrame called on an invalid context or with an invalid type"));
		return;
	}

	MotionMatchingNodePtr->ForceInterruptNextUpdate();
}