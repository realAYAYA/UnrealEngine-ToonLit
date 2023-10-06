// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_RemapCurvesBase.h"

#include "AnimNode_RemapCurvesFromMesh.generated.h"


USTRUCT(BlueprintInternalUseOnly)
struct CURVEEXPRESSION_API FAnimNode_RemapCurvesFromMesh :
	public FAnimNode_RemapCurvesBase
{
	GENERATED_BODY()

	/** This is used by default if it's valid */
	UPROPERTY(BlueprintReadWrite, transient, Category=Copy, meta=(PinShownByDefault, DisplayAfter="SourcePose"))
	TWeakObjectPtr<USkeletalMeshComponent> SourceMeshComponent;

	/** If SourceMeshComponent is not valid, and if this is true, it will look for attached parent as a source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Copy, meta=(NeverAsPin))
	bool bUseAttachedParent = false;
	
	// FAnimNode_Base interface
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	bool Serialize(FArchive& Ar);
	
private:
	void ReinitializeMeshComponent(USkeletalMeshComponent* InNewSkeletalMeshComponent, USkeletalMeshComponent* InTargetMeshComponent);
	void RefreshMeshComponent(USkeletalMeshComponent* InTargetMeshComponent);

	// this is source mesh references, so that we could compare and see if it has changed
	TWeakObjectPtr<const USkeletalMeshComponent> CurrentlyUsedSourceMeshComponent;
	TWeakObjectPtr<const USkeletalMesh> CurrentlyUsedSourceMesh;

	// target mesh 
	TWeakObjectPtr<const USkeletalMesh> CurrentlyUsedTargetMesh;

	// Transient data.
	TMap<FName, float> SourceCurveValues;
};

template<> struct TStructOpsTypeTraits<FAnimNode_RemapCurvesFromMesh> : public TStructOpsTypeTraitsBase2<FAnimNode_RemapCurvesFromMesh>
{
	enum 
	{ 
		WithSerializer = true
	};
};
