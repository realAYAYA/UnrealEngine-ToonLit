// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveTool.h"
#include "ComponentSourceInterfaces.h"
#include "ToolTargets/ToolTarget.h"
#include "InteractiveToolQueryInterfaces.h"

#include "SingleSelectionTool.generated.h"

UCLASS(Transient)
class INTERACTIVETOOLSFRAMEWORK_API USingleSelectionTool : public UInteractiveTool, public IInteractiveToolCameraFocusAPI
{
GENERATED_BODY()
public:
	void SetTarget(UToolTarget* TargetIn)
	{
		Target = TargetIn;
	}

	/**
	 * @return true if all ToolTargets of this tool are still valid
	 */
	virtual bool AreAllTargetsValid() const
	{
		return Target ? Target->IsValid() : false;
	}


public:
	virtual bool CanAccept() const override
	{
		return AreAllTargetsValid();
	}

protected:
	UPROPERTY()
	TObjectPtr<UToolTarget> Target;



public:
	// IInteractiveToolCameraFocusAPI implementation
	virtual bool SupportsWorldSpaceFocusBox() override;
	virtual FBox GetWorldSpaceFocusBox() override;
	virtual bool SupportsWorldSpaceFocusPoint() override;
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;

};
