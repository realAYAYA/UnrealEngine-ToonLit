// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlendStackAnimNodeLibrary.generated.h"

struct FAnimNode_BlendStack;

USTRUCT(BlueprintType)
struct FBlendStackAnimNodeReference : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef FAnimNode_BlendStack FInternalNodeType;
};

// Exposes operations that can be run on a Blend Stack node via Anim Node Functions such as "On Become Relevant" and "On Update".
UCLASS(Experimental)
class BLENDSTACK_API UBlendStackAnimNodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Get a blend stack node context from an anim node context */
	UFUNCTION(BlueprintCallable, Category = "Animation|BlendStack", meta = (BlueprintThreadSafe, ExpandEnumAsExecs = "Result"))
	static FBlendStackAnimNodeReference ConvertToBlendStackNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result);
	
	
	/** Get the current AnimationAsset that is playing from a Blend Stack Input node */
    UFUNCTION(BlueprintPure, Category = "Animation|BlendStack", meta = (BlueprintThreadSafe))
    static UAnimationAsset* GetCurrentBlendStackAnimAsset(const FAnimNodeReference& Node);
	
	/** Get the current elapsed time of the animation that is playing from a Blend Stack Input node */
    UFUNCTION(BlueprintPure, Category = "Animation|BlendStack", meta = (BlueprintThreadSafe))
    static float GetCurrentBlendStackAnimAssetTime(const FAnimNodeReference& Node);

	/** Get a blend stack node context from an anim node context (pure) */
	UFUNCTION(BlueprintPure, Category = "Animation|BlendStack", meta = (BlueprintThreadSafe, DisplayName = "Convert to Blend Stack Node"))
	static void ConvertToBlendStackNodePure(const FAnimNodeReference& Node, FBlendStackAnimNodeReference& BlendStackNode, bool& Result)
	{
		EAnimNodeReferenceConversionResult ConversionResult;
		BlendStackNode = ConvertToBlendStackNode(Node, ConversionResult);
		Result = (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded);
	}

	UFUNCTION(BlueprintCallable, Category = "Animation|BlendStack", meta = (BlueprintThreadSafe, DisplayName = "Force Blend On Next Update"))
	static void ForceBlendNextUpdate(const FBlendStackAnimNodeReference& BlendStackNode);

	UFUNCTION(BlueprintCallable, Category = "Animation|BlendStack", meta = (BlueprintThreadSafe, DisplayName = "Blend To"))
	static void BlendTo(const FAnimUpdateContext& Context, 
						const FBlendStackAnimNodeReference& BlendStackNode, 
						UAnimationAsset* AnimationAsset = nullptr,
						float AnimationTime = 0.f,
						bool bLoop = false,
						bool bMirrored = false,
						float BlendTime = 0.2f,
						FVector BlendParameters = FVector::ZeroVector,
						float WantedPlayRate = 1.f,
						float ActivationDelay = 0.f);
};