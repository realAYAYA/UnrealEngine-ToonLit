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

UCLASS(MinimalAPI)
class UNiagaraStackEmitterPropertiesGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	NIAGARAEDITOR_API UNiagaraStackEmitterPropertiesGroup();

	NIAGARAEDITOR_API void Initialize(FRequiredEntryData InRequiredEntryData);

	virtual EIconMode GetSupportedIconMode() const override { return EIconMode::Brush; }
	NIAGARAEDITOR_API virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool SupportsSecondaryIcon() const override;
	NIAGARAEDITOR_API virtual const FSlateBrush* GetSecondaryIconBrush() const override;
	virtual bool GetCanExpandInOverview() const override { return false; }
	virtual bool GetShouldShowInStack() const override { return false; }

protected:
	NIAGARAEDITOR_API void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	NIAGARAEDITOR_API void ItemAddedFromUtilities(FGuid AddedEventHandlerId, UNiagaraSimulationStageBase* AddedSimulationStage);

private:
	UPROPERTY()
	TObjectPtr<UNiagaraStackEmitterPropertiesItem> PropertiesItem;

	TSharedPtr<INiagaraStackItemGroupAddUtilities> AddUtilities;
};
