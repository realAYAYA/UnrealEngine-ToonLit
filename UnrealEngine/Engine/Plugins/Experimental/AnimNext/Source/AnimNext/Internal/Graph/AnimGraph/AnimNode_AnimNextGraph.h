// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNode_CustomProperty.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphInstancePtr.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "Context.h"
#include "AnimNode_AnimNextGraph.generated.h"

/**
 * Animation node that allows a AnimNextGraph output to be used in an animation graph
 */
USTRUCT()
struct ANIMNEXT_API FAnimNode_AnimNextGraph : public FAnimNode_CustomProperty
{
	GENERATED_BODY()

	FAnimNode_AnimNextGraph();

	// FAnimNode_Base interface
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext & Output) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }

	virtual void InitializeProperties(const UObject* InSourceInstance, UClass* InTargetClass) override;
	virtual void PropagateInputProperties(const UObject* InSourceInstance) override;

private:
#if WITH_EDITOR
	virtual void HandleObjectsReinstanced_Impl(UObject* InSourceObject, UObject* InTargetObject, const TMap<UObject*, UObject*>& OldToNewInstanceMap) override;
#endif
private:

	/** The input pose we will pass to the graph */
	UPROPERTY(EditAnywhere, Category = Links, meta = (DisplayName = "Source"))
	FPoseLink SourceLink;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	TObjectPtr<UAnimNextGraph> AnimNextGraph;

	// Our graph instance, we own it
	UPROPERTY()
	FAnimNextGraphInstancePtr GraphInstance;

	/*
	 * Max LOD that this node is allowed to run
	 * For example if you have LODThreshold to be 2, it will run until LOD 2 (based on 0 index)
	 * when the component LOD becomes 3, it will stop update/evaluate
	 * currently transition would be issue and that has to be re-visited
	 */
	UPROPERTY(EditAnywhere, Category = Performance, meta = (DisplayName = "LOD Threshold"))
	int32 LODThreshold;

protected:
	virtual UClass* GetTargetClass() const override { return AnimNextGraph ? AnimNextGraph->StaticClass() : nullptr; }
	
public:

	void PostSerialize(const FArchive& Ar);

	friend class UAnimGraphNode_AnimNextGraph;
};

template<>
struct TStructOpsTypeTraits<FAnimNode_AnimNextGraph> : public TStructOpsTypeTraitsBase2<FAnimNode_AnimNextGraph>
{
	enum
	{
		WithCopy = false,
		WithPostSerialize = true,
	};
};