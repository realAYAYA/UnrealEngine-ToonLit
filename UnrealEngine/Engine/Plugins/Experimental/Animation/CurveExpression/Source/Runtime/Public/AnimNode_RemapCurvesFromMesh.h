// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExpressionEvaluator.h"
#include "Animation/AnimNodeBase.h"

#include "AnimNode_RemapCurvesFromMesh.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct CURVEEXPRESSION_API FAnimNode_RemapCurvesFromMesh :
	public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = Links)
	FPoseLink SourcePose;
	
	/** This is used by default if it's valid */
	UPROPERTY(BlueprintReadWrite, transient, Category=Copy, meta=(PinShownByDefault))
	TWeakObjectPtr<USkeletalMeshComponent> SourceMeshComponent;

	/** If SourceMeshComponent is not valid, and if this is true, it will look for attached parent as a source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Copy, meta=(NeverAsPin))
	bool bUseAttachedParent = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, editfixedsize, Category = Expressions, meta = (PinHiddenByDefault))
	TMap<FName, FString> CurveExpressions;

	/** The expressions given are immutable and will not change during runtime. Improves performance. */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Expressions, meta=(NeverAsPin))
	bool bExpressionsImmutable = true;

	// Call to verify the expressions. Report any errors back.
	void VerifyExpressions();
	
	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	
private:
	void ReinitializeMeshComponent(USkeletalMeshComponent* InNewSkeletalMeshComponent, USkeletalMeshComponent* InTargetMeshComponent);
	void RefreshMeshComponent(USkeletalMeshComponent* InTargetMeshComponent);

	// this is source mesh references, so that we could compare and see if it has changed
	TWeakObjectPtr<USkeletalMeshComponent>	CurrentlyUsedSourceMeshComponent;
	TWeakObjectPtr<USkeletalMesh>			CurrentlyUsedSourceMesh;

	// target mesh 
	TWeakObjectPtr<USkeletalMesh>			CurrentlyUsedTargetMesh;
	
	TMap<FName, SmartName::UID_Type> CurveNameToUIDMap;

	TOptional<CurveExpression::Evaluator::FEngine> ExpressionEngine;
	TMap<FName, CurveExpression::Evaluator::FExpressionObject> CachedExpressions;
};
