// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeAnimationTrackSetFactoryNode.h"

#include "LevelSequence.h"

UInterchangeAnimationTrackSetFactoryNode::UInterchangeAnimationTrackSetFactoryNode()
{
	CustomAnimationTrackUids.Initialize(Attributes, TEXT("AnimationTrackUids"));
}

UClass* UInterchangeAnimationTrackSetFactoryNode::GetObjectClass() const
{
	return ULevelSequence::StaticClass();
}

int32 UInterchangeAnimationTrackSetFactoryNode::GetCustomAnimationTrackUidCount() const
{
	return CustomAnimationTrackUids.GetCount();
}

void UInterchangeAnimationTrackSetFactoryNode::GetCustomAnimationTrackUids(TArray<FString>& OutAnimationTrackUids) const
{
	CustomAnimationTrackUids.GetItems(OutAnimationTrackUids);
}

void UInterchangeAnimationTrackSetFactoryNode::GetCustomAnimationTrackUid(const int32 Index, FString& OutAnimationTrackUid) const
{
	CustomAnimationTrackUids.GetItem(Index, OutAnimationTrackUid);
}

bool UInterchangeAnimationTrackSetFactoryNode::AddCustomAnimationTrackUid(const FString& AnimationTrackUid)
{
	return CustomAnimationTrackUids.AddItem(AnimationTrackUid);
}

bool UInterchangeAnimationTrackSetFactoryNode::RemoveCustomAnimationTrackUid(const FString& AnimationTrackUid)
{
	return CustomAnimationTrackUids.RemoveItem(AnimationTrackUid);
}

bool UInterchangeAnimationTrackSetFactoryNode::GetCustomFrameRate(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(FrameRate, float);
}

bool UInterchangeAnimationTrackSetFactoryNode::SetCustomFrameRate(const float& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(FrameRate, float);
}
