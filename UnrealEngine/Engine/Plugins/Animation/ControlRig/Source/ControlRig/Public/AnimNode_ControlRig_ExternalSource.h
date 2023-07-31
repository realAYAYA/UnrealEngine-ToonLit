// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_ControlRigBase.h"
#include "AnimNode_ControlRig_ExternalSource.generated.h"

/**
 * Animation node that allows animation ControlRig output to be used in an animation graph
 */
USTRUCT()
struct CONTROLRIG_API FAnimNode_ControlRig_ExternalSource : public FAnimNode_ControlRigBase
{
	GENERATED_BODY()

	FAnimNode_ControlRig_ExternalSource();

	void SetControlRig(UControlRig* InControlRig);
	virtual UControlRig* GetControlRig() const override;
	virtual TSubclassOf<UControlRig> GetControlRigClass() const override;

private:
	UPROPERTY(transient)
	TWeakObjectPtr<UControlRig> ControlRig;
};

