// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimPreviewInstance.h"
#include "AnimNode_CorrectPose.h"
#include "AnimNode_ControlRig_CorrectivesSource.h"

#include "PoseCorrectivesAnimInstance.generated.h"

UCLASS(transient, NotBlueprintable)
class UPoseCorrectivesAnimInstance : public UAnimPreviewInstance
{
	GENERATED_UCLASS_BODY()

public:

	void SetCorrectivesAssetAndSourceComponent(
		UPoseCorrectivesAsset* InPoseCorrectivesAsset,
		TWeakObjectPtr<USkeletalMeshComponent> InSourceMeshComponent);

	void SetSourceControlRig(UControlRig* InControlRig);

	void SetUseControlRigInput(bool bUseControlRigInput);
	bool IsUsingControlRigInput() const;

	void SetUseCorrectiveSource(const FName& CorrectiveName);
	void StopUseCorrectiveSource();

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	UPROPERTY(Transient)
	FAnimNode_CorrectPose PoseCorrectivesNode;

	UPROPERTY(Transient)
	FAnimNode_ControlRig_CorrectivesSource ControlRigSourceNode;
};

UCLASS(transient, NotBlueprintable)
class UPoseCorrectivesAnimSourceInstance : public UAnimPreviewInstance
{
	GENERATED_UCLASS_BODY()

public:

	void SetCorrectivesAsset(UPoseCorrectivesAsset* InPoseCorrectivesAsset);

	void SetUseCorrectiveSource(const FName& CorrectiveName);
	void StopUseCorrectiveSource();

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};
