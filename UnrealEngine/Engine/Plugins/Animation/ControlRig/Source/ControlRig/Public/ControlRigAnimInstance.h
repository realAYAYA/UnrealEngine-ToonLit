// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ControlRigAnimInstance.generated.h"

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct CONTROLRIG_API FControlRigAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

public:
	FControlRigAnimInstanceProxy()
	{
	}

	FControlRigAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual ~FControlRigAnimInstanceProxy() override;

	// FAnimInstanceProxy interface
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;

	TMap<int32, FTransform> StoredTransforms;
	TMap<SmartName::UID_Type, float> StoredCurves;
};

UCLASS(transient, NotBlueprintable)
class CONTROLRIG_API UControlRigAnimInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

		FControlRigAnimInstanceProxy* GetControlRigProxyOnGameThread() { return &GetProxyOnGameThread <FControlRigAnimInstanceProxy>(); }

protected:

	// UAnimInstance interface
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};