// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNode_CustomProperty.h"
#include "Animation/InputScaleBias.h"
#include "AnimNextInterfaceGraph.h"
#include "AnimNode_AnimNextInterfaceGraph.generated.h"

class UNodeMappingContainer;

/**
 * Animation node that allows a AnimNextInterfaceGraph output to be used in an animation graph
 */
USTRUCT()
struct ANIMNEXTINTERFACEGRAPH_API FAnimNode_AnimNextInterfaceGraph : public FAnimNode_CustomProperty
{
	GENERATED_BODY()

	FAnimNode_AnimNextInterfaceGraph();
	~FAnimNode_AnimNextInterfaceGraph();

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
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	TObjectPtr<UAnimNextInterfaceGraph> AnimNextInterfaceGraph;

	/*
	 * Max LOD that this node is allowed to run
	 * For example if you have LODThreadhold to be 2, it will run until LOD 2 (based on 0 index)
	 * when the component LOD becomes 3, it will stop update/evaluate
	 * currently transition would be issue and that has to be re-visited
	 */
	UPROPERTY(EditAnywhere, Category = Performance, meta = (DisplayName = "LOD Threshold"))
	int32 LODThreshold;

protected:
	virtual UClass* GetTargetClass() const override { return AnimNextInterfaceGraph ? AnimNextInterfaceGraph->StaticClass() : nullptr; }
	
public:

	void PostSerialize(const FArchive& Ar);

	friend class UAnimGraphNode_AnimNextInterfaceGraph;
};

template<>
struct TStructOpsTypeTraits<FAnimNode_AnimNextInterfaceGraph> : public TStructOpsTypeTraitsBase2<FAnimNode_AnimNextInterfaceGraph>
{
	enum
	{
		WithPostSerialize = true,
	};
};