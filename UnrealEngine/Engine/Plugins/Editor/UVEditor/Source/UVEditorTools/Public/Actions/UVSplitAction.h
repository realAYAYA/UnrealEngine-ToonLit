// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Actions/UVToolAction.h"
#include "CoreMinimal.h"

#include "UVSplitAction.generated.h"

UCLASS()
class UVEDITORTOOLS_API UUVSplitAction : public UUVToolAction
{	
	GENERATED_BODY()

public:
	virtual bool CanExecuteAction() const override;
	virtual bool ExecuteAction() override;
};