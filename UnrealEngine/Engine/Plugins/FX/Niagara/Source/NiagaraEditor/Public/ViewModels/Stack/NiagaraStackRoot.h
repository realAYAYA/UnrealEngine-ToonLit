// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackCommentCollection.h"
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

UCLASS(MinimalAPI)
class UNiagaraStackRoot : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	NIAGARAEDITOR_API UNiagaraStackRoot();
	
	NIAGARAEDITOR_API void Initialize(FRequiredEntryData InRequiredEntryData, bool bInIncludeSystemInformation, bool bInIncludeEmitterInformation);
	NIAGARAEDITOR_API virtual void FinalizeInternal() override;

	NIAGARAEDITOR_API virtual bool GetCanExpand() const override;
	NIAGARAEDITOR_API virtual bool GetShouldShowInStack() const override;
	UNiagaraStackRenderItemGroup* GetRenderGroup() const
	{
		return RenderGroup;
	}

	UNiagaraStackCommentCollection* GetCommentCollection() const
	{
		return CommentCollection;
	}
	
protected:
	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	NIAGARAEDITOR_API void EmitterArraysChanged();
	NIAGARAEDITOR_API void OnSummaryViewStateChanged();

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
	TObjectPtr<UNiagaraStackCommentCollection> CommentCollection;
	
	UPROPERTY()
	TObjectPtr<UNiagaraStackSummaryViewCollapseButton> SummaryCollapseButton;

	bool bIncludeSystemInformation;
	bool bIncludeEmitterInformation;
};
