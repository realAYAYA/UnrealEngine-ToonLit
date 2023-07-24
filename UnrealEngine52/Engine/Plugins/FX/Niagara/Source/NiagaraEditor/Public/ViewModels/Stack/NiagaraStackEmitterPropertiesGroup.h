// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackScriptItemGroup.h"
#include "NiagaraStackEmitterPropertiesGroup.generated.h"

class FNiagaraEmitterViewModel;
class FNiagaraScriptViewModel;
class INiagaraStackItemGroupAddUtilities;
class UNiagaraSimulationStageBase;
class UNiagaraStackEmitterPropertiesItem;
class UNiagaraStackObject;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackEmitterPropertiesGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	UNiagaraStackEmitterPropertiesGroup();

	void Initialize(FRequiredEntryData InRequiredEntryData);

	virtual EIconMode GetSupportedIconMode() const override { return EIconMode::Brush; }
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool SupportsSecondaryIcon() const override { return true; }
	virtual const FSlateBrush* GetSecondaryIconBrush() const override;
	virtual bool GetCanExpandInOverview() const override { return false; }
	virtual bool GetShouldShowInStack() const override { return false; }

protected:
	void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	void ItemAddedFromUtilities(FGuid AddedEventHandlerId, UNiagaraSimulationStageBase* AddedSimulationStage);

private:
	UPROPERTY()
	TObjectPtr<UNiagaraStackEmitterPropertiesItem> PropertiesItem;

	TSharedPtr<INiagaraStackItemGroupAddUtilities> AddUtilities;
};