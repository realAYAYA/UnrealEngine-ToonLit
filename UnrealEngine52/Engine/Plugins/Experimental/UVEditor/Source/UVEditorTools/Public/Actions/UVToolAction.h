// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UVToolAction.generated.h"

class UUVToolEmitChangeAPI;
class UUVToolSelectionAPI;
class UInteractiveToolManager;

UCLASS()
class UVEDITORTOOLS_API UUVToolAction : public UObject
{
	GENERATED_BODY()
public:

	virtual void Setup(UInteractiveToolManager* ToolManager);
	virtual void Shutdown();
	virtual bool CanExecuteAction() const { return false;}
	virtual bool ExecuteAction() { return false; }

protected:
	UPROPERTY()
	TObjectPtr<UUVToolSelectionAPI> SelectionAPI = nullptr;

	UPROPERTY()
	TObjectPtr<UUVToolEmitChangeAPI> EmitChangeAPI = nullptr;

	TWeakObjectPtr<UInteractiveToolManager> ToolManager;

	virtual UInteractiveToolManager* GetToolManager() const { return ToolManager.Get(); }
};
