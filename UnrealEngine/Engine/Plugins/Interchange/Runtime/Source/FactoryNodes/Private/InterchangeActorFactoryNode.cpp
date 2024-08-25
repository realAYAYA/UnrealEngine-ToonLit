// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeActorFactoryNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeActorFactoryNode)

#if WITH_ENGINE
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#endif

UClass* UInterchangeActorFactoryNode::GetObjectClass() const
{
#if WITH_ENGINE
	FString ActorClassName;
	if (GetCustomActorClassName(ActorClassName))
	{
		UClass* ActorClass = FindObject<UClass>(nullptr, *ActorClassName);
		if (ActorClass->IsChildOf<AActor>())
		{
			return ActorClass;
		}
	}

	return AActor::StaticClass();
#else
	return nullptr;
#endif
}

bool UInterchangeActorFactoryNode::GetCustomGlobalTransform(FTransform& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(GlobalTransform, FTransform);
}

bool UInterchangeActorFactoryNode::SetCustomGlobalTransform(const FTransform& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangeActorFactoryNode, GlobalTransform, FTransform, USceneComponent);
}

bool UInterchangeActorFactoryNode::GetCustomLocalTransform(FTransform& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(GlobalTransform, FTransform);
}

bool UInterchangeActorFactoryNode::SetCustomLocalTransform(const FTransform& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangeActorFactoryNode, GlobalTransform, FTransform, USceneComponent);
}

bool UInterchangeActorFactoryNode::ApplyCustomGlobalTransformToAsset(UObject* Asset) const
{
	FTransform LocalTransform;
	if (GetCustomLocalTransform(LocalTransform))
	{
		if (USceneComponent* Component = Cast<USceneComponent>(Asset))
		{
			Component->SetRelativeTransform(LocalTransform);
			return true;
		}
	}

	FTransform GlobalTransform;
	if (GetCustomGlobalTransform(GlobalTransform))
	{
		if (USceneComponent* Component = Cast<USceneComponent>(Asset))
		{
			Component->SetWorldTransform(GlobalTransform);
			return true;
		}
	}

	return false;
}

bool UInterchangeActorFactoryNode::FillCustomGlobalTransformFromAsset(UObject* Asset)
{
	if (const USceneComponent* Component = Cast<USceneComponent>(Asset))
	{
		FTransform LocalTransform = Component->GetRelativeTransform();
		FTransform GlobalTransform = Component->GetComponentToWorld();

		return this->SetCustomLocalTransform(LocalTransform, false) || this->SetCustomGlobalTransform(GlobalTransform, false);
	}

	return false;
}

void UInterchangeActorFactoryNode::CopyWithObject(const UInterchangeFactoryBaseNode* SourceNode, UObject* Object)
{
	Super::CopyWithObject(SourceNode, Object);

	if (const UInterchangeActorFactoryNode* ActorFactoryNode = Cast<UInterchangeActorFactoryNode>(SourceNode))
	{
		COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangeActorFactoryNode, GlobalTransform, FTransform, USceneComponent::StaticClass())
	}
}

