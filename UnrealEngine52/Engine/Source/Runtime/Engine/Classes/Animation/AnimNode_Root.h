// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_Root.generated.h"

// Root node of an animation tree (sink)
USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FAnimNode_Root : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Result;

#if WITH_EDITORONLY_DATA
protected:
	/** The name of this root node, used to identify the output of this graph. Filled in by the compiler, propagated from the parent graph. */
	UPROPERTY(meta=(FoldProperty, BlueprintCompilerGeneratedDefaults))
	FName Name;

	/** The group of this root node, used to group this output with others when used in a layer. */
	UPROPERTY(meta=(FoldProperty))
	FName Group;
#endif

public:	
	FAnimNode_Root();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

#if WITH_EDITORONLY_DATA
	/** Set the name of this root node, used to identify the output of this graph */
	void SetName(FName InName) { Name = InName; }

	/** Set the group of this root node, used to group this output with others when used in a layer. */
	void SetGroup(FName InGroup) { Group = InGroup; }
#endif

	/** Get the name of this root node, used to identify the output of this graph */
	FName GetName() const;

	/** Get the group of this root node, used to group this output with others when used in a layer. */
	FName GetGroup() const;
};
