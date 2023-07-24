// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimPreviewInstance.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "AnimNode_CorrectPose.h"
#include "AnimNode_CorrectivesSource.h"

#include "PoseCorrectivesAnimInstanceProxy.generated.h"

struct FAnimNode_ControlRig_CorrectivesSource;

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct FPoseCorrectivesAnimInstanceProxy : public FAnimPreviewInstanceProxy
{
	GENERATED_BODY()

public:
	
	FPoseCorrectivesAnimInstanceProxy() = default;
	FPoseCorrectivesAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_CorrectPose* CorrectPoseNode, FAnimNode_ControlRig_CorrectivesSource* ControlRigSourceNode);
	virtual ~FPoseCorrectivesAnimInstanceProxy() override = default;

	/** FAnimInstanceProxy interface */
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	
	/** Called when the anim instance is being initialized. These nodes can be provided */
	virtual FAnimNode_Base* GetCustomRootNode() override;
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override;	
	/** END FAnimInstanceProxy interface */

	void SetCorrectivesAssetAndSourceComponent(
		UPoseCorrectivesAsset* InPoseCorrectivesAsset,
		TWeakObjectPtr<USkeletalMeshComponent> InSourceMeshComponent);

	void SetUseControlRigInput(bool bUseControlRigInput);
	bool IsUsingControlRigInput() const;

	void SetUseCorrectiveSource(const FName& CorrectiveName);
	void StopUseCorrectiveSource();

private:
	FAnimNode_CorrectPose* CorrectPoseNode;
	FAnimNode_ControlRig_CorrectivesSource* ControlRigSourceNode;
	FAnimNode_CopyPoseFromMesh CopySourceNode;
	FAnimNode_CorrectivesSource CorrectivesSourceNode;

	bool bUseControlRigInput = false;
};

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct FPoseCorrectivesAnimSourceInstanceProxy : public FAnimPreviewInstanceProxy
{
	GENERATED_BODY()

public:
	
	FPoseCorrectivesAnimSourceInstanceProxy() = default;
	FPoseCorrectivesAnimSourceInstanceProxy(UAnimInstance* InAnimInstance);
	virtual ~FPoseCorrectivesAnimSourceInstanceProxy() override = default;

	/** FAnimInstanceProxy interface */
	virtual bool Evaluate(FPoseContext& Output) override;

	virtual FAnimNode_Base* GetCustomRootNode() override;
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override;	


	void SetCorrectivesAsset(UPoseCorrectivesAsset* InPoseCorrectivesAsset);
	void SetUseCorrectiveSource(const FName& CorrectiveName);
	void StopUseCorrectiveSource();
	
private:
	FAnimNode_CorrectivesSource CorrectivesSourceNode;
};