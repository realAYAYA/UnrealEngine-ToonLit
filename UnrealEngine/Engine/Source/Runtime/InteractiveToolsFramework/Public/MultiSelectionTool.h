// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveTool.h"
#include "ComponentSourceInterfaces.h"
#include "ToolTargets/ToolTarget.h"
#include "InteractiveToolQueryInterfaces.h"

#include "MultiSelectionTool.generated.h"

UCLASS(Transient)
class INTERACTIVETOOLSFRAMEWORK_API UMultiSelectionTool : public UInteractiveTool, public IInteractiveToolCameraFocusAPI
{
GENERATED_BODY()
public:
	void SetTargets(TArray<TObjectPtr<UToolTarget>> TargetsIn)
	{
		Targets = MoveTemp(TargetsIn);
	}

	/**
	 * @return true if all ComponentTargets of this tool are still valid
	 */
	virtual bool AreAllTargetsValid() const
	{
		for (const TObjectPtr<UToolTarget>& Target : Targets)
		{
			if (Target->IsValid() == false)
			{
				return false;
			}
		}
		return true;
	}


public:
	virtual bool CanAccept() const override
	{
		return AreAllTargetsValid();
	}

protected:
	UPROPERTY()
	TArray<TObjectPtr<UToolTarget>> Targets{};

public:
	// IInteractiveToolCameraFocusAPI implementation
	virtual bool SupportsWorldSpaceFocusBox() override;
	virtual FBox GetWorldSpaceFocusBox() override;
	virtual bool SupportsWorldSpaceFocusPoint() override;
	virtual bool GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut) override;
};


