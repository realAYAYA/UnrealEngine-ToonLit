// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraStackRoot.generated.h"

class FNiagaraEmitterViewModel;
class UNiagaraStackSystemPropertiesGroup;
class UNiagaraStackSystemUserParametersGroup;
class UNiagaraStackEmitterPropertiesGroup;
class UNiagaraStackScriptItemGroup;
class UNiagaraStackRenderItemGroup;
class UNiagaraStackEmitterSummaryGroup;
class UNiagaraStackSummaryViewCollapseButton;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackRoot : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackRoot();
	
	void Initialize(FRequiredEntryData InRequiredEntryData, bool bInIncludeSystemInformation, bool bInIncludeEmitterInformation);
	virtual void FinalizeInternal() override;

	virtual bool GetCanExpand() const override;
	virtual bool GetShouldShowInStack() const override;
	UNiagaraStackRenderItemGroup* GetRenderGroup() const
	{
		return RenderGroup;
	}
protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	void EmitterArraysChanged();
	void OnSummaryViewStateChanged();

private:
	UPROPERTY()
	TObjectPtr<UNiagaraStackSystemPropertiesGroup> SystemPropertiesGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackSystemUserParametersGroup> SystemUserParametersGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> SystemSpawnGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> SystemUpdateGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackEmitterPropertiesGroup> EmitterPropertiesGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackEmitterSummaryGroup> EmitterSummaryGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> EmitterSpawnGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> EmitterUpdateGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> ParticleSpawnGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> ParticleUpdateGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackRenderItemGroup> RenderGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackSummaryViewCollapseButton> SummaryCollapseButton;

	bool bIncludeSystemInformation;
	bool bIncludeEmitterInformation;
};
