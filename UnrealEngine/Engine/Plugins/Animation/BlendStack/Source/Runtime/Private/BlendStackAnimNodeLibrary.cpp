// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "BlendStack/AnimNode_BlendStack.h"
#include "BlendStack/AnimNode_BlendStackInput.h"
#include "BlendStack/BlendStackDefines.h"

FBlendStackAnimNodeReference UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FBlendStackAnimNodeReference>(Node, Result);
}

UAnimationAsset* UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset(const FAnimNodeReference& Node)
{
	if (FAnimNode_BlendStackInput* BlendStackInput = Node.GetAnimNodePtr<FAnimNode_BlendStackInput>())
	{
		if (BlendStackInput->Player && *BlendStackInput->Player )
		{
			return (*BlendStackInput->Player)->GetAnimationAsset();
		}
	}
	return nullptr;
}

float UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAssetTime(const FAnimNodeReference& Node)
{
	if (FAnimNode_BlendStackInput* BlendStackInput = Node.GetAnimNodePtr<FAnimNode_BlendStackInput>())
	{
		if (BlendStackInput->Player && *BlendStackInput->Player )
		{
			return (*BlendStackInput->Player)->GetAccumulatedTime();
		}
	}
	return 0.f;
}

void UBlendStackAnimNodeLibrary::BlendTo(const FAnimUpdateContext& Context, 
										const FBlendStackAnimNodeReference& BlendStackNode, 
										UAnimationAsset* AnimationAsset,
										float AnimationTime,
										bool bLoop,
										bool bMirrored,
										float BlendTime,
										FVector BlendParameters,
										float WantedPlayRate,
										float ActivationDelay)
{
	if (AnimationAsset != nullptr)
	{
		if (FAnimNode_BlendStack* BlendStackNodePtr = BlendStackNode.GetAnimNodePtr<FAnimNode_BlendStack>())
		{
			if (const FAnimationUpdateContext* AnimationUpdateContext = Context.GetContext())
			{
				BlendStackNodePtr->BlendTo(
					*AnimationUpdateContext,
					AnimationAsset,
					AnimationTime,
					bLoop,
					bMirrored,
					BlendStackNodePtr->MirrorDataTable,
					BlendTime,
					BlendStackNodePtr->BlendProfile,
					BlendStackNodePtr->BlendOption,
					BlendStackNodePtr->bUseInertialBlend,
					BlendParameters,
					WantedPlayRate,
					ActivationDelay);
			}
			else
			{
				UE_LOG(LogBlendStack, Warning, TEXT("UBlendStackAnimNodeLibrary::BlendTo called with an invalid context."));
			}
		}
		else
		{
			UE_LOG(LogBlendStack, Warning, TEXT("UBlendStackAnimNodeLibrary::BlendTo called with an invalid type."));
		}
	}
}

void UBlendStackAnimNodeLibrary::ForceBlendNextUpdate(const FBlendStackAnimNodeReference& BlendStackNode)
{
	if (FAnimNode_BlendStack* BlendStackNodePtr = BlendStackNode.GetAnimNodePtr<FAnimNode_BlendStack>())
	{
		BlendStackNodePtr->ForceBlendNextUpdate();
	}
	else
	{
		UE_LOG(LogBlendStack, Warning, TEXT("UBlendStackAnimNodeLibrary::ForceBlendNextUpdate called with an invalid type."));
	}
}